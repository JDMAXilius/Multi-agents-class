#!/usr/bin/env python3
"""BREACHPOINT dynamic content pipeline — Assignment #4.

    prove gaps -> retrieve from the GDD -> generate -> gate -> critic -> revise
    -> gate -> land

Three content types, each one a hole this repo can *prove* it has (`gaps.py`):

  1. DT_SpotterLines_Additions.csv  the announcer lines for four medals that
                                    currently award in total silence
  2. DT_CoachLines.csv              the canned coach table §3.3 says ships in
                                    the build, so the game is "identical minus
                                    flavor" with no connectivity
  3. DT_BotCallsigns.csv            names for the bots that fill up to 7 of 8
                                    slots and currently render as engine defaults

The knowledge base is the capstone's own `BREACHPOINT-GDD-VERTICAL-SLICE.md`
plus its shipped `Content/Data/*.csv`. The generator is told, in the prompt,
that everything it knows about the game must come from the retrieved context —
so a fact in the output that is not in a retrieved chunk is a bug, and the
trace in `output/rag_trace.md` is what makes that checkable.

Usage
-----
    python3 run_pipeline.py                   # replay the committed run (no key needed)
    python3 run_pipeline.py --gaps            # just prove the gaps, no model calls
    python3 run_pipeline.py --dry-run --live  # probe wiring: one tiny call per agent
    python3 run_pipeline.py --live            # real calls, re-records
    python3 run_pipeline.py --live --scope all --recording recording_naive.json
                                              # the un-tweaked retrieval, kept as evidence

Stdlib only. Live runs use ANTHROPIC_API_KEY + `pip install anthropic` if present,
otherwise the `claude` CLI on PATH.
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
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

import gaps as gapmod
import rag

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
OUT = HERE / "output"
DEFAULT_RECORDING = HERE / "recording.json"
DEFAULT_MODEL = "claude-sonnet-5"

EXIT_OK, EXIT_ESCALATE, EXIT_NOT_LANDED, EXIT_GAP_CLOSED = 0, 2, 3, 4


def log(msg: str = ""):
    print(msg, flush=True)


# ===========================================================================
# Canon lint — the deterministic half of consistency checking.
# ===========================================================================
#
# The LLM critic argues about meaning; this argues about vocabulary, and it
# cannot be talked out of a finding. Both lists come from the GDD itself:
# CUT is §6's cut table (systems that exist in the full concept and are NOT in
# the slice), FOREIGN is genre vocabulary from the games BREACHPOINT is
# compared to. A generated line mentioning a motion tracker is not a style
# problem — it describes a system the player does not have.

CUT_SYSTEMS = {
    r"\bmotion tracker\b": "§6 — motion tracker is CUT from the slice",
    r"\bradar\b": "§2.7 — the slice ships no radar; audio carries awareness",
    r"\bplasma\b": "§6 — Plasma Rifle is CUT from the slice",
    r"\bvehicle\b|\bwarthog\b|\bbanshee\b": "§6 — vehicles are not planned",
    r"\bfirefight\b|\bfree.for.all\b|\bffa\b": "§6 — FFA and Firefight are CUT",
    r"\bcapture the flag\b|\bctf\b|\bflag\b|\boddball\b|\bking of the hill\b":
        "§5.1 — Team Slayer is the only shipped mode",
    r"\bdedicated server\b": "§6 — dedicated servers are Phase 2",
    r"\bsecond map\b|\bnext map\b|\bmap rotation\b": "§5.1 — one arena ships",
}

FOREIGN_VOCAB = {
    r"\bspartan\b|\bcovenant\b|\bmaster chief\b|\bkilltacular\b|\bslayer!\b":
        "another game's IP, not BREACHPOINT's",
    r"\bheadshot!\b|\bboom\b|\bnice shot\b|\bgg\b|\bnoob\b":
        "chat-speak; the announcer is clipped military callout",
}

WEAPONS_IN_SLICE = {"assault rifle", "ar", "magnum", "rocket launcher", "rocket",
                    "grenade", "frag", "melee", "grappleshot"}


def canon_lint(texts: list[tuple[str, str]]) -> list[dict]:
    """texts: [(row_id, text)] -> findings. Deterministic, no model involved."""
    findings = []
    for row_id, text in texts:
        low = text.lower()
        for pattern, why in {**CUT_SYSTEMS, **FOREIGN_VOCAB}.items():
            m = re.search(pattern, low)
            if m:
                findings.append({"row": row_id, "kind": "canon-lint",
                                 "severity": "high", "quote": m.group(0),
                                 "why": why})
    return findings


# ===========================================================================
# Engines
# ===========================================================================

class LiveEngine:
    """Real model calls: anthropic SDK when available, else the `claude` CLI."""

    def __init__(self, model: str, recording_path: Path):
        self.model, self.recording_path, self.exchanges = model, recording_path, []
        self._api = None
        if os.environ.get("ANTHROPIC_API_KEY"):
            try:
                import anthropic
                self._api = anthropic.Anthropic()
            except ImportError:
                pass
        if self._api is None and not _which("claude"):
            sys.exit("error: --live needs ANTHROPIC_API_KEY + `pip install anthropic`, "
                     "or the `claude` CLI on PATH")

    def call(self, prompt: str, meta: dict) -> str:
        usage = {}
        if self._api is not None:
            resp = self._api.messages.create(
                model=self.model, max_tokens=8000,
                messages=[{"role": "user", "content": prompt}])
            text = "".join(b.text for b in resp.content if b.type == "text")
            u = getattr(resp, "usage", None)
            if u is not None:
                usage = {"input_tokens": getattr(u, "input_tokens", 0),
                         "output_tokens": getattr(u, "output_tokens", 0)}
        else:
            # A nested `claude` inherits the parent session's BUN_OPTIONS and
            # dies on a bare ENOENT. Strip it. (Same fix as Assignment #3.)
            env = {k: v for k, v in os.environ.items() if k != "BUN_OPTIONS"}
            proc = subprocess.run(
                ["claude", "-p", prompt, "--model", self.model,
                 "--output-format", "json"],
                capture_output=True, text=True, timeout=900, env=env)
            if proc.returncode != 0:
                sys.exit(f"error: claude CLI failed at {meta}: {proc.stderr.strip()[:400]}")
            try:
                payload = json.loads(proc.stdout)
                text = (payload.get("result") or "").strip()
                u = payload.get("usage") or {}
                usage = {k: u.get(k, 0) for k in
                         ("input_tokens", "output_tokens",
                          "cache_read_input_tokens", "cache_creation_input_tokens")}
                if payload.get("total_cost_usd") is not None:
                    usage["cost_usd"] = payload["total_cost_usd"]
            except json.JSONDecodeError:
                text = proc.stdout.strip()
        self.exchanges.append({**meta, "prompt": prompt, "response": text, "usage": usage})
        if usage:
            log(f"        tokens in {usage.get('input_tokens', 0):,} · "
                f"out {usage.get('output_tokens', 0):,}"
                + (f" · ${usage['cost_usd']:.4f}" if usage.get("cost_usd") else ""))
        return text

    def save_recording(self):
        self.recording_path.write_text(json.dumps(
            {"model": self.model, "exchanges": self.exchanges,
             "totals": summarize_usage(self.exchanges)}, indent=2), encoding="utf-8")


class ReplayEngine:
    """Re-drives recorded responses through the same code paths.

    Everything except the model executes for real on replay: retrieval, the
    gates, the canon lint, the bounce loop, CSV assembly. That is the point —
    the pipeline has to be demonstrably runnable on a machine with no API key,
    and 'runs' has to mean more than 'prints the committed answer'.
    """

    def __init__(self, recording_path: Path):
        if not recording_path.exists():
            sys.exit(f"error: no {recording_path.name} — run with --live first")
        data = json.loads(recording_path.read_text(encoding="utf-8"))
        self.exchanges, self.i = data["exchanges"], 0
        self.totals = data.get("totals", {})

    def call(self, prompt: str, meta: dict) -> str:
        if self.i >= len(self.exchanges):
            sys.exit(f"error: replay exhausted at {meta}")
        ex = self.exchanges[self.i]
        self.i += 1
        if (ex["agent"], ex["stage"], ex.get("job")) != (meta["agent"], meta["stage"], meta.get("job")):
            sys.exit(f"error: replay mismatch — recorded {ex['agent']}/{ex['stage']}/"
                     f"{ex.get('job')}, pipeline asked for {meta['agent']}/{meta['stage']}/"
                     f"{meta.get('job')}")
        return ex["response"]

    def save_recording(self):
        pass


class DryRunComplete(Exception):
    pass


class DryRunEngine:
    """Assemble every prompt, call (almost) nothing. Seconds, not minutes."""

    def __init__(self, model: str, live: bool, recording_path: Path):
        self.inner = LiveEngine(model, recording_path) if live else None
        self.seen: set[str] = set()

    def call(self, prompt: str, meta: dict) -> str:
        assert isinstance(prompt, str) and prompt.strip(), "prompt assembly produced nothing"
        agent = meta["agent"]
        if agent not in self.seen:
            self.seen.add(agent)
            if self.inner is not None:
                reply = self.inner.call(
                    f"Connectivity probe for the '{agent}' slot. Reply with one word.", meta)
                log(f"  dry-run: {agent:10} prompt {len(prompt):6,d} chars · "
                    f"round trip {'OK' if reply.strip() else 'EMPTY — check auth/model'}")
            else:
                log(f"  dry-run: {agent:10} prompt {len(prompt):6,d} chars · assembled")
        raise DryRunComplete(f"{agent}/{meta['stage']}")

    def save_recording(self):
        pass


def _which(cmd):
    from shutil import which
    return which(cmd)


def summarize_usage(exchanges: list) -> dict:
    tot = {"input_tokens": 0, "output_tokens": 0, "cost_usd": 0.0, "calls": len(exchanges)}
    for ex in exchanges:
        u = ex.get("usage") or {}
        tot["input_tokens"] += u.get("input_tokens", 0)
        tot["output_tokens"] += u.get("output_tokens", 0)
        tot["cost_usd"] += u.get("cost_usd", 0.0)
    return tot


# ===========================================================================
# Model I/O helpers
# ===========================================================================

def extract_json(text: str):
    """Models wrap JSON in prose or fences more often than they don't."""
    text = text.strip()
    fence = re.search(r"```(?:json)?\s*(.+?)```", text, re.S)
    if fence:
        text = fence.group(1).strip()
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        pass
    for opener, closer in (("{", "}"), ("[", "]")):
        start, end = text.find(opener), text.rfind(closer)
        if start != -1 and end > start:
            try:
                return json.loads(text[start:end + 1])
            except json.JSONDecodeError:
                continue
    raise ValueError(f"no JSON object in response (first 200 chars: {text[:200]!r})")


