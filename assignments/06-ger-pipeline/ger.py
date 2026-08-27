#!/usr/bin/env python3
"""Assignment #6 — a GER pipeline for BREACHPOINT.

    Generator -> Evaluator -> Refiner -> (loop) -> Circuit Breaker

Run it:

    python3 ger.py            # replay the committed run — stdlib only, no API key
    python3 ger.py --live     # real model calls, re-records
    python3 ger.py --rules    # print the evaluator's rules and exit, no model call

## What it generates

Canned announcer lines for **team events**. BREACHPOINT became a team game this
week and `DT_SpotterLines` has 23 triggers, none of them about a teammate — a
teammate dying is silent. See `PRE-BUILD-DECLARATION.txt`.

## The rule the Evaluator enforces

GDD §3.3, lines 293-299: the canned table is *"shipped in the build"* and
*"No connectivity ⇒ the game is identical minus flavor."*

A line that needs live data cannot satisfy that. It cannot ship in the build,
because the build does not know it; and offline it renders empty or stale. So
**every line must stand alone** — no player name, no score, no count, nothing
the canned path cannot know. The same section caps an event line at 18 words.

This is deliberately not a generic validity check. "Reyes is down" is perfectly
valid text, correctly spelled, in the right register, the right length — and
wrong for this game, for a reason the GDD states.

## Why the loop needs a circuit breaker

A refiner that is asked to remove the specific thing making a line good will
sometimes produce another line with the same defect, or drift the voice while
fixing the rule. Retrying forever burns tokens and lands nothing. After
MAX_ATTEMPTS the breaker trips, the slot is escalated to the human lead with its
whole history, and the pipeline keeps going for the other slots.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
GAME = REPO / "breachpoint"


def _game_file(repo_rel: Path, mirror_name: str) -> Path:
    """The live game file, or the copy a zip carries.

    The pipeline reads two things from BREACHPOINT: the GDD it enforces a rule
    from, and the shipped line table it takes the house voice from. Both live in
    the game tree, which a submitted zip does not have — so `make_submission.sh`
    mirrors them into `game/` and this falls back to them. The rule is still the
    real GDD's rule either way.
    """
    return repo_rel if repo_rel.exists() else HERE / "game" / mirror_name


GDD = _game_file(GAME / "BREACHPOINT-GDD-VERTICAL-SLICE.md",
                 "BREACHPOINT-GDD-VERTICAL-SLICE.md")
SHIPPED = _game_file(GAME / "Content" / "Data" / "DT_SpotterLines.csv",
                     "DT_SpotterLines.csv")
OUT = HERE / "output"
RECORDING = HERE / "recording.json"
MODEL = "claude-sonnet-5"

MAX_ATTEMPTS = 3          # generate once, then at most 2 refinements per slot
CANDIDATES_PER_SLOT = 3   # the shipped table gives every trigger 3 variants


def log(msg: str = ""):
    print(msg, flush=True)


# ===========================================================================
# The slots — team events the game now raises and the announcer cannot speak to
# ===========================================================================
#
# Two of these are deliberately chosen because they TEMPT the rule. A line about
# the last teammate alive wants to say how many are left; a line about a
# teammate avenging you wants to name them. If the evaluator only ever saw safe
# slots it would never demonstrate anything.

SLOTS = [
    ("Team.Mate.Down", "a teammate was killed", "safe"),
    ("Team.Mate.Avenged", "a teammate killed the enemy who just killed you",
     "TEMPTS a player name"),
    ("Team.LastAlive", "you are the last living member of your team",
     "TEMPTS a count of the living"),
    ("Team.Wipe.Enemy", "your team killed all four enemies at once", "safe"),
    ("Team.Regroup", "your team is scattered and should regroup", "safe"),
]


# ===========================================================================
# EVALUATOR — deterministic, and the rules come from the GDD
# ===========================================================================

@dataclass
class Violation:
    rule: str
    detail: str
    citation: str

    def __str__(self):
        return f"[{self.rule}] {self.detail} ({self.citation})"


# Words that look like a person but are BREACHPOINT's own vocabulary. Without
# this the proper-noun check fires on the sentence's first word every time.
CANON_NOUNS = {
    "team", "teammate", "teammates", "enemy", "enemies", "squad", "hostile",
    "hostiles", "shields", "shield", "health", "rocket", "magnum", "grenade",
    "melee", "grappleshot", "spree", "match", "core", "gantry", "mezzanine",
    "catwalks", "barricade", "stack", "north", "south", "east", "west",
    "sudden", "death", "warmup", "slayer", "breachpoint",
}

NUMBER_WORDS = {"one", "two", "three", "four", "five", "six", "seven", "eight"}

# §6's cut list — a line may not reference a system the slice does not ship.
CUT_SYSTEMS = {
    r"\bradar\b": "§2.7 — the slice ships no radar",
    r"\bmotion tracker\b": "§6 — motion tracker is CUT",
    r"\bplasma\b": "§6 — Plasma Rifle is CUT",
    r"\bvehicle\b": "§6 — vehicles are not planned",
    r"\bflag\b|\bctf\b": "§5.1 — Team Slayer is the only shipped mode",
}

GDD_33 = "GDD §3.3"
MAX_WORDS = 18            # GDD §3.3: "per event {spotter_line (≤ 18 words) | null}"

# Naming a person has a grammatical SHAPE, and that is what this looks for.
#
# Two approaches were tried and rejected first. Flagging any capitalised word
# not at position 0 misses the commonest form of all — "Reyes is down." puts the
# name first. Flagging any capitalised word whose lowercase form is absent from
# the GDD's vocabulary flags "Teammate", "Squad" and "Regroup", which are
# legitimate announcer words that simply never appear in a design document.
#
# What survives both is the pattern: a subject that takes a verb, a thing after
# "by", or a possessive — each exempted when the word is BREACHPOINT's own
# vocabulary. "Teammate down." is silent here; "Reyes is down." is not.
NAME_PATTERNS = [
    (re.compile(r"^([A-Z][a-z]+)\s+(?:is|was|has|had|got|went|just|died|fell|"
                r"needs|took|holds|went)\b"), "subject of a verb"),
    (re.compile(r"\bby\s+([A-Z][a-z]+)\b"), "agent after 'by'"),
    (re.compile(r"\b([A-Z][a-z]+)'s\b"), "possessive"),
    (re.compile(r"(?<![.!?])\s([A-Z][a-z]+)\b(?!\s*[.!?]$)"), "capitalised mid-sentence"),
]


def detect_person_names(text: str) -> list[str]:
    found = []
    for rx, _why in NAME_PATTERNS:
        for m in rx.finditer(text):
            word = m.group(1)
            if word.lower() not in CANON_NOUNS and word not in found:
                found.append(word)
    return found


def normalise(text: str) -> str:
    return re.sub(r"[^a-z ]", "", text.lower()).strip()


def evaluate(text: str, accepted: tuple = ()) -> list[Violation]:
    """Every rule here is traceable to a line of the GDD. Rule 1 is the one the
    assignment asks for: specific to this game, not a validity check.

    `accepted` is what has already passed for THIS slot, and rule 5 needs it.
    """
    v: list[Violation] = []
    words = text.split()

    # --- RULE 5 — DISTINCT ------------------------------------------------
    # The shipped table gives every trigger three variants for one reason: a
    # player who earns the same event twice should not hear the identical clip.
    # This rule exists because the first live run shipped a table where it was
    # violated — the refiner escaped a count violation on "All four down." by
    # writing "Team wipe.", which was already accepted for that slot. Three
    # variants, two distinct. A refiner's cheapest escape from any rule is to
    # collapse onto the safest line it has already seen, so the loop has to
    # forbid that explicitly or it silently trades variety for compliance.
    if normalise(text) in {normalise(a) for a in accepted}:
        v.append(Violation("DISTINCT", f"{text!r} duplicates a variant already accepted "
                           f"for this trigger", "DT_SpotterLines.csv — 3 variants/trigger"))

    # --- RULE 1 — STANDS_ALONE. The headline rule. ------------------------
    # GDD §3.3: the canned table ships in the build and plays with no
    # connectivity. Anything the offline path cannot know makes the line
    # unshippable, however well written it is.
    if re.search(r"[{}<>\[\]]", text):
        v.append(Violation("STANDS_ALONE", f"substitution placeholder in {text!r} — "
                           f"the canned path has nothing to substitute", GDD_33))
    if re.search(r"\d", text):
        v.append(Violation("STANDS_ALONE", f"digit in {text!r} — a canned line cannot "
                           f"know a score or a count", GDD_33))
    for w in words:
        bare = w.strip(".,!?'\"").lower()
        if bare in NUMBER_WORDS:
            v.append(Violation("STANDS_ALONE", f"spelled-out count {bare!r} — the canned "
                               f"path cannot know how many", GDD_33))
    for name in detect_person_names(text):
        v.append(Violation("STANDS_ALONE", f"{name!r} reads as a player name — the "
                           f"canned path has no roster", GDD_33))

    # --- RULE 2 — LENGTH -------------------------------------------------
    if len(words) > MAX_WORDS:
        v.append(Violation("LENGTH", f"{len(words)} words, cap is {MAX_WORDS}", GDD_33))

    # --- RULE 3 — CANON --------------------------------------------------
    low = text.lower()
    for pattern, why in CUT_SYSTEMS.items():
        m = re.search(pattern, low)
        if m:
            v.append(Violation("CANON", f"names {m.group(0)!r}", why))

    # --- RULE 4 — HOUSE VOICE --------------------------------------------
    # Measured, not asserted: zero of the 63 shipped lines carry an
    # exclamation mark, and the longest is 6 words.
    if "!" in text:
        v.append(Violation("VOICE", "exclamation mark — zero of the 63 shipped lines "
                           "have one", "DT_SpotterLines.csv"))
    if not text.strip():
        v.append(Violation("SCHEMA", "empty line", "DT_SpotterLines.csv"))
    return v


def describe_rules() -> str:
    return f"""EVALUATOR RULES — every one traceable to the GDD or the shipped table

