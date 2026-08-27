#!/usr/bin/env python3
"""Assignment #8 — a virtual Dungeon Master with a JSON facts ledger.

    player action -> EXTRACTOR (action -> ledger deltas)
                  -> GUARDS    (deterministic state machine; illegal deltas bounce)
                  -> LEDGER    (applied, versioned, printed every turn)
                  -> NARRATOR  (ledger -> narration + the facts it used)

Run it:

    python3 dm.py                     # replay BOTH committed sessions — no API key
    python3 dm.py --session loyal     # one branch only
    python3 dm.py --live              # re-run the scripted sessions for real
    python3 dm.py --live --play       # free-form interactive session (you type)

## The design decision everything hangs on

**The ledger is the only memory.** The narrator is never shown the transcript —
it receives the world rules, the CURRENT ledger, the player's action, and the
previous turn's narration for prose continuity. Nothing else. If the story
stays consistent across eight turns, the ledger must be doing the work,
because there is nothing else to remember with. Consistency is therefore not a
property we hope the model has; it is a property the architecture forces —
and if the ledger were deleted mid-session, the engine would fail loudly
rather than improvise.

## Why two recorded sessions

The rubric's hardest criterion to *prove* is that dialogue reacts to state,
"not just the most recent input". So the committed artifact is a twin: a LOYAL
session and a BETRAYAL session that share their opening turns, diverge once,
and then send the IDENTICAL player input at the probe turn. Same input, same
world, same prompt — different ledger. The responses differ, and the narrator
cites which ledger facts drove each one. Recency cannot explain that.
"""

from __future__ import annotations

import argparse
import copy
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "output"
MODEL = "claude-sonnet-5"


def log(msg: str = ""):
    print(msg, flush=True)


# ===========================================================================
# THE WORLD — small on purpose: three rooms, two NPCs, one moral fork
# ===========================================================================

WORLD = """# THE EMBER VAULT

A sealed mountain vault holds the Ember — a fist-sized stone that burns
without fuel. The village of Harrow's Rest will not last the winter without
it. You have climbed to the vault with SERA, the village lamplighter, who
trusts you. Your lantern burns lamp oil; when the oil is gone, the dark of
the vault is lethal.

## PLACES (movement only along these connections)
- gatehouse  <-> stair_of_hooks
- stair_of_hooks <-> vault_floor

## PEOPLE
- SERA — lamplighter of Harrow's Rest. Came to carry the Ember home. Loyal,
  practical, keeps count of promises out loud.
- MOSS — a scavenger who works these ruins. He ALWAYS intercepts travellers
  on the stair_of_hooks and offers silver for the Ember. He is charming,
  patient, and keeps his word to the letter and no further.

## THINGS
- vault_key (unique, in the gatehouse desk) · ember (unique, on vault_floor,
  inside the vault) · moss_silver (only if Moss pays)

## HARD RULES OF THIS WORLD
- The vault door opens only with the vault_key, only from vault_floor.
- The Ember cannot be taken before the vault is opened.
- Dead is dead. Broken trust does not mend in a day.
- Lamp oil only ever burns down.
"""

LOCATIONS = {"gatehouse", "stair_of_hooks", "vault_floor"}
EDGES = {("gatehouse", "stair_of_hooks"), ("stair_of_hooks", "gatehouse"),
         ("stair_of_hooks", "vault_floor"), ("vault_floor", "stair_of_hooks")}
UNIQUE_ITEMS = {"vault_key", "ember", "moss_silver"}
TRUST_STATES = {"loyal", "wary", "betrayed"}
STICKY_FLAGS = {"vault_opened", "ember_taken", "deal_with_moss"}

START_LEDGER = {
    "turn": 0,
    "player": {"location": "gatehouse", "inventory": ["lantern"],
               "wounds": [], "oil_remaining": 8},
    "npcs": {
        "sera": {"status": "alive", "location": "gatehouse", "trust": "loyal",
                 "knows": ["the village needs the ember"]},
        "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary",
                 "knows": ["travellers must pass the stair"]},
    },
    "flags": {"vault_opened": False, "ember_taken": False, "deal_with_moss": False},
    "promises": [],
}


