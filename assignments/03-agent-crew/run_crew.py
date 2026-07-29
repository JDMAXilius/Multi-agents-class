#!/usr/bin/env python3
"""
BREACHPOINT agent crew orchestrator — Assignment #3 (Multi-Agent AI for Game Development).

Raw orchestration (no framework): a deterministic script coordinates four LLM agents
through a gated pipeline to produce game-ready data for BREACHPOINT, a UE 5.8 4v4
arena FPS. The agent definitions are the project's real crew files in
crew/.claude/agents/ — the same definitions used for production, not copies.

Pipeline per job (Class-04 hierarchical pattern; the "manager" is this script):

    producer (curator, read-only)  →  proposes structured records with evidence
      gate A (deterministic)       →  JSON parses, schema fields, sane ranges
    critic (REFUTER, read-only)    →  attacks the design; findings need a
                                      concrete failure scenario, or it passes
      bounce loop                  →  findings go back to the producer (max 2)
    builder (the only author)      →  turns validated records into the
                                      game-ready file format
      gate B (deterministic)       →  artifact parses, header/keys match schema
    verifier (read-only)           →  independently recomputes the math and
                                      proves the artifact; PASS required
    land                           →  only after PASS is the file written

Jobs:
    weapons  → output/DT_Weapons.csv       (producer: tuning-curator)
    arena    → output/arena_manifest.json  (producer: arena-architect)

Modes:
    python3 run_crew.py            replay mode (default): re-drives the recorded
                                   live exchanges through this same pipeline.
                                   Python 3 stdlib only — no key, no install.
    python3 run_crew.py --live     calls Claude for real. Uses the `anthropic`
                                   package + ANTHROPIC_API_KEY if available,
                                   else falls back to the `claude` CLI.
                                   Records every exchange to recording.json.

Exit codes: 0 ok · 2 design refuted after max bounces · 3 verifier failed.
"""

import argparse
import csv
import io
import json
import math
import re
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUTPUT_DIR = HERE / "output"
RECORDING_PATH = HERE / "recording.json"
MAX_BOUNCES = 3

# A REFUTER with an attack surface but no statement of intent will attack the
# design itself, escalating every round and never converging. These are the
# rulings the critic judges against — deliberate trade-offs are not defects.
REVIEW_DOCTRINE = """\
# DESIGN INTENT (rulings already made — attacking these is out of scope)
- Precision weapons SHOULD reward aim: the Magnum's headshot TTK beating every
  other path is the intended fantasy, paid for by an 8-round mag, per-shot
  accuracy pressure, and no forgiveness on a miss. "Skilled Magnum play is
  strong" is the design working, not a finding.
- The AR is the shield-stripper, not a finisher; being slower solo is intended.
- The Magnum is the FINISHER. It is NOT required to kill a full-200-EHP target
  from one magazine on body shots alone — the intended line is AR-strip, swap,
  Magnum finish. "The Magnum cannot solo a full-health target without
  reloading" is the two-weapon carry doing its job, not a defect.
- Geometry claims — mutual visibility, line-of-sight breakage, what a given
  wall occludes — CANNOT be settled from a manifest's coordinates alone. They
  are editor-rung (walkthrough) checks. A manifest that records them in
  doubts[] with that caveat has handled them CORRECTLY. Only flag geometry when
  the manifest's own numbers contradict each other: a coordinate collision, a
  distance past a stated cap, a spawn without any assigned cover entry.
- The Rocket is a map pickup on a 90 s timer with 2 shots — it is SUPPOSED to
  win the fight it is present for; scarcity is the balance, not its damage.
- The arena is one compact map for a vertical slice, not a competitive-ranked
  layout. Imperfect spawn distribution is acceptable if the hard constraints
  hold and the imbalance is documented.

# WHAT COUNTS AS A FINDING
A finding shows a violation of a HARD CONSTRAINT or an internal contradiction:
a number that contradicts another number in the same set, a stated claim the
values disprove, a mag that cannot complete its weapon's stated job, a GDD
constraint breached (35 m sightline, >= 8 spawns, 200 ms bot floor), or a
schema/units error. Restating an intended trade-off is NOT a finding, and
neither is a preference for different tuning.

# CONVERGENCE
Consider the producer's `doubts[]` — a risk they already documented and
accepted is closed, not re-openable. Residual risk that only real playtest data
can settle belongs in doubts[], not in a finding. This review exists to catch
defects before they land, not to design the game."""
DEFAULT_MODEL = "claude-sonnet-5"