def ask(engine, agent: str, stage: str, job: str, prompt: str,
        validate: Callable | None = None, retries: int = 1):
    """One call, with the error fed back verbatim for a single self-correction.

    Class-04 error handling made concrete: don't retry blind, tell the model
    exactly which invariant it broke. The retry budget is 1 on purpose — a
    second failure is a prompt bug, and looping on it just spends money.
    """
    meta = {"agent": agent, "stage": stage, "job": job}
    attempt, last_err = 0, None
    while attempt <= retries:
        text = engine.call(prompt if attempt == 0 else
                           f"{prompt}\n\n## YOUR PREVIOUS REPLY FAILED VALIDATION\n"
                           f"{last_err}\n\nReturn corrected JSON only.", meta)
        try:
            data = extract_json(text)
            if validate:
                validate(data)
            return data
        except (ValueError, KeyError, AssertionError) as e:
            last_err = str(e)
            log(f"        gate rejected ({attempt + 1}/{retries + 1}): {last_err[:160]}")
            attempt += 1
    sys.exit(f"error: {agent}/{stage} failed validation twice: {last_err}")


# ===========================================================================
# Jobs
# ===========================================================================

@dataclass
class Job:
    key: str
    gap_key: str
    title: str
    out_file: str
    query: str
    boost: dict = field(default_factory=dict)
    k: int = 6
    task: str = ""
    columns: list[str] = field(default_factory=list)
    validate: Callable | None = None
    dynamic_task: Callable | None = None   # builds task text from proven gap data