# ===========================================================================
# GUARDS — the deterministic state machine. The model proposes; this disposes.
# ===========================================================================

class GuardError(Exception):
    pass


def get_path(ledger: dict, path: str):
    """Resolve a dotted path, list indices included.

    The first live run died here: the narrator cited `promises[0].text` — a
    correct reference to a real ledger entry — and this function only walked
    dict keys, so a VALID citation bounced twice and the guard killed the
    session. A validator stricter than the data it validates is a bug in the
    validator. Both `promises[0].text` and `promises.0.text` resolve now.
    """
    node = ledger
    for part in re.sub(r"\[(\d+)\]", r".\1", path).split("."):
        if isinstance(node, dict) and part in node:
            node = node[part]
        elif isinstance(node, list) and part.isdigit() and int(part) < len(node):
            node = node[int(part)]
        else:
            raise GuardError(f"path {path!r} does not exist in the ledger")
    return node


EDITABLE = re.compile(
    r"^(player\.location|player\.inventory|player\.wounds"
    r"|npcs\.(sera|moss)\.(status|location|trust|knows)"
    r"|flags\.(vault_opened|ember_taken|deal_with_moss)"
    r"|promises)$")


def apply_deltas(ledger: dict, deltas: list[dict]) -> dict:
    """Apply the extractor's proposed ops, or raise GuardError naming the law.

    Every rejection message is written for the model that will read it on
    retry: it names the rule, not just the refusal.
    """
    new = copy.deepcopy(ledger)
    for d in deltas:
        op, path = d.get("op"), d.get("path", "")
        value = d.get("value")
        if not EDITABLE.match(path):
            raise GuardError(f"path {path!r} is not editable — oil_remaining and "
                             f"turn belong to the engine, and unknown paths are "
                             f"not silently created")
        cur = get_path(new, path) if op != "add" or "." in path else None

        if path == "player.location":
            if value not in LOCATIONS:
                raise GuardError(f"{value!r} is not a place in this world")
            here = new["player"]["location"]
            if (here, value) not in EDGES and here != value:
                raise GuardError(f"no path from {here} to {value} — movement "
                                 f"follows the map's connections")
            new["player"]["location"] = value

        elif path.endswith(".location") and path.startswith("npcs."):
            if value not in LOCATIONS:
                raise GuardError(f"{value!r} is not a place in this world")
            get_path(new, path.rsplit(".", 1)[0])["location"] = value

        elif path in ("player.inventory", "player.wounds") or path.endswith(".knows"):
            parent, key = path.rsplit(".", 1) if "." in path else ("", path)
            container = get_path(new, parent)[key] if parent else new[key]
            if op == "add":
                if value in container and (key != "wounds"):
                    raise GuardError(f"{value!r} is already in {path}")
                if key == "inventory" and value in UNIQUE_ITEMS:
                    held_elsewhere = any(value in n.get("inventory", [])
                                         for n in new["npcs"].values())
                    if held_elsewhere:
                        raise GuardError(f"{value!r} is unique and someone else holds it")
                container.append(value)
            elif op == "remove":
                if value not in container:
                    raise GuardError(f"cannot remove {value!r} from {path} — "
                                     f"it is not there")
                container.remove(value)
            else:
                raise GuardError(f"{path} takes add/remove, not {op!r}")

        elif path.endswith(".status"):
            npc = get_path(new, path.rsplit(".", 1)[0])
            if npc["status"] == "dead":
                raise GuardError("dead is dead — status cannot change again")
            if value not in ("alive", "wounded", "dead"):
                raise GuardError(f"status {value!r} is not alive/wounded/dead")
            npc["status"] = value

        elif path.endswith(".trust"):
            npc = get_path(new, path.rsplit(".", 1)[0])
            if value not in TRUST_STATES:
                raise GuardError(f"trust {value!r} is not {sorted(TRUST_STATES)}")
            if npc["trust"] == "betrayed" and value != "betrayed":
                raise GuardError("broken trust does not mend in a day — "
                                 "betrayed is sticky in this world")
            npc["trust"] = value

        elif path.startswith("flags."):
            flag = path.split(".", 1)[1]
            if new["flags"][flag] is True and value is not True:
                raise GuardError(f"flag {flag} is sticky — what happened, happened")
            if flag == "ember_taken" and value is True and not new["flags"]["vault_opened"]:
                raise GuardError("the Ember cannot be taken before the vault is opened")
            if flag == "vault_opened" and value is True:
                if "vault_key" not in new["player"]["inventory"]:
                    raise GuardError("the vault opens only with the vault_key in hand")
                if new["player"]["location"] != "vault_floor":
                    raise GuardError("the vault opens only from vault_floor")
            new["flags"][flag] = bool(value)

        elif path == "promises":
            if op != "add" or not isinstance(value, dict) or \
                    not {"to", "text"} <= set(value):
                raise GuardError('promises takes add of {"to": ..., "text": ...}')
            new["promises"].append({"to": value["to"], "text": value["text"],
                                    "kept": value.get("kept")})
        else:
            raise GuardError(f"no guard knows how to apply {op!r} to {path!r}")

    # Engine-owned world rules, applied after every action:
    new["turn"] += 1
    new["player"]["oil_remaining"] -= 1          # oil only ever burns down
    if new["player"]["oil_remaining"] < 0:
        raise GuardError("the lantern is out of oil — the session should have ended")
    return new