# ---------------------------------------------------------------------------
# Canon: the combat model from the BREACHPOINT GDD (Appendix A). Single source
# for every prompt and every deterministic check in this script.
# ---------------------------------------------------------------------------

COMBAT_MODEL = """\
Shields 100 (recharge 60/s beginning 2.5 s after last damage) over Health 100
(never regenerates). Effective HP from full: 200. Headshot multiplier applies
to the whole shot (x2). Weapon swap 0.4 s. Sandbox intent (Halo model): the
Assault Rifle is the shield-stripper, the Magnum is the precision finisher,
the Rocket Launcher is the contested power weapon on a 90 s map timer.
GDD first-pass values (complete and defend them — do not silently change them):
  Assault Rifle: 8 dmg/shot, 600 RPM, 32 mag, hitscan
  Magnum: 22 dmg/shot, x2 headshot, 8 mag, hitscan
  Rocket Launcher: 120 dmg, 4 m splash radius, 2 shots, projectile
"""

# FireMode is trigger cadence; DamageDelivery is how the shot reaches the
# target. They were one column until the verifier proved the hitscan invariant
# was unverifiable against a field that only ever held {Automatic, SemiAuto}.
WEAPON_COLUMNS = [
    "Name", "DisplayName", "FireMode", "DamageDelivery", "DamagePerShot",
    "RPM", "MagSize", "ReserveMags", "ReloadTime_s", "HeadshotMult",
    "ProjectileSpeed", "SplashRadius_m", "SplashDamage", "EquipTime_s",
    "MeshSoftPath", "FireCueTag",
]

ARENA_BRIEF = """\
One compact three-level 4v4 arena (8 players), readable at a glance.
Hard constraints from the GDD: no sightline longer than 35 m; scored respawns
(farthest-from-combat) need >= 8 spawn points, no pair mutually visible, and
every spawn breaks line of sight within 5 m; the Rocket Launcher pad (90 s
timer) is the central contested landmark; every landmark gets a callout name
because callouts are gameplay; Grappleshot (20 m range) must have vertical
routes worth grappling to.
"""

# ---------------------------------------------------------------------------
# Agent definitions: loaded from the real crew files, frontmatter stripped.
# ---------------------------------------------------------------------------

def find_agents_dir() -> Path:
    candidates = [
        HERE / "agents",                                      # zip layout
        HERE.parent.parent / "crew" / ".claude" / "agents",   # repo layout
    ]
    for cand in candidates:
        if (cand / "critic.md").exists():
            return cand
    sys.exit("error: agent definitions not found (looked in ./agents and ../../crew/.claude/agents)")


def load_agent(agents_dir: Path, name: str) -> str:
    for cand in (agents_dir / f"{name}.md", agents_dir / "curators" / f"{name}.md"):
        if cand.exists():
            text = cand.read_text(encoding="utf-8")
            # strip YAML frontmatter
            m = re.match(r"^---\n.*?\n---\n", text, flags=re.DOTALL)
            return text[m.end():].strip() if m else text.strip()
    sys.exit(f"error: agent definition not found: {name}")


# ---------------------------------------------------------------------------
# Engines: how a prompt becomes a response.
# ---------------------------------------------------------------------------

class LiveEngine:
    """Calls Claude for real: anthropic SDK if available, else the claude CLI."""

    def __init__(self, model: str, recording_path: Path = RECORDING_PATH):
        self.model = model
        self.recording_path = recording_path
        self.exchanges = []
        self._api = None
        if os.environ.get("ANTHROPIC_API_KEY"):
            try:
                import anthropic  # optional
                self._api = anthropic.Anthropic()
            except ImportError:
                pass
        if self._api is None and not _which("claude"):
            sys.exit("error: --live needs ANTHROPIC_API_KEY + `pip install anthropic`, "
                     "or the `claude` CLI on PATH")

    def call(self, prompt: str, meta: dict) -> str:
        if self._api is not None:
            resp = self._api.messages.create(
                model=self.model, max_tokens=8000,
                messages=[{"role": "user", "content": prompt}])
            text = "".join(b.text for b in resp.content if b.type == "text")
        else:
            # Strip the parent's Bun tuning: when this script is itself launched
            # from a Claude Code session, the inherited BUN_OPTIONS breaks the
            # nested CLI with a bare ENOENT.
            env = {k: v for k, v in os.environ.items() if k != "BUN_OPTIONS"}
            proc = subprocess.run(
                ["claude", "-p", prompt, "--model", self.model],
                capture_output=True, text=True, timeout=900, env=env)
            if proc.returncode != 0:
                sys.exit(f"error: claude CLI failed at {meta}: {proc.stderr.strip()[:500]}")
            text = proc.stdout.strip()
        self.exchanges.append({**meta, "prompt": prompt, "response": text})
        return text

    def save_recording(self):
        self.recording_path.write_text(json.dumps(
            {"model": self.model, "exchanges": self.exchanges},
            indent=2), encoding="utf-8")