def _word_cap(rows, field_name, cap, ctx):
    for r in rows:
        n = len(str(r[field_name]).split())
        if n > cap:
            raise ValueError(f"{ctx}: row {r.get('RowName')} has {n} words in "
                             f"{field_name} (cap {cap}): {r[field_name]!r}")


def _require(rows, cols, ctx):
    if not isinstance(rows, list) or not rows:
        raise ValueError(f"{ctx}: expected a non-empty list of rows")
    for r in rows:
        missing = [c for c in cols if c not in r]
        if missing:
            raise ValueError(f"{ctx}: row {r!r} missing {missing}")
    names = [r["RowName"] for r in rows]
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        raise ValueError(f"{ctx}: duplicate RowName(s) {sorted(dupes)}")


# --- job 1: announcer lines for the four silent medals ---------------------

ANNOUNCER_COLS = ["RowName", "TriggerId", "Text", "Audience", "Weight", "RepeatCooldown_s"]


def validate_announcer(data):
    rows = data["rows"] if isinstance(data, dict) else data
    _require(rows, ANNOUNCER_COLS, "announcer")
    _word_cap(rows, "Text", 6, "announcer")        # measured: existing table is 1-6 words
    for r in rows:
        if "!" in r["Text"]:
            raise ValueError(f"announcer: exclamation mark in {r['RowName']} — the "
                             f"63 shipped lines contain zero")
        if r["Audience"] not in ("Self", "Team", "All"):
            raise ValueError(f"announcer: Audience {r['Audience']!r} not in Self/Team/All")
        if str(r["RepeatCooldown_s"]) not in ("0", "8", "20"):
            raise ValueError(f"announcer: RepeatCooldown_s {r['RepeatCooldown_s']!r} "
                             f"not one of the shipped values 0/8/20")
    return rows