# ===========================================================================
# Engine — live or replay (same shape as assignments #3-#7)
# ===========================================================================

class ParseFailure(Exception):
    pass


def extract_json(text: str):
    text = text.strip()
    fence = re.search(r"```(?:json)?\s*(.+?)```", text, re.S)
    if fence:
        text = fence.group(1).strip()
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        pass
    a = text.find("{")
    if a != -1:
        try:
            obj, _ = json.JSONDecoder().raw_decode(text[a:])
            return obj
        except json.JSONDecodeError:
            pass
    raise ParseFailure(f"no parseable JSON (first 160: {text[:160]!r})")


class Engine:
    def __init__(self, live: bool, recording_path: Path):
        self.live, self.path = live, recording_path
        self.exchanges, self.i = [], 0
        if not live:
            if not recording_path.exists():
                sys.exit(f"error: no {recording_path.name} — run once with --live")
            self.exchanges = json.loads(recording_path.read_text(encoding="utf-8"))["exchanges"]

    def call(self, prompt: str, meta: dict) -> dict:
        if not self.live:
            if self.i >= len(self.exchanges):
                sys.exit(f"error: replay exhausted at {meta}")
            ex = self.exchanges[self.i]; self.i += 1
            got = (ex["role"], ex["turn"], ex.get("attempt", 1))
            want = (meta["role"], meta["turn"], meta.get("attempt", 1))
            if got != want:
                sys.exit(f"error: replay mismatch — recorded {got}, asked {want}")
            return ex["response"]

        text = None
        if os.environ.get("ANTHROPIC_API_KEY"):
            try:
                import anthropic
                r = anthropic.Anthropic().messages.create(
                    model=MODEL, max_tokens=2000,
                    messages=[{"role": "user", "content": prompt}])
                text = "".join(b.text for b in r.content if b.type == "text")
            except ImportError:
                pass
        if text is None:
            from shutil import which
            if not which("claude"):
                sys.exit("error: --live needs ANTHROPIC_API_KEY + `pip install "
                         "anthropic`, or the `claude` CLI on PATH")
            env = {k: v for k, v in os.environ.items() if k != "BUN_OPTIONS"}
            p = subprocess.run(["claude", "-p", prompt, "--model", MODEL,
                                "--output-format", "json"],
                               capture_output=True, text=True, timeout=600, env=env)
            if p.returncode != 0:
                sys.exit(f"error: claude CLI failed at {meta}: {p.stderr.strip()[:200]}")
            text = (json.loads(p.stdout).get("result") or "").strip()
        data = extract_json(text)
        self.exchanges.append({**meta, "prompt": prompt, "response": data})
        self.save()                                # after EVERY call (lesson from #6)
        return data

    def call_validated(self, prompt: str, meta: dict, validate) -> dict:
        """One retry with the exact error — the self-correction bridge."""
        attempt, last = 1, None
        while attempt <= 2:
            try:
                data = self.call(prompt if attempt == 1 else
                                 f"{prompt}\n\n## YOUR PREVIOUS REPLY FAILED\n{last}\n"
                                 f"Return corrected JSON only.",
                                 {**meta, "attempt": attempt})
                validate(data)
                return data
            except (ParseFailure, GuardError, KeyError, ValueError, TypeError) as e:
                last = str(e)
                log(f"        bounced (attempt {attempt}): {last[:110]}")
                attempt += 1
        sys.exit(f"error: {meta} failed twice: {last}")

    def save(self):
        if self.live:
            self.path.write_text(json.dumps({"model": MODEL,
                                             "exchanges": self.exchanges},
                                            indent=1), encoding="utf-8")