class ReplayEngine:
    """Re-drives the recorded exchanges through the same pipeline code paths."""

    def __init__(self, recording_path: Path = RECORDING_PATH):
        if not recording_path.exists():
            sys.exit("error: no recording.json — run with --live first")
        data = json.loads(recording_path.read_text(encoding="utf-8"))
        self.exchanges = data["exchanges"]
        self.i = 0

    def call(self, prompt: str, meta: dict) -> str:
        if self.i >= len(self.exchanges):
            sys.exit(f"error: replay exhausted at {meta}")
        ex = self.exchanges[self.i]
        self.i += 1
        if (ex["agent"], ex["stage"]) != (meta["agent"], meta["stage"]):
            sys.exit(f"error: replay mismatch — recorded {ex['agent']}/{ex['stage']}, "
                     f"pipeline asked for {meta['agent']}/{meta['stage']}")
        return ex["response"]

    def save_recording(self):
        pass


def _which(cmd):
    from shutil import which
    return which(cmd)


# ---------------------------------------------------------------------------
# Shared plumbing.
# ---------------------------------------------------------------------------

LOG_LINES = []

def log(msg: str):
    print(msg, flush=True)
    LOG_LINES.append(msg)


def extract_json(text: str):
    """Agents are told JSON-only, but tolerate a ```json fence or prose around it."""
    m = re.search(r"```(?:json)?\s*(\{.*?\})\s*```", text, flags=re.DOTALL)
    if m:
        return json.loads(m.group(1))
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        start, end = text.find("{"), text.rfind("}")
        if start == -1 or end <= start:
            raise
        return json.loads(text[start:end + 1])


def write_risk_register(job: str, open_risks: list):
    """Accepted findings are not discarded — they land beside the artifact so
    the human lead inherits them. A risk that vanishes silently is worse than
    one that blocked."""
    path = OUTPUT_DIR / f"open_risks_{job}.json"
    path.write_text(json.dumps(
        {"job": job, "accepted_non_blocking_findings": open_risks,
         "note": "Raised by the critic, judged non-blocking (severity < high). "
                 "Landed with the artifact; the lead resolves or accepts them "
                 "when real playtest data exists."},
        indent=2), encoding="utf-8")
    log(f"  risk register: {len(open_risks)} accepted finding(s) → {path.name}")


def split_findings(review):
    """Blocking vs non-blocking, as in any real review: only `high` stops a
    landing. Medium/low are recorded as accepted risk and carried into the
    artifact — otherwise a reviewer that always finds *something* never lets
    anything ship."""
    if review["verdict"] == "PASS":
        return [], []
    findings = review["findings"]
    blocking = [f for f in findings if f.get("severity") == "high"]
    return blocking, [f for f in findings if f.get("severity") != "high"]


def ask(eng, agent_def: str, agent: str, stage: str, job: str, task: str, validate):
    """One agent call with a single self-correction retry (Class-04 error bridge):
    if the response fails to parse or fails schema validation, the exact error
    is fed back once and the agent corrects itself."""
    prompt = f"# YOUR AGENT DEFINITION\n{agent_def}\n\n{task}"
    for attempt in (1, 2):
        meta = {"job": job, "stage": stage, "agent": agent, "attempt": attempt}
        text = eng.call(prompt, meta)
        try:
            data = extract_json(text)
            err = validate(data)
            if err is None:
                return data
        except (json.JSONDecodeError, KeyError, TypeError, ValueError) as e:
            err = f"response was not valid JSON of the required shape: {e}"
        if attempt == 1:
            log(f"    gate: {agent}/{stage} output rejected ({err}) — one self-correction retry")
            prompt += (f"\n\n# CORRECTION REQUIRED\nYour previous response failed "
                       f"validation: {err}\nReturn ONLY the corrected JSON object.")
    sys.exit(f"error: {agent}/{stage} failed validation twice: {err}")


# ---------------------------------------------------------------------------
# Job 1 — weapons: tuning-curator → critic → builder → verifier → DT_Weapons.csv
# ---------------------------------------------------------------------------