# --- job 2: the canned coach fallback table --------------------------------

COACH_COLS = ["RowName", "ConditionId", "TelemetryField", "Threshold", "Text", "Priority"]


def validate_coach(data):
    rows = data["rows"] if isinstance(data, dict) else data
    _require(rows, COACH_COLS, "coach")
    _word_cap(rows, "Text", 30, "coach")           # GDD §3.3: coach line <= 30 words
    for r in rows:
        # §3.3: "references >= 1 telemetry stat". Offline that means a substitution
        # token, so the invariant is checkable: the token must name the row's field.
        if "{" not in r["Text"] or "}" not in r["Text"]:
            raise ValueError(f"coach: {r['RowName']} has no {{token}} — §3.3 requires "
                             f"the coach line to reference a telemetry stat")
        tokens = set(re.findall(r"\{(\w+)\}", r["Text"]))
        if r["TelemetryField"] not in tokens:
            raise ValueError(f"coach: {r['RowName']} declares TelemetryField "
                             f"{r['TelemetryField']!r} but its text substitutes {sorted(tokens)}")
    return rows


# --- job 3: bot callsigns --------------------------------------------------

CALLSIGN_COLS = ["RowName", "Callsign", "ProfileHint", "Note"]
BOT_PROFILES = {"Recruit", "Marine", "Veteran"}


def validate_callsigns(data):
    rows = data["rows"] if isinstance(data, dict) else data
    _require(rows, CALLSIGN_COLS, "callsigns")
    if len(rows) < 12:
        raise ValueError(f"callsigns: {len(rows)} rows — solo play needs 7 bots and "
                         f"repeats inside one match read as a bug; give at least 12")
    seen = set()
    for r in rows:
        cs = r["Callsign"]
        if len(cs) > 12:
            raise ValueError(f"callsigns: {cs!r} is {len(cs)} chars — the killfeed "
                             f"column is narrow; cap 12")
        if cs.lower() in seen:
            raise ValueError(f"callsigns: duplicate callsign {cs!r}")
        seen.add(cs.lower())
        if r["ProfileHint"] not in BOT_PROFILES:
            raise ValueError(f"callsigns: ProfileHint {r['ProfileHint']!r} is not one of "
                             f"{sorted(BOT_PROFILES)} (DT_BotTuning row names)")
    return rows


def build_jobs(proven: dict[str, gapmod.Gap]) -> list[Job]:
    orphans = getattr(proven["announcer_coverage"], "orphans", [])
    orphan_desc = "\n".join(
        f"  - {r['MedalName']} — TriggerId `{r['TriggerId']}` — awarded when: {r['Description']}"
        for r in orphans)

    return [
        Job(key="announcer", gap_key="announcer_coverage",
            title="Announcer lines for the four medals that award in silence",
            out_file="DT_SpotterLines_Additions.csv",
            query=("announcer callout line medal killfeed rocket multi kill first kill "
                   "of the match sudden death killing spree ended clipped military voice"),
            boost={"DT_SpotterLines.csv": 2.5, "DT_Medals.csv": 2.0},
            k=7,
            columns=ANNOUNCER_COLS, validate=validate_announcer,
            task=f"""Write announcer lines for four medals that currently award with NO
audio line. These rows append to the shipped `DT_SpotterLines.csv`.

The four uncovered triggers:
{orphan_desc}

Write EXACTLY 3 line variants per trigger (12 rows total). Variants exist so a
player who earns the same medal twice in a match does not hear the identical
clip.

Schema — one JSON object per row, keys exactly:
  RowName            continue the shipped series; use S22a..S22c, S23a..S23c,
                     S24a..S24c, S25a..S25c
  TriggerId          the trigger from the list above, copied exactly
  Text               the spoken line
  Audience           Self | Team | All  (match how the shipped table scopes
                     comparable events)
  Weight             1.0
  RepeatCooldown_s   one of the shipped values: 0, 8, or 20 — pick the one the
                     shipped table uses for comparable events

Return: {{"rows": [ ... ]}}"""),

        Job(key="coach", gap_key="coach_fallback",
            title="The canned coach table that ships as the Spotter's offline fallback",
            out_file="DT_CoachLines.csv",
            query=("spotter coach line match end telemetry stat fights lost below 40% "
                   "shields accuracy grapple rocket holds shield break conversion "
                   "canned fallback no connectivity"),
            boost={"DT_SpotterLines.csv": 1.5},
            k=8,
            columns=COACH_COLS, validate=validate_coach,
            task="""Write the canned coach-line table the GDD says ships in the build so
that with no connectivity "the game is identical minus flavor."

Each row is a match-end coaching line selected by a telemetry condition, with
the stat substituted in at runtime. The retrieved telemetry schema is the ONLY
source of field names — do not invent a stat the game does not record.

Write 10 rows covering distinct, non-overlapping player mistakes and strengths.

Schema — one JSON object per row, keys exactly:
  RowName          C01..C10
  ConditionId      short PascalCase id, e.g. LostFightsLowShields
  TelemetryField   snake_case field name derived from the retrieved telemetry
                   schema, e.g. fights_lost_below_40_shields
  Threshold        the numeric value the field is compared against to fire this row
  Text             the coach line. MUST contain {TelemetryField} as a
                   substitution token, spelled exactly as TelemetryField.
                   Max 30 words. Be specific and corrective — name the mistake
                   AND the fix.
  Priority         1 (highest) .. 10; only the top-scoring row is shown

Return: {"rows": [ ... ]}"""),

        Job(key="callsigns", gap_key="bot_callsigns",
            title="Callsigns for the bots that fill up to seven of eight slots",
            out_file="DT_BotCallsigns.csv",
            query=("bots fill every unfilled slot difficulty profiles Recruit Marine "
                   "Veteran killfeed scoreboard player name team slayer"),
            boost={"DT_BotTuning.csv": 2.0},
            k=6,
            columns=CALLSIGN_COLS, validate=validate_callsigns,
            task="""Bots fill every unfilled slot, so in solo play seven of the eight
names in the killfeed are bots. They currently have no names at all.

Write 15 callsigns. They appear in the killfeed and on the scoreboard, next to
human Steam names, so they must read as squad callsigns rather than as
usernames or as obvious robots.

Schema — one JSON object per row, keys exactly:
  RowName       B01..B15
  Callsign      the displayed name. MAX 12 characters — the killfeed column is
                narrow. No spaces, no leetspeak, no numbers-as-letters.
  ProfileHint   Recruit | Marine | Veteran — which DT_BotTuning profile this
                callsign suits. Spread them across all three.
  Note          one short line: why this callsign fits that profile

Return: {"rows": [ ... ]}"""),
    ]