# ===========================================================================
# The two model roles
# ===========================================================================

EXTRACT_PROMPT = """You are the STATE EXTRACTOR for a text adventure. You do not write
story. You read what the player DID and report what changed as ledger edits.

{world}

## CURRENT LEDGER (authoritative)
{ledger}

## THE PLAYER'S ACTION THIS TURN
{action}

Report only changes that actually follow from this action. Editable paths:
player.location · player.inventory (add/remove) · player.wounds (add) ·
npcs.<name>.status/location/trust · npcs.<name>.knows (add) ·
flags.vault_opened/ember_taken/deal_with_moss · promises (add).
Do NOT touch turn or oil_remaining — the engine owns them. An action that
changes nothing returns an empty list.

JSON only:
{{"deltas": [{{"op": "set|add|remove", "path": "...", "value": ...}}],
  "reason": "<one line>"}}
"""

NARRATE_PROMPT = """You are the DUNGEON MASTER of a text adventure. Write the next beat.

{world}

## THE LEDGER — this is your ONLY memory of the story so far
You are not shown the transcript. Everything you say must be consistent with
this ledger; if the ledger and your instincts disagree, the ledger is right.
{ledger}

## WHAT JUST HAPPENED (for prose continuity only)
Previous beat: {previous}
The player's action this turn: {action}

Write 2-5 sentences of narration and dialogue. NPC dialogue must reflect the
ledger — an NPC whose trust is "betrayed" does not speak like one whose trust
is "loyal"; a dead NPC does not speak at all; promises the ledger records are
remembered by whoever heard them.

JSON only:
{{"narration": "<the beat, dialogue inline>",
  "facts_used": ["npcs.sera.trust", "flags.vault_opened"]}}

`facts_used` entries are BARE ledger paths exactly as above — never
"npcs.sera.trust: loyal", never prose. Paths only.
"""


def validate_deltas(data):
    if not isinstance(data.get("deltas"), list):
        raise ValueError('reply must carry a "deltas" list')


def normalize_fact_path(p: str) -> str:
    """`npcs.sera.trust: betrayed` -> `npcs.sera.trust`.

    Live models cited facts three different ways across three runs — bare
    paths, list indices (`promises[0].text`), and path-colon-value. The first
    two taught the resolver; this one taught the validator to accept the
    annotation and keep the path, because bouncing a citation whose only sin
    is helpfulness wastes a retry on a formatting nit. The value half is
    discarded, never trusted.
    """
    return re.split(r"[:=\s(]", str(p).strip(), 1)[0].strip().rstrip(",;")


def make_narration_validator(ledger: dict):
    def validate(data):
        if not str(data.get("narration", "")).strip():
            raise ValueError("empty narration")
        used = data.get("facts_used")
        if not isinstance(used, list) or not used:
            raise ValueError('"facts_used" must be a non-empty list of ledger paths')
        normalized = [normalize_fact_path(p) for p in used]
        if not all(normalized):
            raise ValueError(f"unusable facts_used entries: {used!r}")
        for p in normalized:
            get_path(ledger, p)            # raises GuardError if it does not resolve
        data["facts_used"] = normalized    # downstream audits see clean paths
        # the ledger, not the model's imagination, decides who can speak
        text = str(data["narration"])
        for name, npc in ledger["npcs"].items():
            if npc["status"] == "dead" and re.search(
                    rf"\b{name}\b[^.]*?(says|whispers|shouts|replies|laughs)",
                    text, re.I):
                raise ValueError(f"{name} is dead in the ledger and cannot speak")
    return validate