def validate_weapon_proposals(data):
    props = data.get("proposals")
    if not isinstance(props, list) or len(props) != 3:
        return "expected `proposals`: a list of exactly 3 records (AR, Magnum, Rocket)"
    for p in props:
        row = p.get("full_row")
        if not isinstance(row, dict):
            return "each proposal needs `full_row` (dict of all schema columns)"
        missing = [c for c in WEAPON_COLUMNS if c not in row]
        if missing:
            return f"full_row for {row.get('Name', '?')} missing columns: {missing}"
        for col in ("DamagePerShot", "RPM", "MagSize", "HeadshotMult"):
            if not (isinstance(row[col], (int, float)) and row[col] > 0):
                return f"{row.get('Name', '?')}.{col} must be a positive number"
        if row["FireMode"] not in ("Automatic", "SemiAuto"):
            return f"{row.get('Name', '?')}.FireMode must be Automatic or SemiAuto (trigger cadence)"
        if row["DamageDelivery"] not in ("Hitscan", "Projectile"):
            return f"{row.get('Name', '?')}.DamageDelivery must be Hitscan or Projectile"
        # The invariant the verifier proved unverifiable — now enforced here.
        hitscan = row["DamageDelivery"] == "Hitscan"
        if hitscan != (float(row["ProjectileSpeed"]) == 0):
            return (f"{row.get('Name', '?')}: ProjectileSpeed must be 0 iff "
                    f"DamageDelivery is Hitscan (got {row['DamageDelivery']}, "
                    f"speed {row['ProjectileSpeed']})")
        for key in ("evidence", "implied_ttk_s", "risk", "doubts"):
            if key not in p:
                return f"proposal for {row.get('Name', '?')} missing `{key}`"
    return None


def validate_verdict(data):
    if data.get("verdict") not in ("PASS", "FINDINGS"):
        return "expected `verdict`: \"PASS\" or \"FINDINGS\""
    if data["verdict"] == "FINDINGS":
        f = data.get("findings")
        if not isinstance(f, list) or not f:
            return "verdict FINDINGS requires a non-empty `findings` list"
        for item in f:
            if not item.get("failure_scenario"):
                return "every finding needs `failure_scenario` (input -> wrong output); findings without one are opinions"
    return None


def validate_built_csv(data):
    text = data.get("csv_text")
    if not isinstance(text, str):
        return "expected `csv_text`: the full CSV file content as a string"
    rows = list(csv.reader(io.StringIO(text)))
    if not rows or rows[0] != WEAPON_COLUMNS:
        return f"CSV header must be exactly: {','.join(WEAPON_COLUMNS)}"
    if len(rows) != 4:
        return "CSV must contain exactly 3 data rows (AR, Magnum, Rocket)"
    for r in rows[1:]:
        if len(r) != len(WEAPON_COLUMNS):
            return f"row {r[0] if r else '?'} has {len(r)} cells, expected {len(WEAPON_COLUMNS)}"
        cell = dict(zip(WEAPON_COLUMNS, r))
        try:
            for col in ("DamagePerShot", "RPM", "MagSize", "ProjectileSpeed"):
                float(cell[col])
        except ValueError:
            return f"row {r[0]}: {col} must be numeric"
        if cell["DamageDelivery"] not in ("Hitscan", "Projectile"):
            return f"row {r[0]}: DamageDelivery must be Hitscan or Projectile"
        if (cell["DamageDelivery"] == "Hitscan") != (float(cell["ProjectileSpeed"]) == 0):
            return (f"row {r[0]}: ProjectileSpeed must be 0 iff DamageDelivery "
                    f"is Hitscan")
    return None


def validate_verifier_report(data):
    if data.get("verdict") not in ("PASS", "FAIL"):
        return "expected `verdict`: \"PASS\" or \"FAIL\""
    checks = data.get("checks")
    if not isinstance(checks, list) or not checks:
        return "expected `checks`: a non-empty list of {name, result, detail}"
    return None