# ===========================================================================
# Prompts
# ===========================================================================

GENERATOR_PROMPT = """You are the content generator for BREACHPOINT, a 4v4 arena FPS.

## HARD RULE — grounding
Everything you know about BREACHPOINT must come from the RETRIEVED CONTEXT
below. It is the game's real design document and its shipped data tables. If a
detail you want is not in the context, you do not have it: leave it out rather
than filling it in from other shooters you know. A fact in your output that is
not in the context is a defect, and this pipeline checks for exactly that.

## VOICE
Match the voice of the shipped tables in the context, not a generic game's.
Where the context contains existing rows of the table you are extending, they
are the style specification — read their length, punctuation and register and
stay inside them.

## RETRIEVED CONTEXT
{context}

## YOUR TASK
{task}

Return ONLY the JSON object. No commentary, no code fence.
"""

CRITIC_PROMPT = """You are the critic for BREACHPOINT's content pipeline. You are in
REFUTER mode: your job is to find what is wrong with the generated content, not
to appreciate it. A review that says "looks good" when a defect is present is
the failure mode this role exists to prevent.

You judge against the RETRIEVED CANON below and nothing else. Your own
knowledge of other shooters is not evidence — if the canon does not say it, the
canon does not say it.

## RETRIEVED CANON
{context}

## GENERATED CONTENT UNDER REVIEW
{content}

## WHAT COUNTS AS A FINDING
1. **lore-break** — the content states or implies something the canon
   contradicts, or references a system the canon says is cut/absent. Highest
   severity. Quote the canon line you are judging against.
2. **tone-drift** — the content does not sound like the shipped rows in the
   canon: wrong length, wrong register, exclamation marks, chattiness,
   jokes, marketing voice, or generic-shooter phrasing.
3. **schema-risk** — the row would break the game or the table on import.
4. **redundancy** — two rows say the same thing, so the variation they exist
   to provide does not exist.

Do NOT report taste preferences, and do NOT report a row as a lore-break just
because the canon is silent about it — silence is not contradiction.

## OUTPUT
{{"verdict": "PASS" | "FINDINGS",
  "findings": [
    {{"row": "<RowName>", "kind": "lore-break|tone-drift|schema-risk|redundancy",
      "severity": "high|medium|low",
      "quote": "<the exact text you object to>",
      "canon": "<the canon line or table row that refutes it, or '' for tone>",
      "why": "<one sentence: what breaks, concretely>",
      "fix": "<the corrected text you propose>"}}
  ]}}

Only `high` findings block. Return ONLY JSON.
"""