1. STANDS_ALONE   ({GDD_33})  ** the rule this pipeline exists for **
   The canned table is "shipped in the build" and "No connectivity => the game
   is identical minus flavor". A line needing live data cannot satisfy that.
   Rejected: {{placeholders}}, digits, spelled-out counts, and proper nouns that
   read as player names.

2. LENGTH         ({GDD_33})
   "per event {{spotter_line (<= {MAX_WORDS} words) | null}}"

3. CANON          (GDD §6 cut list, §5.1 shipped scope)
   No radar, motion tracker, plasma, vehicles or flag modes — the slice cuts
   all of them, so an announcer cannot mention them.

4. VOICE          (measured from DT_SpotterLines.csv)
   No exclamation marks: zero of the 63 shipped lines carry one.
"""


# ===========================================================================
# Engine — live or replay
# ===========================================================================

class ParseFailure(Exception):
    """The model returned something that is not JSON, twice."""


class Engine:
    def __init__(self, live: bool):
        self.live, self.exchanges, self.i = live, [], 0
        if not live:
            if not RECORDING.exists():
                sys.exit("error: no recording.json — run once with --live first")
            self.exchanges = json.loads(RECORDING.read_text(encoding="utf-8"))["exchanges"]

    def call(self, prompt: str, meta: dict) -> dict:
        """One logical call, with a single self-correction on malformed output.

        A live run died here first time round: the generator returned
        `{"lines": ["Revenge kill.", "Payback.", "Avenged."}` — a missing
        bracket, genuinely invalid, not a parsing-strategy problem. Crashing on
        it loses the whole run. Feeding the parse error back and asking again
        recovers it; failing twice is a `ParseFailure`, which the slot loop
        escalates through the circuit breaker like any other unrecoverable
        failure. "The loop can't self-correct" covers transport, not just rules.
        """
        if not self.live:
            if self.i >= len(self.exchanges):
                sys.exit(f"error: replay exhausted at {meta}")
            ex = self.exchanges[self.i]
            self.i += 1
            got = (ex["agent"], ex["slot"], ex["attempt"])
            want = (meta["agent"], meta["slot"], meta["attempt"])
            if got != want:
                sys.exit(f"error: replay mismatch — recorded {got}, asked for {want}")
            return ex["response"]

        err = ""
        for tries in range(2):
            ask = prompt if tries == 0 else (
                f"{prompt}\n\n## YOUR PREVIOUS REPLY WAS NOT VALID JSON\n{err}\n"
                f"Return ONLY the JSON object, nothing else.")
            text, usage = self._raw(ask, meta)
            try:
                data = extract_json(text)
            except ValueError as e:
                err = str(e)
                log(f"             (malformed reply, retrying: {err[:70]})")
                continue
            self.exchanges.append({**meta, "prompt": ask, "response": data,
                                   "usage": usage, "parse_retries": tries})
            self.save()
            return data
        raise ParseFailure(err)

    def _raw(self, prompt: str, meta: dict) -> tuple[str, dict]:
        text, usage = None, {}
        if os.environ.get("ANTHROPIC_API_KEY"):
            try:
                import anthropic
                r = anthropic.Anthropic().messages.create(
                    model=MODEL, max_tokens=4000,
                    messages=[{"role": "user", "content": prompt}])
                text = "".join(b.text for b in r.content if b.type == "text")
            except ImportError:
                pass
        if text is None:
            from shutil import which
            if not which("claude"):
                sys.exit("error: --live needs ANTHROPIC_API_KEY + `pip install anthropic`, "
                         "or the `claude` CLI on PATH")
            # A nested `claude` inherits the parent's BUN_OPTIONS and dies on a
            # bare ENOENT. Strip it.
            env = {k: val for k, val in os.environ.items() if k != "BUN_OPTIONS"}
            p = subprocess.run(["claude", "-p", prompt, "--model", MODEL,
                                "--output-format", "json"],
                               capture_output=True, text=True, timeout=900, env=env)
            if p.returncode != 0:
                sys.exit(f"error: claude CLI failed at {meta}: {p.stderr.strip()[:300]}")
            payload = json.loads(p.stdout)
            text = (payload.get("result") or "").strip()
            u = payload.get("usage") or {}
            usage = {"input_tokens": u.get("input_tokens", 0),
                     "output_tokens": u.get("output_tokens", 0),
                     "cost_usd": payload.get("total_cost_usd")}
        return text, usage

    # Saved after EVERY call, not at the end. The first live run crashed at slot
    # three and took two slots of paid calls with it, because the recording was
    # only written once the whole run succeeded.
    def save(self):
        if self.live:
            RECORDING.write_text(json.dumps({"model": MODEL, "exchanges": self.exchanges},
                                            indent=2), encoding="utf-8")


def extract_json(text: str):
    """Take the FIRST complete JSON value in the reply.

    The obvious implementation — first '{' to last '}' — died on this run's
    refiner, which returned two objects back to back. Spanning them produces
    "Extra data" and takes the whole pipeline down at slot three. `raw_decode`
    stops at the end of the first valid value, so a chatty reply costs nothing.
    """
    text = text.strip()
    fence = re.search(r"```(?:json)?\s*(.+?)```", text, re.S)
    if fence:
        text = fence.group(1).strip()
    decoder = json.JSONDecoder()
    for i, ch in enumerate(text):
        if ch in "{[":
            try:
                return decoder.raw_decode(text[i:])[0]
            except json.JSONDecodeError:
                continue
    raise ValueError(f"no parseable JSON in reply (first 200: {text[:200]!r})")


# ===========================================================================
# GENERATOR and REFINER prompts
# ===========================================================================

VOICE_BLOCK = """## THE HOUSE VOICE — these are real rows from the shipped table
{exemplars}