def job_weapons(eng, agents_dir):
    curator = load_agent(agents_dir, "tuning-curator")
    critic = load_agent(agents_dir, "critic")
    builder = load_agent(agents_dir, "builder")
    verifier = load_agent(agents_dir, "verifier")
    job = "weapons"
    open_risks = []
    log("\n=== JOB: weapons → DT_Weapons.csv ===")

    schema = ",".join(WEAPON_COLUMNS)
    curator_task = f"""# TASK — propose the initial DT_Weapons rows for BREACHPOINT
Combat model (canon, from the GDD):
{COMBAT_MODEL}
Schema (UE DataTable CSV columns): {schema}
`Name` is the UE row name (AR, Magnum, Rocket). `MeshSoftPath` is a soft object
path under /Game/Weapons/. `FireCueTag` is a GameplayCue tag
(GameplayCue.Weapon.<Name>.Fire).
Two distinct columns, do not conflate them:
  `FireMode` = trigger cadence, exactly one of {{Automatic, SemiAuto}}.
  `DamageDelivery` = how the shot reaches the target, exactly one of
  {{Hitscan, Projectile}}. AR and Magnum are Hitscan; Rocket is Projectile.
`ProjectileSpeed` (m/s) must be 0 if and only if DamageDelivery is Hitscan.
Complete the table: keep the GDD first-pass values, propose the missing columns
(reserve mags, reload, equip, splash, paths, tags), and defend every number
against the shields+health model per your doctrine — state the implied
body-shot TTK vs 200 EHP for each weapon, and how the AR-strip -> Magnum-finish
combo plays out including the 0.4 s swap.

# OUTPUT
Return ONLY a JSON object:
{{"proposals": [{{"full_row": {{<every schema column>}}, "evidence": "...",
"implied_ttk_s": <number>, "expected_effect": "...", "risk": "...",
"doubts": ["..."]}}, ...3 records...]}}"""

    proposals = ask(eng, curator, "tuning-curator", "propose", job, curator_task,
                    validate_weapon_proposals)
    log(f"  tuning-curator: 3 rows proposed "
        f"(TTKs: {', '.join(str(p['implied_ttk_s']) for p in proposals['proposals'])} s)")

    for bounce in range(MAX_BOUNCES + 1):
        last_round = (bounce == MAX_BOUNCES)
        critic_task = f"""# TASK — REFUTER mode: attack these proposed DT_Weapons rows
Combat model (canon):
{COMBAT_MODEL}
{REVIEW_DOCTRINE}
{"# FINAL ROUND — only hard-constraint violations or internal contradictions may block now. Anything softer goes to doubts[] and PASSes." if last_round else ""}
Proposed rows with the curator's evidence:
{json.dumps(proposals, indent=2)}

Attack the numbers, not the formatting. Candidate attacks: a TTK that breaks the
strip-then-finish sandbox (AR should strip shields in ~1.3 s but never
out-finish the Magnum; Magnum headshots must reward aim without making body
shots pointless); a weapon that dominates or invalidates another slot; a mag
that cannot complete its job (can the AR strip 100 shields in one mag? can the
Magnum finish 100 health?); rocket splash/damage that trivializes or fails the
power-weapon fantasy; reload/equip times that erase the 0.4 s swap decision.
Every finding needs a concrete failure scenario: input -> wrong outcome in a
live 4v4. If the rows hold, PASS is allowed — agreement as last resort.

# OUTPUT
Return ONLY a JSON object:
{{"verdict": "PASS"}} or
{{"verdict": "FINDINGS", "findings": [{{"row": "...", "failure_scenario": "...",
"severity": "high|medium|low", "suggested_fix": "..."}}]}}"""

        review = ask(eng, critic, "critic", "refute", job, critic_task, validate_verdict)
        blocking, accepted = split_findings(review)
        for f in review.get("findings", []):
            log(f"    - [{f.get('severity', '?')}] {f.get('row', '?')}: {f['failure_scenario'][:110]}")
        open_risks.extend(accepted)
        if not blocking:
            log(f"  critic: no blocking findings (bounce {bounce}; "
                f"{len(accepted)} non-blocking recorded as accepted risk)")
            break
        log(f"  critic: {len(blocking)} BLOCKING finding(s) — bouncing to curator")
        if bounce == MAX_BOUNCES:
            log("REFUTED: blocking findings survived every bounce — escalating to the human lead")
            sys.exit(2)
        revise_task = curator_task + f"""

# REVISION REQUIRED (bounce {bounce + 1})
The critic raised these BLOCKING findings against your previous proposals:
{json.dumps(blocking, indent=2)}
Address every one (fix the value, or rebut with evidence in `doubts`) and
return the full corrected JSON object. Change only what these findings require
— churning unrelated numbers creates new inconsistencies."""
        proposals = ask(eng, curator, "tuning-curator", "revise", job, revise_task,
                        validate_weapon_proposals)
        log("  tuning-curator: revised proposals returned")

    builder_task = f"""# TASK — land the validated DT_Weapons rows as a UE DataTable CSV
These rows survived adversarial review. Produce the exact file content for
Content/Data/DT_Weapons.csv, importable against FBRWeaponRow in
Source/Breachpoint/Data/BRDataRows.h.
Header (exact, in order): {schema}
Validated rows:
{json.dumps([p["full_row"] for p in proposals["proposals"]], indent=2)}
Rules: no reordering, no value changes (a change here is a silent drift — the
bug class this pipeline exists to kill), RFC-4180 quoting where needed,
row order AR, Magnum, Rocket.

# OUTPUT
Return ONLY a JSON object: {{"csv_text": "<full file content>", "notes": "..."}}"""

    built = ask(eng, builder, "builder", "land", job, builder_task, validate_built_csv)
    log("  builder: CSV formatted (gate B: header + 3 rows verified deterministically)")

    verifier_task = f"""# TASK — prove the artifact (read-only)
Packet: DT_Weapons.csv for BREACHPOINT. Combat model (canon):
{COMBAT_MODEL}
Artifact under test (verbatim):
```csv
{built["csv_text"]}
```
Run these checks and report verbatim results, showing your arithmetic:
1. Schema: header matches {schema} exactly; every cell parses to its type.
2. TTK recompute, per weapon, from the row values alone: shots to break
   200 EHP = ceil(200 / DamagePerShot); body TTK = (shots - 1) * (60 / RPM).
   State each result. Flag any weapon whose mag cannot complete its
   sandbox job (AR: strip 100 shields; Magnum: finish 100 health on headshots).
3. Headshot math: Magnum headshot damage and shots-to-finish 100 health.
4. UE importability: soft paths well-formed (/Game/...), cue tags match
   GameplayCue.Weapon.<Name>.Fire, FireMode in {{Automatic, SemiAuto}},
   DamageDelivery in {{Hitscan, Projectile}}, and ProjectileSpeed == 0 if and
   only if DamageDelivery == Hitscan.
A check you cannot run is BLOCKED with the reason — never silently skipped.

# OUTPUT
Return ONLY a JSON object:
{{"verdict": "PASS" or "FAIL", "checks": [{{"name": "...", "result":
"pass|fail|blocked", "detail": "<the actual numbers/evidence>"}}]}}"""

    report = ask(eng, verifier, "verifier", "prove", job, verifier_task,
                 validate_verifier_report)
    for c in report["checks"]:
        log(f"  verifier: [{c['result']}] {c['name']}: {c['detail'][:110]}")
    if report["verdict"] != "PASS":
        log("VERIFIER FAILED — artifact not landed")
        sys.exit(3)

    out = OUTPUT_DIR / "DT_Weapons.csv"
    out.write_text(built["csv_text"], encoding="utf-8")
    log(f"  LANDED: {out.relative_to(HERE)} (written only after verifier PASS)")
    write_risk_register(job, open_risks)


