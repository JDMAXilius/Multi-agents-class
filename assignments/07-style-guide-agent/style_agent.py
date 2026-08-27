#!/usr/bin/env python3
"""Assignment #7 — the Style Guide Agent for BREACHPOINT.

    Generator -> Evaluator -> Refiner -> (loop) -> Circuit Breaker

Run it:

    python3 style_agent.py            # replay the committed run — stdlib, no key
    python3 style_agent.py --live     # real model calls, re-records
    python3 style_agent.py --rules    # print the style guide's rules, no model
    python3 style_agent.py --selftest # prove every rule fires, deterministically

## What it generates

**Arena intel cards** — the loading-screen / map-screen blurb for each of the
seven landmarks the shipped arena actually has (`arena_manifest.json`). The
arena has callouts and geometry but zero player-facing prose; this pipeline
writes it, and the Evaluator holds it to `STYLE-GUIDE.md`.

## What the Evaluator enforces

The committed STYLE GUIDE — six rule families, each extracted from a real
project source (the GDD, the arena manifest, the shipped text tables), not
invented for the assignment:

    1 CANON_PLACES    seven landmarks exist; an invented proper noun fails
    2 CANON_ARSENAL   three weapons exist; "sniper perch" fails
    3 CUT_SYSTEMS     radar / plasma / vehicles / flags are named cuts
    4 CANON_NUMBERS   every digit must be a real tuning value (45 s rocket
                      respawn is a hallucination; it is 90 — GDD Appendix A)
    5 VOICE           zero "!" and zero first person, measured across all
                      63 shipped spotter lines and every UI LOCTEXT
    6 FORMAT          <= 2 sentences, <= 28 words (the guide's own slot spec)
    + NO_FICTION      the narrative doctrine: the arena is a place, not a
                      story — years and backstory vocabulary fail

This is deliberately not generic validation. "Built by the Vanguard
Corporation to guard the reactor core" is grammatical, evocative, correctly
spelled — and wrong for THIS game, for reasons the style guide cites.

## Why the loop needs a circuit breaker

A refiner asked to strip the evocative thing out of a card will sometimes
swap one violation for another (trade a faction for a fake number), or
sand the card down to nothing. After MAX_ATTEMPTS the breaker trips, the
slot escalates to the human lead with its full history, and the pipeline
keeps going for the other six landmarks.
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
    """The live game file, or the copy a submitted zip carries in game/."""
    return repo_rel if repo_rel.exists() else HERE / "game" / mirror_name


MANIFEST = _game_file(GAME / "Content" / "Data" / "arena_manifest.json",
                      "arena_manifest.json")
GDD = _game_file(GAME / "BREACHPOINT-GDD-VERTICAL-SLICE.md",
                 "BREACHPOINT-GDD-VERTICAL-SLICE.md")
SHIPPED_LINES = _game_file(GAME / "Content" / "Data" / "DT_SpotterLines.csv",
                           "DT_SpotterLines.csv")
OUT = HERE / "output"
RECORDING = HERE / "recording.json"
MODEL = "claude-sonnet-5"

MAX_ATTEMPTS = 3   # generate once, then at most 2 refinements per card

RUN_LOG: list[str] = []


def log(msg: str = ""):
    print(msg, flush=True)
    RUN_LOG.append(msg)


# ===========================================================================
# EVALUATOR — the style guide, executable. Sources in STYLE-GUIDE.md.
# ===========================================================================

@dataclass
class Violation:
    rule: str
    detail: str
    citation: str

    def __str__(self):
        return f"[{self.rule}] {self.detail} ({self.citation})"


# Rule 1 — the complete capitalized vocabulary of BREACHPOINT. Landmark words
# come from arena_manifest.json's seven `landmarks[].name` entries; the rest
# is the sandbox (GDD §2.2, §2.4, §2.5) and ordinary copy mechanics.
CANON_CAPITALIZED = {
    # the seven landmarks, word by word
    "the", "core", "mezzanine", "catwalks", "gantry",
    "south", "north", "barricade", "west", "east", "stack",
    # the sandbox
    "rocket", "launcher", "assault", "rifle", "magnum", "grappleshot",
    "frag", "frags", "grenade", "grenades", "melee", "shields", "shield",
    "grapple", "breachpoint", "team", "slayer",
    # level vocabulary the GDD itself uses (§2.6)
    "lower", "mid", "upper",
}

# Rule 2 — weapon classes the slice does not ship (GDD §2.4: three weapons).
ARSENAL_BANNED = {
    r"\bsniper\b": "GDD §2.4 — the arsenal is AR/Magnum/Rocket; no sniper exists",
    r"\bshotgun\b": "GDD §2.4 — no shotgun exists",
    r"\bsmg\b": "GDD §2.4 — no SMG exists",
    r"\bsword\b": "GDD §2.4 — no sword exists",
    r"\bneedler\b": "GDD §2.4 — no needler exists",
    r"\blaser\b": "GDD §2.4 — no laser exists",
    r"\bturret\b": "GDD §2.4 — no turret exists",
}

# Rule 3 — §6's cut table and §5.1's shipped scope, same sources as
# Assignment #6 because the canon is the same canon.
CUT_SYSTEMS = {
    r"\bradar\b": "GDD §2.7 — the slice ships no radar, by name",
    r"\bmotion tracker\b": "GDD §6 — motion tracker is CUT",
    r"\bplasma\b": "GDD §6 — Plasma Rifle is CUT",
    r"\bvehicle\b|\bwarthog\b": "GDD §6 — vehicles are not planned",
    r"\bflag\b|\bctf\b": "GDD §5.1 — Team Slayer is the only shipped mode",
}

# Rule 4 — every number BREACHPOINT's copy may cite: GDD Appendix A tuning,
# §§1.2/2.1–2.6 match rules, and the manifest's measured geometry (32 m
# corridors, 14 m runs, 13 m anchor approaches, grapple_note).
CANON_NUMBERS = {2, 3, 4, 5, 8, 13, 14, 20, 22, 25, 32, 35, 60, 70, 90,
                 100, 120, 600, 0.4, 2.5}
CANON_NUMBER_LITERALS = ("8:00",)   # the match clock, GDD §1.2

# NO_FICTION — the narrative doctrine's sharp edges. Measured fact: the GDD
# contains zero fiction — no faction, year, war, or planet — so backstory
# vocabulary in BREACHPOINT copy is an invention by definition.
FICTION_WORDS = {
    "war", "wars", "ancient", "alien", "aliens", "corporation", "colony",
    "colonial", "empire", "imperial", "ruins", "god", "gods", "sacred",
    "temple", "civilization", "century", "centuries", "decades",
    "forgotten", "abandoned", "legend", "legends", "prophecy", "haunted",
}
FICTION_PHRASES = ("built by", "long ago", "once stood", "in memory of")
YEAR_RX = re.compile(r"\b(1[0-9]{3}|2[0-9]{3})\b")

# Rule 5 — measured: 63/63 shipped spotter lines and every committed LOCTEXT
# carry zero exclamation marks and zero first person.
FIRST_PERSON = {"i", "we", "our", "my", "me"}

MAX_WORDS = 28
MAX_SENTENCES = 2

STYLE_GUIDE = "STYLE-GUIDE.md"


def _sentences(text: str) -> list[str]:
    return [s.strip() for s in re.split(r"[.!?]+", text) if s.strip()]


def _bare(word: str) -> str:
    m = re.match(r"[A-Za-z][A-Za-z']*", word)
    return m.group(0) if m else ""


def evaluate(text: str) -> list[Violation]:
    """Every rule here is a section of STYLE-GUIDE.md, and every section of
    STYLE-GUIDE.md cites the project file it was extracted from."""
    v: list[Violation] = []
    low = text.lower()
    words = text.split()

    # --- FORMAT (rule 6) --------------------------------------------------
    if not text.strip():
        v.append(Violation("FORMAT", "empty card", STYLE_GUIDE + " rule 6"))
        return v
    if len(words) > MAX_WORDS:
        v.append(Violation("FORMAT", f"{len(words)} words, cap is {MAX_WORDS}",
                           STYLE_GUIDE + " rule 6"))
    if len(_sentences(text)) > MAX_SENTENCES:
        v.append(Violation("FORMAT", f"{len(_sentences(text))} sentences, cap is "
                           f"{MAX_SENTENCES}", STYLE_GUIDE + " rule 6"))

    # --- CANON_PLACES (rule 1) — capitalized words mid-sentence ----------
    # A capitalized token that does not open its sentence and is not canon
    # vocabulary reads as an invented proper noun. (A lore noun that OPENS a
    # sentence slips this check — documented limit; NO_FICTION is the second
    # net, exactly as Assignment #6 documented its name-detector's misses.)
    for s in _sentences(text):
        for tok in s.split()[1:]:
            # The first live run flagged "Core's" — the possessive of a canon
            # landmark is canon, so the lookup uses the stem before any
            # apostrophe.
            bare = _bare(tok).split("'")[0]
            if bare and bare[0].isupper() and bare.lower() not in CANON_CAPITALIZED:
                v.append(Violation("CANON_PLACES",
                                   f"{bare!r} is not BREACHPOINT vocabulary — the "
                                   f"arena has seven landmarks and no fiction",
                                   "arena_manifest.json landmarks[]"))

    # --- CANON_ARSENAL (rule 2) ------------------------------------------
    for pattern, why in ARSENAL_BANNED.items():
        m = re.search(pattern, low)
        if m:
            v.append(Violation("CANON_ARSENAL", f"names {m.group(0)!r}", why))

    # --- CUT_SYSTEMS (rule 3) ---------------------------------------------
    for pattern, why in CUT_SYSTEMS.items():
        m = re.search(pattern, low)
        if m:
            v.append(Violation("CUT_SYSTEMS", f"names {m.group(0)!r}", why))

    # --- CANON_NUMBERS (rule 4) -------------------------------------------
    scrubbed = text
    for lit in CANON_NUMBER_LITERALS:
        scrubbed = scrubbed.replace(lit, "")
    for num in re.findall(r"\d+(?:\.\d+)?", scrubbed):
        value = float(num)
        if value not in CANON_NUMBERS:
            v.append(Violation("CANON_NUMBERS",
                               f"{num!r} is not a canon tuning value — a wrong "
                               f"number is worse than none",
                               "GDD Appendix A + arena_manifest.json"))

    # --- NO_FICTION (the doctrine) ----------------------------------------
    if YEAR_RX.search(scrubbed):
        v.append(Violation("NO_FICTION", f"a year in {text!r} — BREACHPOINT has "
                           f"no dates", STYLE_GUIDE + " — the GDD contains zero fiction"))
    for w in words:
        if _bare(w).lower() in FICTION_WORDS:
            v.append(Violation("NO_FICTION", f"backstory word {_bare(w).lower()!r} — "
                               f"the arena is a place, not a story",
                               STYLE_GUIDE + " — the GDD contains zero fiction"))
    for phrase in FICTION_PHRASES:
        if phrase in low:
            v.append(Violation("NO_FICTION", f"backstory phrase {phrase!r}",
                               STYLE_GUIDE + " — the GDD contains zero fiction"))

    # --- VOICE (rule 5) ----------------------------------------------------
    if "!" in text:
        v.append(Violation("VOICE", "exclamation mark — zero of the 63 shipped "
                           "lines and zero UI strings carry one",
                           "DT_SpotterLines.csv + UI LOCTEXTs, measured"))
    for w in words:
        if _bare(w).lower() in FIRST_PERSON:
            v.append(Violation("VOICE", f"first person {_bare(w).lower()!r} — no "
                               f"shipped BREACHPOINT string uses it",
                               "DT_SpotterLines.csv + UI LOCTEXTs, measured"))
    return v


def describe_rules() -> str:
    return f"""THE STYLE GUIDE, EXECUTABLE — sources in STYLE-GUIDE.md, verbatim citations