# ===========================================================================
# The scripted twin sessions
# ===========================================================================
#
# Turns 1-3 and 5 are IDENTICAL text in both sessions. Turn 4 is the fork.
# Turn 6 is the probe: the same input in both sessions — only the ledger
# differs, so any difference in the response is ledger-driven by construction.

SHARED_OPEN = [
    "I search the gatekeeper's desk in the gatehouse for the vault key.",
    "I tell Sera: 'I promise you — the Ember goes home to Harrow's Rest.' Then we take the stair down.",
    "I hear Moss out — what exactly is he offering for the Ember?",
]
FORK = {
    "loyal":    "I refuse Moss to his face: the Ember is promised to the village. We push on to the vault floor.",
    "betrayal": "I shake Moss's hand in front of Sera and promise him the Ember for his silver. Then we go down to the vault floor.",
}
SHARED_CLOSE = [
    "I unlock the vault with the key and take the Ember.",
    "I hold out the Ember to Sera and ask her to carry it home for the village.",   # THE PROBE
    "I check my pack and my lantern — what exactly am I carrying, and how much oil is left?",
    "I climb back up toward the gatehouse and settle what I owe.",
]

SESSIONS = {name: SHARED_OPEN + [FORK[name]] + SHARED_CLOSE
            for name in ("loyal", "betrayal")}
PROBE_TURN = 6


def run_session(name: str, live: bool) -> dict:
    rec = HERE / f"recording_{name}.json"
    engine = Engine(live, rec)
    ledger = copy.deepcopy(START_LEDGER)
    history, transcript = [copy.deepcopy(ledger)], []
    log(f"\n{'='*74}\nSESSION: {name.upper()}  ({'live' if live else 'replay'})\n{'='*74}")

    previous = "You and Sera stand at the gatehouse door, lantern lit, eight fingers of oil in the reservoir."
    for turn_no, action in enumerate(SESSIONS[name], start=1):
        log(f"\n--- turn {turn_no} ---")
        log(f"  PLAYER   {action}")

        state = {"current": ledger}          # rebindable for the retry closure

        def _validate(data):
            validate_deltas(data)
            state["applied"] = apply_deltas(state["current"], data["deltas"])

        ex = engine.call_validated(
            EXTRACT_PROMPT.format(world=WORLD, ledger=json.dumps(ledger, indent=1),
                                  action=action),
            {"role": "extractor", "turn": turn_no, "session": name}, _validate)
        ledger = state["applied"]
        for d in ex["deltas"]:
            log(f"  LEDGER   {d['op']:6} {d['path']} = {json.dumps(d.get('value'))}")

        nr = engine.call_validated(
            NARRATE_PROMPT.format(world=WORLD, ledger=json.dumps(ledger, indent=1),
                                  previous=previous, action=action),
            {"role": "narrator", "turn": turn_no, "session": name},
            make_narration_validator(ledger))
        previous = nr["narration"]
        log(f"  DM       {nr['narration']}")
        log(f"  FACTS    {', '.join(nr['facts_used'])}")
        log(f"  ledger>  {json.dumps(ledger)}")     # rubric: state visible every turn

        history.append(copy.deepcopy(ledger))
        transcript.append({"turn": turn_no, "action": action,
                           "deltas": ex["deltas"], "narration": nr["narration"],
                           "facts_used": nr["facts_used"],
                           "ledger_after": copy.deepcopy(ledger)})
    engine.save()
    return {"session": name, "turns": transcript, "ledger_history": history}