# ---------------------------------------------------------------------------
# Job 2 — arena: arena-architect → critic → builder → verifier → arena_manifest.json
# ---------------------------------------------------------------------------

def validate_arena_manifest(data):
    man = data.get("manifest")
    if not isinstance(man, dict):
        return "expected `manifest`: the arena_manifest object"
    for key in ("arena_id", "bounds", "spawn_points", "landmarks", "cover",
                "sightlines", "hazards", "doubts"):
        if key not in man:
            return f"manifest missing `{key}`"
    spawns = man["spawn_points"]
    if not isinstance(spawns, list) or len(spawns) < 8:
        return "need >= 8 spawn_points for scored 4v4 respawns"
    for s in spawns:
        loc = s.get("location", {})
        if not all(k in loc for k in ("x", "y", "z")):
            return f"spawn {s.get('id', '?')} needs location {{x,y,z}} (metres)"
        if "scoring_hints" not in s:
            return f"spawn {s.get('id', '?')} missing scoring_hints"
    if float(man["sightlines"].get("max_length_m", 999)) > 35:
        return "sightlines.max_length_m exceeds the GDD cap of 35 m"
    # deterministic anti-spawn-camp gate: no two spawns closer than 8 m
    for i, a in enumerate(spawns):
        for b in spawns[i + 1:]:
            d = math.dist((a["location"]["x"], a["location"]["y"], a["location"]["z"]),
                          (b["location"]["x"], b["location"]["y"], b["location"]["z"]))
            if d < 8:
                return f"spawns {a.get('id')} and {b.get('id')} are {d:.1f} m apart (< 8 m)"
    if not any(("rocket" in (l.get("name", "") + l.get("purpose", "")).lower())
               for l in man["landmarks"]):
        return "no landmark serves the contested Rocket Launcher pad"
    return None