Measured across all 63 shipped lines: 1 to 6 words, median 2, zero exclamation
marks. Match that."""

GENERATE_PROMPT = """You write announcer lines for BREACHPOINT, a 4v4 team arena FPS.

{voice}

## THE EVENT
`{trigger}` — {brief}

## THE CONSTRAINT THAT MATTERS
These lines ship inside the build and play when the game has no connectivity.
The build does not know who is playing or what the score is, so a line cannot
refer to anything only a live match would know.

Write {n} short variants. Return JSON only:
{{"lines": ["...", "...", "..."]}}
"""

REFINE_PROMPT = """You write announcer lines for BREACHPOINT, a 4v4 team arena FPS.

{voice}

## THE EVENT
`{trigger}` — {brief}

## A LINE YOU WROTE FAILED REVIEW
Line: {line!r}

Violations:
{violations}

Rewrite ONLY this line so it passes, keeping the same event and the same
register. Do not solve the problem by writing something vague — "something
happened" passes the rules and says nothing.

Return JSON only: {{"line": "..."}}
"""


def exemplars(n: int = 8) -> str:
    if not SHIPPED.exists():
        return "(shipped table unavailable)"
    rows = list(csv.DictReader(SHIPPED.open(encoding="utf-8")))
    picked = rows[:3] + rows[30:33] + rows[-2:]
    return "\n".join(f"  {r['TriggerId']:26} {r['Text']!r}" for r in picked[:n])


# ===========================================================================
# The loop
# ===========================================================================

@dataclass
class Attempt:
    n: int
    stage: str                 # generate | refine
    line: str
    violations: list[str] = field(default_factory=list)


@dataclass
class SlotResult:
    trigger: str
    brief: str
    tempts: str
    accepted: list[str] = field(default_factory=list)
    history: list[Attempt] = field(default_factory=list)
    escalated: list[dict] = field(default_factory=list)


def run_slot(engine: Engine, trigger: str, brief: str, tempts: str,
             voice: str) -> SlotResult:
    res = SlotResult(trigger=trigger, brief=brief, tempts=tempts)
    log(f"\n=== {trigger} — {brief}")
    if tempts != "safe":
        log(f"    (this slot {tempts})")

    # ---- GENERATOR ----
    try:
        gen = engine.call(GENERATE_PROMPT.format(voice=voice, trigger=trigger, brief=brief,
                                                 n=CANDIDATES_PER_SLOT),
                          {"agent": "generator", "slot": trigger, "attempt": 0})
    except ParseFailure as e:
        log(f"  BREAKER    generator returned unparseable output twice — escalating")
        res.escalated.append({"final_line": None, "attempts": 2,
                              "unresolved": [f"generator output unparseable: {e}"],
                              "history": [],
                              "why": "the slot produced nothing to evaluate; a human "
                                     "decides whether to re-prompt or drop the event"})
        return res
    lines = [str(x).strip() for x in gen["lines"]][:CANDIDATES_PER_SLOT]
    log(f"  GENERATED  {lines}")

    for line in lines:
        attempt, current = 1, line
        while True:
            violations = evaluate(current, tuple(res.accepted))
            res.history.append(Attempt(attempt, "generate" if attempt == 1 else "refine",
                                       current, [str(x) for x in violations]))
            if not violations:
                res.accepted.append(current)
                if attempt > 1:
                    log(f"  REFINED    {current!r}  (passed on attempt {attempt})")
                break

            log(f"  REJECTED   {current!r}")
            for viol in violations:
                log(f"             {viol}")

            # ---- CIRCUIT BREAKER ----
            if attempt >= MAX_ATTEMPTS:
                log(f"  BREAKER    tripped after {attempt} attempts — escalating to the "
                    f"human lead")
                res.escalated.append({
                    "final_line": current,
                    "attempts": attempt,
                    "unresolved": [str(x) for x in violations],
                    "history": [asdict(a) for a in res.history if a.line],
                    "why": "the refiner could not satisfy the rule without losing the "
                           "event; a human decides whether the event needs a line at all",
                })
                break

            # ---- REFINER ----
            attempt += 1
            try:
                ref = engine.call(
                    REFINE_PROMPT.format(
                        voice=voice, trigger=trigger, brief=brief, line=current,
                        violations="\n".join(f"  - {v}" for v in violations)),
                    {"agent": "refiner", "slot": trigger, "attempt": attempt})
            except ParseFailure as e:
                log(f"  BREAKER    refiner returned unparseable output twice — escalating")
                res.escalated.append({
                    "final_line": current, "attempts": attempt,
                    "unresolved": [str(x) for x in violations] + [f"refiner unparseable: {e}"],
                    "history": [asdict(a) for a in res.history if a.line],
                    "why": "the refiner stopped returning usable output; the line is "
                           "left as it last stood, unaccepted"})
                break
            current = str(ref["line"]).strip()
    return res


def rows_to_csv(results: list[SlotResult]) -> str:
    buf = io.StringIO()
    cols = ["RowName", "TriggerId", "Text", "Audience", "Weight", "RepeatCooldown_s"]
    w = csv.DictWriter(buf, fieldnames=cols, lineterminator="\n")
    w.writeheader()
    n = 26   # the shipped table ends at S25 after Assignment #4's additions
    for r in results:
        for i, line in enumerate(r.accepted):
            w.writerow({"RowName": f"S{n}{'abcdefgh'[i]}", "TriggerId": r.trigger,
                        "Text": line, "Audience": "Team", "Weight": "1.0",
                        "RepeatCooldown_s": "20"})
        n += 1
    return buf.getvalue()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--live", action="store_true", help="make real model calls")
    ap.add_argument("--rules", action="store_true", help="print the rules and exit")
    args = ap.parse_args()

    if args.rules:
        log(describe_rules())
        return

    if not GDD.exists():
        sys.exit(f"error: the game's GDD is not at {GDD} — this pipeline targets "
                 f"BREACHPOINT and has nothing to enforce without it")
    OUT.mkdir(exist_ok=True)
    voice = VOICE_BLOCK.format(exemplars=exemplars())
    engine = Engine(args.live)
    log(f"GER pipeline · BREACHPOINT · {'LIVE' if args.live else 'replay'} · "
        f"max {MAX_ATTEMPTS} attempts per line")
    log(f"rule under enforcement: STANDS_ALONE ({GDD_33}) + length, canon, voice")

    results = [run_slot(engine, t, b, tempt, voice) for t, b, tempt in SLOTS]
    engine.save()

    accepted = sum(len(r.accepted) for r in results)
    escalated = sum(len(r.escalated) for r in results)
    refined = sum(1 for r in results for a in r.history
                  if a.stage == "refine" and not a.violations)
    rejections = sum(1 for r in results for a in r.history if a.violations)

    (OUT / "DT_SpotterLines_TeamEvents.csv").write_text(rows_to_csv(results),
                                                        encoding="utf-8")
    (OUT / "run_report.json").write_text(json.dumps(
        {"accepted": accepted, "escalated": escalated,
         "rejections": rejections, "refined_to_pass": refined,
         "max_attempts": MAX_ATTEMPTS,
         "slots": [asdict(r) for r in results]}, indent=2), encoding="utf-8")
    (OUT / "rules.txt").write_text(describe_rules(), encoding="utf-8")

    log(f"\n--- summary ---")
    log(f"  accepted            {accepted}")
    log(f"  rejected by rule    {rejections}")
    log(f"  fixed by the refiner {refined}")
    log(f"  escalated by breaker {escalated}")
    for r in results:
        for e in r.escalated:
            log(f"    ESCALATED {r.trigger}: {e['final_line']!r} — {e['unresolved']}")
    log(f"\nwrote output/DT_SpotterLines_TeamEvents.csv · output/run_report.json")


if __name__ == "__main__":
    main()