REVISE_PROMPT = """You are the content generator for BREACHPOINT. Your rows were
reviewed and the critic raised findings. Apply the blocking ones.

## RETRIEVED CONTEXT (unchanged)
{context}

## YOUR ROWS
{content}

## FINDINGS TO APPLY
{findings}

Rules:
- Change ONLY the rows named in the findings. Every other row must come back
  byte-identical — silent drift in unreviewed rows is its own bug class.
- Keep the schema exactly as it was.
- If you believe a finding is wrong, still return a row that satisfies the
  canon; disagreement is settled by evidence, not by ignoring review.

Return the COMPLETE row set as {{"rows": [ ... ]}}. JSON only.
"""


# ===========================================================================
# CSV landing
# ===========================================================================

def rows_to_csv(rows: list[dict], columns: list[str]) -> str:
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=columns, lineterminator="\n",
                       extrasaction="ignore")
    w.writeheader()
    for r in rows:
        w.writerow({c: r.get(c, "") for c in columns})
    return buf.getvalue()


# ===========================================================================
# The run
# ===========================================================================

def run_job(engine, index: rag.Index, job: Job, scope: str | None, traces: list,
            critic_records: list) -> list[dict]:
    log(f"\n=== {job.key}: {job.title}")

    # -- retrieve -----------------------------------------------------------
    hits = index.search(job.query, k=job.k, scope=scope, boost=job.boost)
    if not hits:
        sys.exit(f"error: retrieval returned nothing for {job.key}")
    context = rag.format_context(hits)
    log(f"  retrieved {len(hits)} chunks (scope={scope or 'all'}):")
    for c, s in hits:
        log(f"    {s:7.2f}  {c.canon:6}  {c.citation}")

    # What the un-tweaked retriever would have pulled — recorded for the README's
    # before/after, and free, because retrieval costs nothing.
    naive = index.search(job.query, k=job.k, scope=None, boost=None)

    # -- generate -----------------------------------------------------------
    gen_prompt = GENERATOR_PROMPT.format(context=context, task=job.task)
    data = ask(engine, "generator", "generate", job.key, gen_prompt, job.validate)
    rows = job.validate(data)
    log(f"  generated {len(rows)} rows · gate A passed")

    # -- deterministic canon lint ------------------------------------------
    text_field = "Text" if "Text" in job.columns else "Callsign"
    lint = canon_lint([(r["RowName"], " ".join(str(r.get(c, "")) for c in job.columns))
                       for r in rows])
    if lint:
        log(f"  canon lint: {len(lint)} finding(s)")
        for f in lint:
            log(f"    {f['row']}: {f['quote']!r} — {f['why']}")

    # -- critic -------------------------------------------------------------
    before = {r["RowName"]: dict(r) for r in rows}
    review = ask(engine, "critic", "review", job.key,
                 CRITIC_PROMPT.format(context=context,
                                      content=json.dumps(rows, indent=1)),
                 validate=lambda d: d["verdict"] in ("PASS", "FINDINGS")
                 or (_ for _ in ()).throw(ValueError("verdict must be PASS or FINDINGS")))
    findings = list(review.get("findings", [])) + lint
    blocking = [f for f in findings if f.get("severity") == "high"]
    log(f"  critic: {review['verdict']} — {len(findings)} finding(s), "
        f"{len(blocking)} blocking")
    for f in findings:
        log(f"    [{f.get('severity','?'):6}] {f.get('kind','?'):11} {f.get('row','?')}: "
            f"{f.get('why','')[:100]}")

    # -- revise -------------------------------------------------------------
    if blocking:
        data = ask(engine, "generator", "revise", job.key,
                   REVISE_PROMPT.format(context=context,
                                        content=json.dumps(rows, indent=1),
                                        findings=json.dumps(blocking, indent=1)),
                   job.validate)
        rows = job.validate(data)
        relint = canon_lint([(r["RowName"], " ".join(str(r.get(c, "")) for c in job.columns))
                             for r in rows])
        if relint:
            log("  canon lint STILL failing after revision — not landing")
            for f in relint:
                log(f"    {f['row']}: {f['quote']!r} — {f['why']}")
            sys.exit(EXIT_NOT_LANDED)
        log(f"  revised · gate B passed · canon lint clean")

    after = {r["RowName"]: dict(r) for r in rows}
    changed = [rn for rn in before if rn in after and before[rn] != after[rn]]
    if changed:
        log(f"  corrected rows: {', '.join(changed)}")

    critic_records.append({
        "job": job.key, "title": job.title, "verdict": review["verdict"],
        "findings": findings, "blocking": len(blocking),
        "before": before, "after": after, "changed": changed,
    })
    traces.append({
        "job": job.key, "query": job.query, "scope": scope or "all",
        "boost": job.boost,
        "hits": [{"citation": c.citation, "canon": c.canon, "score": round(s, 2),
                  "heading": c.heading, "text": c.text} for c, s in hits],
        "naive_hits": [{"citation": c.citation, "canon": c.canon, "score": round(s, 2)}
                       for c, s in naive],
        "rows": rows,
    })
    return rows