def validate_built_manifest(data):
    text = data.get("json_text")
    if not isinstance(text, str):
        return "expected `json_text`: the full arena_manifest.json content as a string"
    try:
        obj = json.loads(text)
    except json.JSONDecodeError as e:
        return f"json_text is not valid JSON: {e}"
    err = validate_arena_manifest({"manifest": obj})
    return err


def job_arena(eng, agents_dir):
    architect = load_agent(agents_dir, "arena-architect")
    critic = load_agent(agents_dir, "critic")
    builder = load_agent(agents_dir, "builder")
    verifier = load_agent(agents_dir, "verifier")
    job = "arena"
    open_risks = []
    log("\n=== JOB: arena → arena_manifest.json ===")

    architect_task = f"""# TASK — design the vertical-slice arena for BREACHPOINT
Brief:
{ARENA_BRIEF}
Coordinates in metres, z = floor height (three levels). Follow your doctrine's
manifest shape exactly; scoring_hints per spawn = {{"min_dist_to_combat_m":
<number>, "last_used_cooldown_s": <number>}}.

CLAIM DISCIPLINE (this is what gets manifests rejected):
- `sightlines.notes` states ONLY what the coordinates prove — a measured
  distance, a footprint that provably spans a segment. NEVER write a blanket
  claim like "the Core blocks every corner-to-corner line": a reviewer will
  test it with arithmetic and find the diagonal it misses.
- Occlusion, mutual visibility, and 5 m line-of-sight breakage CANNOT be proven
  from coordinates. Each belongs in `doubts[]` phrased as a claim to be
  confirmed at the editor walkthrough rung — not asserted as fact.
- Before submitting, check your own numbers: no two landmarks or cover volumes
  may occupy the same (x,y) footprint, and no spawn may sit inside one.

# OUTPUT
Return ONLY a JSON object: {{"manifest": {{...arena_manifest...}},
"blockout_notes": "..."}}"""

    result = ask(eng, architect, "arena-architect", "propose", job, architect_task,
                 validate_arena_manifest)
    man = result["manifest"]
    log(f"  arena-architect: '{man['arena_id']}' — {len(man['spawn_points'])} spawns, "
        f"{len(man['landmarks'])} landmarks, max sightline {man['sightlines']['max_length_m']} m")

    for bounce in range(MAX_BOUNCES + 1):
        last_round = (bounce == MAX_BOUNCES)
        critic_task = f"""# TASK — REFUTER mode: attack this arena manifest
Brief (the law, not inspiration):
{ARENA_BRIEF}
{REVIEW_DOCTRINE}
{"# FINAL ROUND — only hard-constraint violations or internal contradictions may block now. Anything softer goes to doubts[] and PASSes." if last_round else ""}
Manifest:
{json.dumps(result, indent=2)}
Candidate attacks: a spawn pair the stated cover does not actually separate;
a spawn without line-of-sight breakage within 5 m; a sightline the layout
implies but the manifest under-reports; a rocket pad that one team can hold
risk-free; a landmark nobody could call out; dead vertical space the
Grappleshot (20 m) cannot make relevant; spawn scoring that starves one team.
Every finding needs a concrete failure scenario in a live 4v4. Geometry you
cannot prove from the manifest alone is attacked as an under-reported doubt,
not invented. PASS allowed — agreement as last resort.

# OUTPUT
Return ONLY a JSON object:
{{"verdict": "PASS"}} or
{{"verdict": "FINDINGS", "findings": [{{"target": "...", "failure_scenario":
"...", "severity": "high|medium|low", "suggested_fix": "..."}}]}}"""

        review = ask(eng, critic, "critic", "refute", job, critic_task, validate_verdict)
        blocking, accepted = split_findings(review)
        for f in review.get("findings", []):
            log(f"    - [{f.get('severity', '?')}] {f.get('target', '?')}: {f['failure_scenario'][:110]}")
        open_risks.extend(accepted)
        if not blocking:
            log(f"  critic: no blocking findings (bounce {bounce}; "
                f"{len(accepted)} non-blocking recorded as accepted risk)")
            break
        log(f"  critic: {len(blocking)} BLOCKING finding(s) — bouncing to architect")
        if bounce == MAX_BOUNCES:
            log("REFUTED: blocking findings survived every bounce — escalating to the human lead")
            sys.exit(2)
        revise_task = architect_task + f"""

# REVISION REQUIRED (bounce {bounce + 1})
The critic raised these BLOCKING findings against your previous manifest:
{json.dumps(blocking, indent=2)}
Address every one (move geometry, add cover, or concede into doubts[] with the
editor-rung caveat) and return the full corrected JSON. Change only what these
findings require — churning unrelated coordinates creates new collisions."""
        result = ask(eng, architect, "arena-architect", "revise", job, revise_task,
                     validate_arena_manifest)
        man = result["manifest"]
        log("  arena-architect: revised manifest returned")

    builder_task = f"""# TASK — land the validated arena manifest
This manifest survived adversarial review. Produce the exact file content for
Content/Data/arena_manifest.json — the source of truth the blockout builder
executes in the editor (binary-asset law: the .umap has one owner; this file
is the spec it follows).
Validated manifest:
{json.dumps(man, indent=2)}
Rules: no geometry changes (a change here is a silent drift), stable key
order, 2-space indentation, add one top-level key "landing_note" (string):
what the executing builder must do and which ladder rung verifies it.

# OUTPUT
Return ONLY a JSON object: {{"json_text": "<full file content>", "notes": "..."}}"""

    built = ask(eng, builder, "builder", "land", job, builder_task, validate_built_manifest)
    log("  builder: manifest formatted (gate B: parse + schema + spawn spacing re-verified)")

    verifier_task = f"""# TASK — prove the artifact (read-only)
Packet: arena_manifest.json for BREACHPOINT. Brief (the law):
{ARENA_BRIEF}
Artifact under test (verbatim):
```json
{built["json_text"]}
```
Run these checks and report verbatim results, showing your arithmetic:
1. Schema: all required keys; every spawn has location, facing, scoring_hints.
2. Spawn spacing: compute the minimum pairwise spawn distance from the
   coordinates; state the pair and the number.
3. Constraint audit: spawn count >= 8, max sightline <= 35 m, a named rocket
   landmark exists, three distinct z-levels are actually used.
4. Honesty audit: which claims require the editor rung (mutual visibility,
   LOS breakage) — confirm the manifest's doubts[]/landing_note say so rather
   than overclaiming. Overclaiming is a FAIL.
A check you cannot run is BLOCKED with the reason — never silently skipped.

# OUTPUT
Return ONLY a JSON object:
{{"verdict": "PASS" or "FAIL", "checks": [{{"name": "...", "result":
"pass|fail|blocked", "detail": "<the actual numbers/evidence>"}}]}}"""

    report = ask(eng, verifier, "verifier", "prove", job, verifier_task,
                 validate_verifier_report)
    for c in report["checks"]:
        log(f"  verifier: [{c['result']}] {c['name']}: {c['detail'][:110]}")
    if report["verdict"] != "PASS":
        log("VERIFIER FAILED — artifact not landed")
        sys.exit(3)

    out = OUTPUT_DIR / "arena_manifest.json"
    out.write_text(built["json_text"], encoding="utf-8")
    log(f"  LANDED: {out.relative_to(HERE)} (written only after verifier PASS)")
    write_risk_register(job, open_risks)


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="BREACHPOINT agent crew orchestrator")
    ap.add_argument("--live", action="store_true",
                    help="call Claude for real (default: replay recording.json)")
    ap.add_argument("--job", choices=["weapons", "arena", "all"], default="all")
    ap.add_argument("--model", default=DEFAULT_MODEL)
    ap.add_argument("--recording", default=str(RECORDING_PATH),
                    help="recording file to write (--live) or replay from")
    args = ap.parse_args()

    # Absolute: an aborted run must still save the exchanges it earned, even if
    # the process cwd has moved.
    rec = Path(args.recording)
    rec = rec if rec.is_absolute() else (HERE / rec)
    agents_dir = find_agents_dir()
    eng = LiveEngine(args.model, rec) if args.live else ReplayEngine(rec)
    mode = "LIVE" if args.live else "REPLAY (recorded exchanges, same pipeline code)"
    log(f"BREACHPOINT agent crew — mode: {mode} — agents: {agents_dir}")

    OUTPUT_DIR.mkdir(exist_ok=True)
    try:
        if args.job in ("weapons", "all"):
            job_weapons(eng, agents_dir)
        if args.job in ("arena", "all"):
            job_arena(eng, agents_dir)
    finally:
        eng.save_recording()
        (OUTPUT_DIR / "run_log.txt").write_text("\n".join(LOG_LINES) + "\n",
                                                encoding="utf-8")
    log("\nAll jobs landed. Output in ./output — game-ready data for BREACHPOINT.")


if __name__ == "__main__":
    main()