def interactive(name: str = "play"):
    engine = Engine(True, HERE / f"recording_{name}.json")
    ledger = copy.deepcopy(START_LEDGER)
    previous = "You and Sera stand at the gatehouse door, lantern lit."
    log(WORLD + "\n(type your action; empty line quits)\n")
    turn_no = 0
    while ledger["player"]["oil_remaining"] > 0:
        action = input("> ").strip()
        if not action:
            break
        turn_no += 1
        state = {"current": ledger}

        def _validate(data):
            validate_deltas(data)
            state["applied"] = apply_deltas(state["current"], data["deltas"])
        ex = engine.call_validated(
            EXTRACT_PROMPT.format(world=WORLD, ledger=json.dumps(ledger, indent=1),
                                  action=action),
            {"role": "extractor", "turn": turn_no, "session": name}, _validate)
        ledger = state["applied"]
        nr = engine.call_validated(
            NARRATE_PROMPT.format(world=WORLD, ledger=json.dumps(ledger, indent=1),
                                  previous=previous, action=action),
            {"role": "narrator", "turn": turn_no, "session": name},
            make_narration_validator(ledger))
        previous = nr["narration"]
        log(f"\n{nr['narration']}\n")
        log(f"[ledger] {json.dumps(ledger)}\n")


# ===========================================================================
# Outputs
# ===========================================================================

def write_outputs(results: dict):
    OUT.mkdir(exist_ok=True)
    for name, r in results.items():
        (OUT / f"session_{name}.json").write_text(json.dumps(r, indent=1),
                                                  encoding="utf-8")
    lines = ["# The Ember Vault — twin session transcript", "",
             "Two sessions from the same opening. Turns 1-3 and 5 are identical text;",
             "turn 4 is the fork; **turn 6 sends the identical input in both** — only",
             "the ledger differs. The full ledger is printed after every turn, because",
             "it is the narrator's only memory.", ""]
    for name, r in results.items():
        lines += [f"## {name.upper()} session", ""]
        for t in r["turns"]:
            probe = "  ⟵ THE PROBE (identical input in both sessions)" \
                if t["turn"] == PROBE_TURN else ""
            lines += [f"### turn {t['turn']}{probe}", "",
                      f"**Player:** {t['action']}", "",
                      f"**DM:** {t['narration']}", "",
                      f"*facts used:* `{'`, `'.join(t['facts_used'])}`", "",
                      "```json", json.dumps(t["ledger_after"]), "```", ""]
    (OUT / "transcript.md").write_text("\n".join(lines), encoding="utf-8")

    # The probe, side by side — the Reactive Dialogue evidence, extracted.
    a, b = results["loyal"]["turns"][PROBE_TURN - 1], \
        results["betrayal"]["turns"][PROBE_TURN - 1]
    (OUT / "probe_comparison.json").write_text(json.dumps({
        "identical_input": a["action"],
        "input_matches": a["action"] == b["action"],
        "loyal": {"sera_trust": a["ledger_after"]["npcs"]["sera"]["trust"],
                  "narration": a["narration"], "facts_used": a["facts_used"]},
        "betrayal": {"sera_trust": b["ledger_after"]["npcs"]["sera"]["trust"],
                     "narration": b["narration"], "facts_used": b["facts_used"]},
    }, indent=1), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--live", action="store_true")
    ap.add_argument("--session", choices=["loyal", "betrayal"])
    ap.add_argument("--play", action="store_true",
                    help="free-form interactive session (needs --live)")
    args = ap.parse_args()

    if args.play:
        if not args.live:
            sys.exit("error: --play needs --live (a human cannot be replayed)")
        interactive()
        return

    names = [args.session] if args.session else ["loyal", "betrayal"]
    results = {n: run_session(n, args.live) for n in names}
    if set(results) == {"loyal", "betrayal"}:
        write_outputs(results)
        a = results["loyal"]["turns"][PROBE_TURN - 1]
        b = results["betrayal"]["turns"][PROBE_TURN - 1]
        log(f"\n{'='*74}\nTHE PROBE (turn {PROBE_TURN}) — identical input, different ledger:")
        log(f"  input     {a['action']!r}")
        log(f"  loyal     {a['narration'][:120]}...")
        log(f"  betrayal  {b['narration'][:120]}...")
        log(f"\nwrote output/transcript.md · session_*.json · probe_comparison.json")


if __name__ == "__main__":
    main()