def write_rag_trace(traces: list, path: Path):
    out = ["# RAG trace — query, retrieved chunk, generated output, side by side",
           "",
           "One section per content type. The chunk text below is *verbatim* what the",
           "generator received; the rows below it are what came back. Every citation is",
           "`file:line-line` against this repo, so any claim here can be opened and checked.",
           ""]
    for t in traces:
        out += [f"## {t['job']}", "",
                f"**Query** (`scope={t['scope']}`, boost `{t['boost'] or '{}'}`)", "",
                "```", t["query"], "```", "",
                "### Retrieved chunks", ""]
        for i, h in enumerate(t["hits"], 1):
            out += [f"<details open><summary><b>[{i}] {h['citation']}</b> — "
                    f"canon <code>{h['canon']}</code>, BM25 {h['score']} — "
                    f"{h['heading']}</summary>", "",
                    "```text", h["text"].rstrip(), "```", "", "</details>", ""]
        out += ["### Generated output (post-review, as landed)", "",
                "```json", json.dumps(t["rows"], indent=1), "```", ""]
        naive_only = [h["citation"] for h in t["naive_hits"]
                      if h["citation"] not in {x["citation"] for x in t["hits"]}]
        if naive_only:
            out += ["> **What the un-tweaked retriever would have returned instead:** "
                    + ", ".join(f"`{c}`" for c in naive_only)
                    + " — see README §The retrieval tweak.", ""]
    path.write_text("\n".join(out), encoding="utf-8")