1 CANON_PLACES   (arena_manifest.json)  seven landmarks — The Core, Mezzanine
                 Catwalks, The Gantry, South/North Barricade, West/East Stack —
                 and no other proper noun. Invented places and factions fail.
2 CANON_ARSENAL  (GDD §2.4)  three weapons + triangle + Grappleshot. No sniper,
                 shotgun, SMG, sword, needler, laser, turret.
3 CUT_SYSTEMS    (GDD §6, §2.7, §5.1)  radar, motion tracker, plasma, vehicles,
                 flag modes are named cuts and may not be mentioned.
4 CANON_NUMBERS  (GDD Appendix A + manifest)  every digit must be a real tuning
                 value: {sorted(x for x in CANON_NUMBERS if x == int(x))} + 0.4, 2.5, "8:00".
5 VOICE          (measured: 63 spotter lines + all UI LOCTEXTs)  zero "!", zero
                 first person. Terse, present, certain.
6 FORMAT         (STYLE-GUIDE.md's own slot spec, honestly labeled)  <= {MAX_SENTENCES}
                 sentences, <= {MAX_WORDS} words.
+ NO_FICTION     (measured: the GDD contains zero fiction)  no years, no
                 backstory vocabulary — tactical present tense only.
"""


# ===========================================================================
# Self-test — every rule demonstrably fires, no model in the room
# ===========================================================================

SELFTEST = [
    ("Built by the Vanguard Corporation in 2552 to guard the reactor.",
     {"CANON_PLACES", "NO_FICTION", "CANON_NUMBERS"}),
    ("A sniper's perch above the arena.", {"CANON_ARSENAL"}),
    ("Watch your radar near the plasma vents.", {"CUT_SYSTEMS"}),
    ("The rocket returns every 45 seconds.", {"CANON_NUMBERS"}),
    ("We hold this line!", {"VOICE"}),
    ("", {"FORMAT"}),
    ("One. Two. Three sentences is a paragraph.", {"FORMAT"}),
    ("Rocket pad on the roof of The Core. Contest it every 90 seconds.", set()),
    # the first live run's false positive, pinned fixed: a canon landmark's
    # possessive is canon — but an unknown noun's possessive still fails
    ("Hold the Core's roof.", set()),
    ("Guard the Vanguard's gate.", {"CANON_PLACES"}),
]


class _IncorrigibleEngine:
    """A scripted engine whose refiner never complies — the breaker's proof.

    The committed live run happens to land 7/7 (the model fixed everything the
    evaluator caught within the attempt budget), so the circuit breaker cannot
    be demonstrated from the recording alone. This drives the REAL run_slot
    loop — same breaker, same escalation record — deterministically.
    """

    def call(self, prompt: str, meta: dict) -> dict:
        return {"card": "A sniper's perch built by the Vanguard in 2552."}


def selftest() -> bool:
    ok = True
    for text, want in SELFTEST:
        got = {x.rule for x in evaluate(text)}
        verdict = "ok " if got == want else "FAIL"
        if got != want:
            ok = False
        log(f"  {verdict}  {text!r:60} -> {sorted(got) or 'clean'}"
            + ("" if got == want else f"   (wanted {sorted(want)})"))

    log("\ncircuit-breaker proof — a refiner that never complies must escalate "
        f"after exactly {MAX_ATTEMPTS} attempts:")
    res = run_slot(_IncorrigibleEngine(), "The Gantry", "(scripted)", "(scripted)")
    tripped = (len(res.escalated) == 1 and res.accepted is None
               and len(res.history) == MAX_ATTEMPTS)
    log(f"  {'ok ' if tripped else 'FAIL'}  escalated={len(res.escalated)} "
        f"accepted={res.accepted!r} attempts={len(res.history)}")
    return ok and tripped


# ===========================================================================
# Engine — live or replay (the Assignment #6 engine, same discipline:
# save after EVERY call; one self-correction on malformed JSON)
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
                sys.exit("error: --live needs ANTHROPIC_API_KEY + `pip install "
                         "anthropic`, or the `claude` CLI on PATH")
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

    def save(self):
        if self.live:
            RECORDING.write_text(json.dumps({"model": MODEL,
                                             "exchanges": self.exchanges},
                                            indent=2), encoding="utf-8")


def extract_json(text: str):
    """First complete JSON value in the reply (raw_decode — a chatty reply or
    a second trailing object costs nothing; Assignment #6 died on both)."""
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
#
# The generator is DELIBERATELY given less than the whole style guide: it gets
# the voice and the no-fiction doctrine (what a copywriter would be told) and
# the landmark's raw manifest prose (what a copywriter would be handed). The
# manifest prose is full of engineering numbers and the brief asks for
# atmosphere — that tension is the temptation, and catching what it produces
# is the Evaluator's demonstration. Feeding the generator every rule up front
# would only prove the rules can be pasted into a prompt.

VOICE_BLOCK = """## THE HOUSE VOICE — real shipped BREACHPOINT lines
{exemplars}

Measured across everything the game ships: no exclamation marks, no first
person, terse and certain. Tactical present tense — what a position does for
the player NOW. BREACHPOINT has no story: no factions, no dates, no lore."""

GENERATE_PROMPT = """You write player-facing copy for BREACHPOINT, a 4v4 team arena FPS
(shields over health, Assault Rifle / Magnum / Rocket Launcher, Grappleshot).

{voice}

## THE JOB
An intel card for the map screen: an evocative, useful blurb for one landmark
of the arena. Players read it in the seconds before a match.

## THE LANDMARK — `{name}`
The level designer's notes, verbatim from the arena manifest:
{purpose}

Write the card: at most 2 sentences. Return JSON only:
{{"card": "..."}}
"""

REFINE_PROMPT = """You write player-facing copy for BREACHPOINT, a 4v4 team arena FPS.

{voice}

## THE LANDMARK — `{name}`
{purpose}

## A CARD YOU WROTE FAILED THE STYLE GUIDE
Card: {card!r}

Violations:
{violations}

Rewrite ONLY this card so it passes, keeping it evocative and useful — a card
sanded down to "a place on the map" passes every rule and says nothing.
At most 2 sentences. Return JSON only: {{"card": "..."}}
"""


def exemplars(n: int = 8) -> str:
    if not SHIPPED_LINES.exists():
        return "(shipped table unavailable)"
    rows = list(csv.DictReader(SHIPPED_LINES.open(encoding="utf-8")))
    picked = rows[:3] + rows[30:33] + rows[-2:]
    return "\n".join(f"  {r['TriggerId']:26} {r['Text']!r}" for r in picked[:n])


# ===========================================================================
# The loop
# ===========================================================================

@dataclass
class Attempt:
    n: int
    stage: str                 # generate | refine
    card: str
    violations: list[str] = field(default_factory=list)


@dataclass
class SlotResult:
    landmark: str
    tempts: str
    accepted: str | None = None
    history: list[Attempt] = field(default_factory=list)
    escalated: list[dict] = field(default_factory=list)


# Which landmarks TEMPT which rule — declared before the run, so the run can
# be judged against it (the Assignment #6 idiom). The Core's manifest prose
# is a wall of coordinates and its fantasy gravity is "reactor lore"; the
# Gantry is a high walkway, which begs for a sniper that does not exist.
TEMPTATIONS = {
    "The Core": "TEMPTS reactor lore + coordinate echoes from the manifest prose",
    "The Gantry": "TEMPTS a sniper perch + engineering heights (z[7.6,8])",
    "Mezzanine Catwalks": "TEMPTS coordinate echoes (the prose is v2-vs-v3 engineering)",
}


def run_slot(engine: Engine, name: str, purpose: str, voice: str) -> SlotResult:
    tempts = TEMPTATIONS.get(name, "safe")
    res = SlotResult(landmark=name, tempts=tempts)
    log(f"\n=== {name}")
    if tempts != "safe":
        log(f"    (this slot {tempts})")

    try:
        gen = engine.call(GENERATE_PROMPT.format(voice=voice, name=name,
                                                 purpose=purpose),
                          {"agent": "generator", "slot": name, "attempt": 1})
    except ParseFailure as e:
        log(f"  BREAKER    generator returned unparseable output twice — escalating")
        res.escalated.append({"final_card": None, "attempts": 1,
                              "unresolved": [f"generator output unparseable: {e}"],
                              "history": [],
                              "why": "the slot produced nothing to evaluate; a human "
                                     "decides whether to re-prompt or leave the card off"})
        return res

    attempt, current = 1, str(gen["card"]).strip()
    log(f"  GENERATED  {current!r}")
    while True:
        violations = evaluate(current)
        res.history.append(Attempt(attempt, "generate" if attempt == 1 else "refine",
                                   current, [str(x) for x in violations]))
        if not violations:
            res.accepted = current
            if attempt > 1:
                log(f"  REFINED    {current!r}  (passed on attempt {attempt})")
            else:
                log(f"  ACCEPTED   first pass")
            break

        log(f"  REJECTED   {current!r}")
        for viol in violations:
            log(f"             {viol}")

        # ---- CIRCUIT BREAKER ----
        if attempt >= MAX_ATTEMPTS:
            log(f"  BREAKER    tripped after {attempt} attempts — escalating to "
                f"the human lead")
            res.escalated.append({
                "final_card": current,
                "attempts": attempt,
                "unresolved": [str(x) for x in violations],
                "history": [asdict(a) for a in res.history],
                "why": "the refiner could not satisfy the style guide without "
                       "losing the card; a human decides what this landmark says",
            })
            break

        # ---- REFINER ----
        attempt += 1
        try:
            ref = engine.call(
                REFINE_PROMPT.format(voice=voice, name=name, purpose=purpose,
                                     card=current,
                                     violations="\n".join(f"  - {x}" for x in violations)),
                {"agent": "refiner", "slot": name, "attempt": attempt})
        except ParseFailure as e:
            log(f"  BREAKER    refiner returned unparseable output twice — escalating")
            res.escalated.append({
                "final_card": current, "attempts": attempt,
                "unresolved": [str(x) for x in violations] + [f"refiner unparseable: {e}"],
                "history": [asdict(a) for a in res.history],
                "why": "the refiner stopped returning usable output; the card is "
                       "left as it last stood, unaccepted"})
            break
        current = str(ref["card"]).strip()
    return res


def rows_to_csv(results: list[SlotResult]) -> str:
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=["RowName", "Landmark", "Card"],
                       lineterminator="\n")
    w.writeheader()
    for i, r in enumerate(results):
        if r.accepted:
            w.writerow({"RowName": f"IC{i:02}", "Landmark": r.landmark,
                        "Card": r.accepted})
    return buf.getvalue()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--live", action="store_true", help="make real model calls")
    ap.add_argument("--rules", action="store_true", help="print the rules and exit")
    ap.add_argument("--selftest", action="store_true",
                    help="prove every rule fires — deterministic, no model")
    args = ap.parse_args()

    if args.rules:
        log(describe_rules())
        return
    if args.selftest:
        log("style guide self-test — every rule must fire on its planted case:")
        sys.exit(0 if selftest() else 1)

    if not MANIFEST.exists():
        sys.exit(f"error: the arena manifest is not at {MANIFEST} — this pipeline "
                 f"writes copy for BREACHPOINT's real landmarks and has no slots "
                 f"without it")
    OUT.mkdir(exist_ok=True)
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    landmarks = [(lm["name"], lm.get("purpose", "")) for lm in manifest["landmarks"]]
    voice = VOICE_BLOCK.format(exemplars=exemplars())
    engine = Engine(args.live)
    log(f"Style Guide Agent · BREACHPOINT · {'LIVE' if args.live else 'replay'} · "
        f"max {MAX_ATTEMPTS} attempts per card · {len(landmarks)} landmarks")
    log(f"enforcing: STYLE-GUIDE.md — 6 sourced rule families + the no-fiction doctrine")

    results = [run_slot(engine, name, purpose, voice) for name, purpose in landmarks]
    engine.save()

    accepted = sum(1 for r in results if r.accepted)
    escalated = sum(len(r.escalated) for r in results)
    refined = sum(1 for r in results for a in r.history
                  if a.stage == "refine" and not a.violations)
    rejections = sum(1 for r in results for a in r.history if a.violations)

    (OUT / "ArenaIntelCards.csv").write_text(rows_to_csv(results), encoding="utf-8")
    (OUT / "run_report.json").write_text(json.dumps(
        {"accepted": accepted, "escalated": escalated,
         "rejections": rejections, "refined_to_pass": refined,
         "max_attempts": MAX_ATTEMPTS,
         "slots": [asdict(r) for r in results]}, indent=2), encoding="utf-8")
    (OUT / "rules.txt").write_text(describe_rules(), encoding="utf-8")

    log(f"\n--- summary ---")
    log(f"  accepted             {accepted}/{len(landmarks)}")
    log(f"  rejected by rule     {rejections}")
    log(f"  fixed by the refiner {refined}")
    log(f"  escalated by breaker {escalated}")
    for r in results:
        for e in r.escalated:
            log(f"    ESCALATED {r.landmark}: {e['final_card']!r} — {e['unresolved']}")
    log(f"\nwrote output/ArenaIntelCards.csv · output/run_report.json · output/rules.txt")
    (OUT / "run_log.txt").write_text("\n".join(RUN_LOG) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