def write_critic_log(records: list, path: Path):
    out = ["# Critic log — what was caught, and the correction",
           "",
           "`before` and `after` are the generator's own rows, captured on either side of",
           "the review. The diff is computed by the pipeline, not written by hand.",
           ""]
    for r in records:
        out += [f"## {r['job']} — verdict `{r['verdict']}`, "
                f"{len(r['findings'])} finding(s), {r['blocking']} blocking", ""]
        if not r["findings"]:
            out += ["No findings.", ""]
        for f in r["findings"]:
            out += [f"### `{f.get('row','?')}` — {f.get('kind','?')} "
                    f"({f.get('severity','?')})", "",
                    f"- **objected to:** `{f.get('quote','')}`"]
            if f.get("canon"):
                out += [f"- **canon cited:** {f['canon']}"]
            out += [f"- **why:** {f.get('why','')}"]
            if f.get("fix"):
                out += [f"- **proposed fix:** `{f['fix']}`"]
            out += [""]
        if r["changed"]:
            out += ["### Correction applied", "",
                    "| Row | Before | After |", "|---|---|---|"]
            for rn in r["changed"]:
                b, a = r["before"][rn], r["after"][rn]
                diffs = [k for k in b if b.get(k) != a.get(k)]
                for k in diffs:
                    out += [f"| `{rn}`.{k} | `{b.get(k)}` | `{a.get(k)}` |"]
            out += [""]
        else:
            out += ["_No blocking finding, so no row changed._", ""]
    path.write_text("\n".join(out), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--live", action="store_true", help="make real model calls")
    ap.add_argument("--dry-run", action="store_true", help="assemble prompts, don't run gates")
    ap.add_argument("--gaps", action="store_true", help="prove the gaps and exit")
    ap.add_argument("--job", choices=["announcer", "coach", "callsigns"],
                    help="run one job only")
    ap.add_argument("--scope", choices=["slice", "all"], default="slice",
                    help="retrieval canon scope (default slice; 'all' is the un-tweaked "
                         "retriever kept for the before/after)")
    ap.add_argument("--naive", action="store_true",
                    help="the retriever BEFORE the tweak: no canon scope and no "
                         "exemplar boost. Kept runnable so the README's before/after "
                         "is reproducible rather than remembered.")
    ap.add_argument("--model", default=DEFAULT_MODEL)
    ap.add_argument("--recording", default=str(DEFAULT_RECORDING))
    args = ap.parse_args()

    OUT.mkdir(exist_ok=True)
    recording = Path(args.recording)
    if not recording.is_absolute():
        recording = HERE / recording

    # -- step 0: prove the gaps --------------------------------------------
    if gapmod.repo_available():
        log("--- proving the content gaps from the repo ---")
        proven_list = gapmod.prove_all()
        proven = {g.key: g for g in proven_list}
        (OUT / "gap_report.md").write_text(gapmod.render(proven_list), encoding="utf-8")
        (OUT / "gap_report.json").write_text(gapmod.to_json(proven_list), encoding="utf-8")
        for g in proven_list:
            log(f"  {'PROVEN    ' if g.proven else 'NOT PROVEN'} {g.key}: {g.title}")
        if args.gaps:
            log(f"\nwrote {OUT / 'gap_report.md'}")
            return
        closed = [g.key for g in proven_list if not g.proven]
        if closed:
            log(f"\nerror: gap(s) {closed} no longer provable — the table exists. "
                f"Generating content for a filled gap is waste; re-cut the job.")
            sys.exit(EXIT_GAP_CLOSED)
    else:
        # Zip layout: no game repo to grep. Say so plainly — this run did NOT
        # re-prove anything, it is reading the record of a run that did.
        snapshot = OUT / "gap_report.json"
        if not snapshot.exists():
            sys.exit("error: no game repo to prove gaps against, and no committed "
                     "output/gap_report.json to fall back to")
        log("--- gaps NOT re-proven: no game repo in this layout ---")
        log(f"    reading the committed record: {snapshot.relative_to(HERE)}")
        data = json.loads(snapshot.read_text(encoding="utf-8"))
        proven = {}
        for key, d in data.items():
            g = gapmod.Gap(key=d["key"], title=d["title"], promise=d["promise"],
                           citation=d["citation"],
                           evidence=[gapmod.Evidence(**e) for e in d["evidence"]])
            if key == "announcer_coverage":
                g.orphans = d.get("orphans", [])   # type: ignore[attr-defined]
            proven[key] = g
            log(f"  recorded {key}: {d['title']}")
        if args.gaps:
            return

    # -- engine -------------------------------------------------------------
    if args.dry_run:
        engine = DryRunEngine(args.model, args.live, recording)
    elif args.live:
        engine = LiveEngine(args.model, recording)
    else:
        engine = ReplayEngine(recording)
    log(f"\nengine: {type(engine).__name__} · model {args.model} · "
        f"recording {recording.name}")

    index = rag.build_index(REPO)
    log(f"knowledge base: {len(index.chunks)} chunks from "
        f"{len({c.source for c in index.chunks})} sources "
        f"({sum(1 for c in index.chunks if c.canon == 'slice')} slice / "
        f"{sum(1 for c in index.chunks if c.canon == 'phase2')} phase2)")

    scope = None if (args.scope == "all" or args.naive) else "slice"
    jobs = [j for j in build_jobs(proven) if not args.job or j.key == args.job]
    if args.naive:
        for j in jobs:
            j.boost = {}
        log("retrieval: NAIVE (no canon scope, no exemplar boost)")

    traces, critic_records, landed = [], [], []
    try:
        for job in jobs:
            rows = run_job(engine, index, job, scope, traces, critic_records)
            path = OUT / job.out_file
            path.write_text(rows_to_csv(rows, job.columns), encoding="utf-8")
            landed.append((job, path, len(rows)))
            log(f"  LANDED {path.relative_to(HERE)} ({len(rows)} rows)")
    except DryRunComplete as e:
        log(f"\ndry run complete ({e}) — prompts assemble, wiring reachable")
        engine.save_recording()
        return

    engine.save_recording()
    write_rag_trace(traces, OUT / "rag_trace.md")
    write_critic_log(critic_records, OUT / "critic_log.md")

    # Only `high` blocks a landing (the project's rule — a reviewer that always
    # finds something otherwise lets nothing ship). The rest are real and must
    # not evaporate: they are carried to the human lead as open risks.
    open_risks = [{"job": r["job"], **f} for r in critic_records
                  for f in r["findings"] if f.get("severity") != "high"]
    (OUT / "open_risks.json").write_text(json.dumps(open_risks, indent=2),
                                         encoding="utf-8")

    log("\n--- landed ---")
    for job, path, n in landed:
        log(f"  {path.relative_to(HERE)}  {n} rows  — {job.title}")
    total_findings = sum(len(r["findings"]) for r in critic_records)
    total_changed = sum(len(r["changed"]) for r in critic_records)
    log(f"\ncritic: {total_findings} finding(s) across {len(critic_records)} job(s); "
        f"{total_changed} row(s) corrected before landing")
    if isinstance(engine, LiveEngine):
        t = summarize_usage(engine.exchanges)
        log(f"spend: {t['calls']} calls · {t['input_tokens']:,} in · "
            f"{t['output_tokens']:,} out · ${t['cost_usd']:.4f}")
    elif isinstance(engine, ReplayEngine) and engine.totals:
        t = engine.totals
        log(f"spend (recorded run): {t.get('calls')} calls · "
            f"{t.get('input_tokens', 0):,} in · {t.get('output_tokens', 0):,} out · "
            f"${t.get('cost_usd', 0):.4f}")
    log(f"\ntrace: output/rag_trace.md · review: output/critic_log.md · "
        f"gaps: output/gap_report.md")


if __name__ == "__main__":
    main()
