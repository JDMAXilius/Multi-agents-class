#!/usr/bin/env python3
"""BREACHPOINT dynamic content pipeline — Assignment #4.

    prove gaps -> retrieve -> generate a POOL -> gate -> JUDGE -> select top K
    -> REFUTE -> revise -> gate -> land

The two agents are the project's REAL crew definitions, loaded from
`breachpoint/.claude/agents/` and not copied: `curators/spotter.md` (the
authored owner of every line the game speaks) and `critic.md` (adversarial
reviewer, two modes). `CREW_MAP.md` already routes `DT_SpotterLines` + medals
to the spotter, so this pipeline drives the crew the project actually has
rather than a generic pair of prompts written for an assignment.

The shape comes from `CREW_PLAYBOOK.md` §13, which is the project's own
statement of Class 05's "generate 10, keep 3":

    Divergent jobs add ONE stage to the standard pipeline — generate -> score
    -> select — before the critic. The scorer is the critic in JUDGE mode
    ranking candidates; the REFUTER pass still runs on the survivors.

Three content types, each a hole `gaps.py` can prove from disk:

  1. DT_SpotterLines_Additions.csv  announcer lines for four medals that
                                    currently award in total silence
  2. DT_CoachLines.csv              the canned coach table §3.3 says ships in
                                    the build — keyed ONLY to telemetry the
                                    shipped struct actually records
  3. DT_BotCallsigns.csv            names for bots that fill up to 7 of 8 slots

Usage
-----
    python3 run_pipeline.py                   # replay the committed run (no key)
    python3 run_pipeline.py --gaps            # prove the gaps, call nothing
    python3 run_pipeline.py --dry-run --live  # probe wiring, one tiny call/agent
    python3 run_pipeline.py --live            # real calls, re-records
    python3 run_pipeline.py --live --naive --job announcer --recording recording_naive.json

Stdlib only. Live runs use ANTHROPIC_API_KEY + `pip install anthropic` if
present, otherwise the `claude` CLI on PATH.
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

import crew
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
# The shipped telemetry schema — the gate that the first run needed and lacked.
# ===========================================================================
#
# GDD Appendix C describes the telemetry BREACHPOINT WANTS ("accuracy per
# weapon, fights lost below 40% shields, grapple kills"). BRTelemetrySubsystem.h
# is the telemetry BREACHPOINT HAS. The first live run retrieved the prose,
# never saw the header, and shipped ten coach rows of which NINE keyed on stats
# the game does not record — precisely the failure spotter.md predicts:
#
#     "a coach line without a real predicate behind it is invented advice"
#
# This reads the real field list so the pipeline can no longer make that
# mistake, and so the failure is checkable rather than remembered.

TELEMETRY_HEADER = (REPO / "breachpoint" / "Source" / "Breachpoint" /
                    "Telemetry" / "BRTelemetrySubsystem.h")
UPROP_RE = re.compile(r"UPROPERTY\(\)\s*\n\s*[\w:<>\s\*]+?\b(\w+)\s*(?:=|;)")


def telemetry_fields() -> set[str]:
    """PascalCase field names the shipped telemetry structs actually record."""
    path = crew.SEARCH_ROOTS and TELEMETRY_HEADER
    if not path.exists():                       # zip layout
        mirrored = HERE / "kb" / TELEMETRY_HEADER.name
        if not mirrored.exists():
            return set()
        path = mirrored
    return set(UPROP_RE.findall(path.read_text(encoding="utf-8")))


def _norm(name: str) -> str:
    """Case- and separator-insensitive form, so `self_inflicted_deaths`,
    `SelfInflictedDeaths` and `Self_Inflicted_Deaths` all compare equal.

    An earlier version normalised by `"".join(p.capitalize() for p in
    name.split("_"))`, which is correct for snake_case input and silently
    WRONG for input that is already PascalCase — `SelfInflictedDeaths` became
    `Selfinflicteddeaths` and failed to match itself. It cost three of six
    coach slots in a live run and nothing complained, because a dropped slot
    just meant a smaller table. Hence `log_dropped_slots` below: a constraint
    that removes work has to say so.
    """
    return name.replace("_", "").lower()


def telemetry_field_exists(declared: str, fields: set[str]) -> bool:
    """Whole-name match against the shipped struct, tolerating the `b` prefix
    UE puts on bools.

    Deliberately strict about *whole names*: a substring match would pass
    `melee_kills_rear` against `Kills`, which is how nine bad rows survived the
    first run's review.
    """
    if not fields:
        return True                              # cannot check; do not pretend to
    want = _norm(declared)
    return any(_norm(f) == want or _norm(f) == f"b{want}" for f in fields)


# ===========================================================================
# Canon lint — the deterministic half of consistency checking.
# ===========================================================================
#
# The LLM critic argues about meaning; this argues about vocabulary and cannot
# be talked out of a finding. Both lists come from the GDD: CUT is §6's cut
# table, FOREIGN is genre vocabulary from the games BREACHPOINT gets compared
# to. spotter.md adds the narrative ban — "no lore, no fiction, no characters".

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
    r"\bspartan\b|\bcovenant\b|\bmaster chief\b|\bkilltacular\b":
        "another game's IP, not BREACHPOINT's",
    r"\bheadshot!\b|\bboom\b|\bnice shot\b|\bgg\b|\bnoob\b":
        "chat-speak; the announcer is a clipped military callout",
}

# spotter.md: "No lore, no fiction, no characters — Breachpoint has no
# narrative and inventing one is a finding against you, not colour."
INVENTED_NARRATIVE = {
    r"\bthe (?:order|covenant|syndicate|corporation|company|collective)\b":
        "spotter.md — inventing a faction is inventing narrative",
    r"\bcenturies\b|\blegend\b|\bprophec\w+\b|\bancient\b":
        "spotter.md — BREACHPOINT has no fiction to draw on",
}


def canon_lint(texts: list[tuple[str, str]]) -> list[dict]:
    """texts: [(row_id, text)] -> findings. Deterministic, no model involved."""
    findings = []
    rules = {**CUT_SYSTEMS, **FOREIGN_VOCAB, **INVENTED_NARRATIVE}
    for row_id, text in texts:
        low = text.lower()
        for pattern, why in rules.items():
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
                model=self.model, max_tokens=16000,
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
                capture_output=True, text=True, timeout=1200, env=env)
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

    Everything except the model executes for real on replay: retrieval, both
    gates, the canon lint, the telemetry-field check, the judge's selection
    arithmetic, the bounce loop and CSV assembly. That is the point — the
    pipeline has to be demonstrably runnable with no API key, and "runs" has to
    mean more than "prints the committed answer".
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
        got = (ex["agent"], ex["stage"], ex.get("job"))
        want = (meta["agent"], meta["stage"], meta.get("job"))
        if got != want:
            sys.exit(f"error: replay mismatch — recorded {got}, pipeline asked for {want}")
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
        key = f"{meta['agent']}/{meta['stage']}"
        if key not in self.seen:
            self.seen.add(key)
            if self.inner is not None:
                reply = self.inner.call(
                    f"Connectivity probe for the '{key}' slot. Reply with one word.", meta)
                log(f"  dry-run: {key:18} prompt {len(prompt):6,d} chars · "
                    f"round trip {'OK' if reply.strip() else 'EMPTY — check auth/model'}")
            else:
                log(f"  dry-run: {key:18} prompt {len(prompt):6,d} chars · assembled")
        raise DryRunComplete(key)

    def save_recording(self):
        pass


def _which(cmd):
    from shutil import which
    return which(cmd)


# Claude Sonnet list prices, $ per million tokens. Cache reads are the whole reason
# this pipeline is affordable, so they are priced as their own line, not folded into
# input. ONE rate table, used by the recorder, the replayer and the on-screen total.
PRICE_PER_M = {"input": 3.00, "output": 15.00, "cache_write": 3.75, "cache_read": 0.30}


def summarize_usage(exchanges: list) -> dict:
    """Price a run from the per-call `usage` blocks the API returned.

    WHY THIS RECOMPUTES INSTEAD OF SUMMING A STORED cost_usd (3 Sep):
    it used to add up each exchange's own `cost_usd` field, which was written without
    itemising cache tokens -- `totals` in recording.json carries input/output/cost and
    no cache counts at all. For the committed #4 run that summed to $4.1760, while the
    same recording's usage blocks (80 in / 145,942 out / 211,096 cache-write /
    2,129,120 cache-read) price to $3.6197 at the rates above. Two numbers for one run,
    and the pipeline printed the one the audit does not use.

    The usage blocks are the ground truth -- they are what the API reported per call --
    so they are what gets priced here. The figure is LOWER than the old one, which is
    the direction that deserves the most scrutiny, so the breakdown is returned and
    printed itemised: anyone can multiply the four token counts by the four published
    rates and land on the same total.
    """
    tot = {"input_tokens": 0, "output_tokens": 0, "cache_write_tokens": 0,
           "cache_read_tokens": 0, "cost_usd": 0.0, "calls": len(exchanges)}
    for ex in exchanges:
        u = ex.get("usage") or {}
        tot["input_tokens"] += u.get("input_tokens", 0)
        tot["output_tokens"] += u.get("output_tokens", 0)
        tot["cache_write_tokens"] += u.get("cache_creation_input_tokens", 0)
        tot["cache_read_tokens"] += u.get("cache_read_input_tokens", 0)
    tot["cost_usd"] = (
        tot["input_tokens"] * PRICE_PER_M["input"]
        + tot["output_tokens"] * PRICE_PER_M["output"]
        + tot["cache_write_tokens"] * PRICE_PER_M["cache_write"]
        + tot["cache_read_tokens"] * PRICE_PER_M["cache_read"]
    ) / 1_000_000
    return tot


# ===========================================================================
# Model I/O
# ===========================================================================

def extract_json(text: str):
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
    exactly which invariant it broke. The budget is 1 — a second failure is a
    prompt bug, and looping on it only spends money.
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
        except (ValueError, KeyError, AssertionError, TypeError) as e:
            last_err = str(e)
            log(f"        gate rejected ({attempt + 1}/{retries + 1}): {last_err[:170]}")
            attempt += 1
    sys.exit(f"error: {agent}/{stage} failed validation twice: {last_err}")


# ===========================================================================
# Jobs
# ===========================================================================

@dataclass
class Slot:
    id: str            # the trigger / predicate / profile this pool is for
    brief: str         # what the line must convey


@dataclass
class Job:
    key: str
    gap_key: str
    title: str
    out_file: str
    query: str
    slots: list[Slot]
    columns: list[str]
    to_row: Callable            # (job, cand, slot_index, rank, seq) -> dict
    char_cap: int
    pool_per_slot: int = 10     # playbook §13: "generate N (~10)"
    keep_per_slot: int = 3      #               "keep the best K (~3)"
    boost: dict = field(default_factory=dict)
    k: int = 7
    extra_fields: list[str] = field(default_factory=list)
    task: str = ""
    check_candidate: Callable | None = None   # job-specific rule, candidate shape
    check_row: Callable | None = None         # the SAME rule, landed-row shape


CANDIDATE_KEYS = ["id", "slot", "text", "tone", "char_count", "sources", "risk", "doubts"]


def make_candidate_validator(job: Job):
    """The candidate record shape is spotter.md's, not one invented here:

        { table, slot, text, tone, char_count, sources[], risk, doubts[] }

    `sources[]` is the part that matters for RAG: every candidate must name
    which retrieved chunks it drew on, so an unsourced line is a gate failure
    rather than something a reviewer has to notice.
    """
    slot_ids = {s.id for s in job.slots}

    def validate(data):
        cands = data["candidates"] if isinstance(data, dict) else data
        if not isinstance(cands, list) or not cands:
            raise ValueError("expected a non-empty 'candidates' list")
        seen_ids = set()
        by_slot: dict[str, int] = {s: 0 for s in slot_ids}
        for c in cands:
            missing = [k for k in CANDIDATE_KEYS + job.extra_fields if k not in c]
            if missing:
                raise ValueError(f"candidate {c.get('id', '?')} missing keys {missing}")
            if c["id"] in seen_ids:
                raise ValueError(f"duplicate candidate id {c['id']!r}")
            seen_ids.add(c["id"])
            if c["slot"] not in slot_ids:
                raise ValueError(f"candidate {c['id']} has unknown slot {c['slot']!r}; "
                                 f"valid slots are {sorted(slot_ids)}")
            by_slot[c["slot"]] += 1
            text = str(c["text"])
            if len(text) > job.char_cap:
                raise ValueError(f"candidate {c['id']} is {len(text)} chars, over "
                                 f"spotter.md's {job.char_cap}-char cap: {text!r}")
            if not isinstance(c["sources"], list) or not c["sources"]:
                raise ValueError(f"candidate {c['id']} cites no sources[] — "
                                 f"spotter.md requires every candidate to name what "
                                 f"it drew on")
            if job.check_candidate:
                job.check_candidate(c)
        short = {s: n for s, n in by_slot.items() if n < job.keep_per_slot}
        if short:
            raise ValueError(f"slots {short} have fewer candidates than the "
                             f"{job.keep_per_slot} that must be kept; playbook §13 "
                             f"wants ~{job.pool_per_slot} per slot — one option is "
                             f"not a choice")
        return cands
    return validate


def make_ranking_validator(job: Job, cands: list[dict]):
    ids_by_slot: dict[str, set] = {}
    for c in cands:
        ids_by_slot.setdefault(c["slot"], set()).add(c["id"])

    def validate(data):
        rankings = data["rankings"] if isinstance(data, dict) else data
        got = {r["slot"] for r in rankings}
        want = set(ids_by_slot)
        if got != want:
            raise ValueError(f"rankings cover {sorted(got)}, expected {sorted(want)}")
        for r in rankings:
            ordered = r["ordered_ids"]
            unknown = [i for i in ordered if i not in ids_by_slot[r["slot"]]]
            if unknown:
                raise ValueError(f"slot {r['slot']}: unknown candidate ids {unknown}")
            if len(set(ordered)) != len(ordered):
                raise ValueError(f"slot {r['slot']}: repeated ids in ordered_ids")
            if len(ordered) < job.keep_per_slot:
                raise ValueError(f"slot {r['slot']}: ranked {len(ordered)}, need at "
                                 f"least {job.keep_per_slot}")
        return rankings
    return validate


# --- job 1: announcer lines for the four silent medals ---------------------

ANNOUNCER_COLS = ["RowName", "TriggerId", "Text", "Audience", "Weight", "RepeatCooldown_s"]
ROWNAME_BASE = 22   # the shipped table ends at S21


def announcer_row(job, cand, slot_index, rank, seq):
    return {"RowName": f"S{ROWNAME_BASE + slot_index}{'abcdefgh'[rank]}",
            "TriggerId": cand["slot"], "Text": cand["text"],
            "Audience": cand.get("audience", "Self"), "Weight": "1.0",
            "RepeatCooldown_s": cand.get("repeat_cooldown_s", "20")}


def check_announcer(c):
    if "!" in str(c["text"]):
        raise ValueError(f"candidate {c['id']}: exclamation mark — the 63 shipped "
                         f"lines contain zero")
    if c.get("audience") not in ("Self", "Team", "All"):
        raise ValueError(f"candidate {c['id']}: audience {c.get('audience')!r} "
                         f"not in Self/Team/All")
    if str(c.get("repeat_cooldown_s")) not in ("0", "8", "20"):
        raise ValueError(f"candidate {c['id']}: repeat_cooldown_s "
                         f"{c.get('repeat_cooldown_s')!r} is not one of the shipped "
                         f"values 0/8/20")


# --- job 2: the canned coach fallback table --------------------------------

COACH_COLS = ["RowName", "ConditionId", "TelemetryField", "Comparison", "Threshold",
              "Text", "Priority"]


def coach_row(job, cand, slot_index, rank, seq):
    return {"RowName": f"C{seq:02d}", "ConditionId": cand["condition_id"],
            "TelemetryField": cand["telemetry_field"],
            "Comparison": cand.get("comparison", ">="),
            "Threshold": cand["threshold"], "Text": cand["text"],
            "Priority": seq}


def _coach_invariants(who: str, field_name: str, text: str, comparison: str,
                      fields: set[str]):
    """The coach rules, in ONE place, so generation and revision cannot diverge.

    The first version of this file expressed these only over the candidate
    shape, and the revision gate re-checked columns and length but not this.
    The critic then (correctly) told the spotter to rewrite every
    `TelemetryField` to the exact PascalCase UPROPERTY name, the spotter changed
    the column and left the `{snake_case}` token in the text, and all twelve
    rows landed with the declared field and the substitution token disagreeing.
    A revision gate weaker than the generation gate is not a gate; it is a
    window.
    """
    if not telemetry_field_exists(field_name, fields):
        raise ValueError(
            f"{who}: TelemetryField {field_name!r} is NOT recorded by "
            f"FBRPlayerMatchTelemetry / FBRMatchTelemetryRecord. spotter.md: "
            f"'a coach line without a real predicate behind it is invented "
            f"advice'. Recorded fields: {sorted(fields)}")
    token = "{" + field_name + "}"
    if token not in str(text):
        raise ValueError(
            f"{who}: text must substitute {token} — the token has to be spelled "
            f"exactly as TelemetryField, because the runtime resolves the field "
            f"by that name and UE's property lookup is case-sensitive. Text was: "
            f"{text!r}")
    if comparison not in (">=", "<=", ">", "<", "=="):
        raise ValueError(f"{who}: comparison {comparison!r} is not an operator")


def make_coach_check(fields: set[str]):
    def check(c):
        _coach_invariants(f"candidate {c['id']}", c["telemetry_field"], c["text"],
                          c.get("comparison"), fields)
    return check


def make_coach_row_check(fields: set[str]):
    def check(r):
        _coach_invariants(f"row {r['RowName']}", r["TelemetryField"], r["Text"],
                          r.get("Comparison"), fields)
    return check


# --- job 3: bot callsigns --------------------------------------------------

CALLSIGN_COLS = ["RowName", "Callsign", "ProfileHint", "Note"]


def callsign_row(job, cand, slot_index, rank, seq):
    return {"RowName": f"B{seq:02d}", "Callsign": cand["text"],
            "ProfileHint": cand["slot"], "Note": cand.get("note", "")}


def check_callsign(c):
    cs = str(c["text"])
    if " " in cs:
        raise ValueError(f"candidate {c['id']}: {cs!r} contains a space — the "
                         f"killfeed column is narrow")
    if not cs.isalpha():
        raise ValueError(f"candidate {c['id']}: {cs!r} is not plain letters "
                         f"(no digits, no leetspeak)")


def build_jobs(proven: dict, fields: set[str]) -> list[Job]:
    orphans = getattr(proven["announcer_coverage"], "orphans", [])

    announcer_slots = [
        Slot(id=r["TriggerId"],
             brief=f"{r['MedalName']} — awarded when: {r['Description']}")
        for r in orphans]

    # Coach predicates are drawn from the SHIPPED struct, not Appendix C. If a
    # field is not recorded, no slot exists for it — the constraint is applied
    # when the job is built, not argued about after the model has written.
    coach_candidates = [
        ("LowKillDeathRatio", "Deaths", "the player died far more than they killed"),
        ("HighAssistsFewKills", "Assists",
         "the player softened targets but let others finish"),
        ("SelfInflicted", "SelfInflictedDeaths",
         "the player killed themselves with their own splash"),
        ("FriendlyFire", "FriendlyFireKills", "the player damaged their own team"),
        ("ShortMatchTime", "TimeInMatchSeconds",
         "the player spent little of the match alive and in play"),
        ("LowKills", "Kills", "the player finished few fights"),
    ]
    coach_slots, dropped = [], []
    for cid, fld, brief in coach_candidates:
        if telemetry_field_exists(fld, fields):
            coach_slots.append(Slot(id=cid, brief=f"{brief} (keys on `{fld}`)"))
        else:
            dropped.append((cid, fld))
    if dropped:
        # Never silent. A gate that shrinks the deliverable has to announce it,
        # or "the table is small" and "the matcher is broken" look identical.
        log(f"  coach: {len(dropped)} predicate(s) dropped — field not recorded by "
            f"the shipped telemetry struct:")
        for cid, fld in dropped:
            log(f"    {cid} (wanted `{fld}`)")

    callsign_slots = [
        Slot(id="Recruit", brief="the dulled profile — 500 ms reaction, 25% accuracy, "
                                 "rare grenades"),
        Slot(id="Marine", brief="the baseline profile — 320 ms, 45%, situational"),
        Slot(id="Veteran", brief="the sharpened profile — 220 ms, 65%, tactical"),
    ]

    return [
        Job(key="announcer", gap_key="announcer_coverage",
            title="Announcer lines for the four medals that award in silence",
            out_file="DT_SpotterLines_Additions.csv",
            query=("announcer callout line medal killfeed rocket multi kill first kill "
                   "of the match sudden death killing spree ended clipped military voice"),
            boost={"DT_SpotterLines.csv": 2.5, "DT_Medals.csv": 2.0}, k=7,
            slots=announcer_slots, columns=ANNOUNCER_COLS, to_row=announcer_row,
            char_cap=48, pool_per_slot=10, keep_per_slot=3,
            extra_fields=["audience", "repeat_cooldown_s"],
            check_candidate=check_announcer,
            task="""Write announcer lines for four medals that currently award with NO
audio line. The winners append to the shipped `DT_SpotterLines.csv`.

Per candidate, add to the record:
  audience            Self | Team | All — match how the shipped table scopes
                      comparable events (look at what it does for Kill.* rows)
  repeat_cooldown_s   one of the shipped values 0, 8 or 20 — again, copy how
                      the shipped table treats comparable events

`text` is the spoken line. Do NOT invent a RowName; the pipeline assigns it."""),

        Job(key="coach", gap_key="coach_fallback",
            title="The canned coach table, keyed only to telemetry the game records",
            out_file="DT_CoachLines.csv",
            query=("telemetry fields recorded per player match kills deaths assists "
                   "self inflicted friendly fire time in match spotter coach line "
                   "canned fallback no connectivity"),
            boost={"BRTelemetrySubsystem.h": 3.0}, k=8,
            slots=coach_slots, columns=COACH_COLS, to_row=coach_row,
            char_cap=140, pool_per_slot=8, keep_per_slot=2,
            extra_fields=["condition_id", "telemetry_field", "threshold", "comparison"],
            check_candidate=None,   # installed below, needs the field set
            task="""Write the canned coach-line table the GDD says ships in the build so
that with no connectivity "the game is identical minus flavor."

Each row is a match-end coaching line selected by a telemetry condition, with
the stat substituted in at runtime.

**The retrieved `BRTelemetrySubsystem.h` chunk is the ONLY valid source of
field names.** GDD Appendix C describes telemetry the project intends to
collect one day; the header is what the build records today. A line keyed to a
stat the struct does not carry is invented advice and is rejected at gate.

Per candidate, add to the record:
  condition_id     short PascalCase id for the predicate
  telemetry_field  the field's EXACT UPROPERTY identifier, copied character for
                   character from the header — `SelfInflictedDeaths`, not
                   `self_inflicted_deaths`. UE resolves properties by name and
                   the lookup is case-sensitive, so a re-cased field is a row
                   that can never fire.
  comparison       one of >= <= > < ==
  threshold        the number the field is compared against to fire this row

`text` is the coach line and MUST substitute {telemetry_field} spelled exactly
as you declared it — `{SelfInflictedDeaths}`, matching the column. Name the
mistake AND the fix; "play better" is noise.

Two rows share each slot, so make the pair genuinely different reads of the
same stat rather than one line said twice."""),

        Job(key="callsigns", gap_key="bot_callsigns",
            title="Callsigns for the bots that fill up to seven of eight slots",
            out_file="DT_BotCallsigns.csv",
            query=("bots fill every unfilled slot difficulty profiles Recruit Marine "
                   "Veteran reaction accuracy cover preference killfeed scoreboard"),
            boost={"DT_BotTuning.csv": 2.5}, k=6,
            slots=callsign_slots, columns=CALLSIGN_COLS, to_row=callsign_row,
            char_cap=12, pool_per_slot=10, keep_per_slot=5,
            extra_fields=["note"], check_candidate=check_callsign,
            task="""Bots fill every unfilled slot, so in solo play seven of the eight
names in the killfeed are bots. They currently have no names at all.

One pool per DT_BotTuning profile. `text` is the callsign itself: it appears in
the killfeed beside human Steam names, so it must read as a squad callsign, not
a username and not an obvious robot. MAX 12 characters, letters only, no
spaces, no digits, no leetspeak.

Per candidate, add to the record:
  note   one short line: which numbers in the retrieved DT_BotTuning row make
         this callsign fit this profile. Cite the value, not a vibe.

These are callsigns, not characters. spotter.md: no lore, no fiction, no
narrative — a callsign implying a backstory is a finding."""),
    ]


# ===========================================================================
# Prompts — the doctrine is the crew's, loaded from disk
# ===========================================================================

GENERATE_PROMPT = """{doctrine}

---

# THIS RUN

You are operating inside an automated pipeline. Return JSON only — no
commentary, no code fence.

## HARD RULE — grounding
Everything you know about BREACHPOINT must come from the RETRIEVED CONTEXT
below. It is the game's real design document, its shipped data tables and its
shipped code. If a detail you want is not in the context, you do not have it:
leave it out rather than filling it in from other shooters you know. A fact in
your output that is not in the context is a defect, and this pipeline checks
for exactly that.

## RETRIEVED CONTEXT
{context}

## SLOTS — generate a POOL for each
{slots}

Generate **{pool} candidates per slot** ({total} total). Your doctrine is
explicit that one option is not a choice: the variation must be real, not
cosmetic rephrasing of the same line. A later stage ranks them and keeps
{keep} per slot.

**Hard length cap: {cap} characters per `text`.** Count before returning;
over-length candidates are rejected at gate.

## TASK
{task}

## RECORD SHAPE — one object per candidate
{{"candidates": [
  {{"id": "<slot>-01", "slot": "<slot id, copied exactly>",
    "text": "<the content>", "tone": "<one word>",
    "char_count": <int, the true length of text>,
    "sources": ["<citation of a retrieved chunk you used>", ...],
    "risk": "<the way this candidate could be wrong, or 'none'>",
    "doubts": ["<anything you are unsure of>"]{extra}}}
]}}

`sources` may not be empty — a candidate that cites nothing was not retrieved
from, it was remembered. `doubts` may be empty, but your doctrine says a
flagged doubt beats a bland line.
"""

JUDGE_PROMPT = """{doctrine}

---

# THIS RUN — JUDGE MODE

You are scoring a pool of generated candidates, not attacking one artifact.
`CREW_PLAYBOOK.md` §13: divergent work is generate -> score -> select, and you
are the scorer. Rank every slot's candidates best-first. The top {keep} per
slot survive to the REFUTER pass.

Return JSON only.

## THE CANON YOU JUDGE AGAINST
{context}

## WHAT "BEST" MEANS HERE, IN ORDER
1. **Fit to the shipped voice.** The existing rows in the retrieved tables are
   the specification. A candidate that would not sit comfortably beside them
   loses to one that would, even if it is cleverer.
2. **Correctness against canon.** A candidate that misstates a number, names a
   place the arena manifest never named, or implies a system the slice cut,
   ranks last regardless of how it reads.
3. **Real variation.** Within a slot, three near-identical lines waste two of
   the three. Prefer a top-{keep} that differ from each other in structure, not
   just wording.
4. **Length.** Shorter wins ties. These are read mid-fight.

## THE POOL
{pool}

## OUTPUT
{{"rankings": [
  {{"slot": "<slot id>",
    "ordered_ids": ["<best>", "<next>", ...],   // ALL candidates for the slot, best first
    "why_top": "<one sentence: why the winner beat the runner-up>",
    "rejected_outright": [{{"id": "<id>", "reason": "<what is wrong with it>"}}]
  }}
]}}
"""

REFUTE_PROMPT = """{doctrine}

---

# THIS RUN — REFUTER MODE

The pool has been ranked and the winners selected. Your job now is to find what
is wrong with the survivors, not to appreciate them. A review that says "looks
good" when a defect is present is the failure mode this role exists to prevent.

You judge against the RETRIEVED CANON below and nothing else. Your knowledge of
other shooters is not evidence — if the canon does not say it, the canon does
not say it.

Return JSON only.

## RETRIEVED CANON
{context}

## THE SELECTED CONTENT UNDER REVIEW
{content}

## WHAT COUNTS AS A FINDING
1. **lore-break** — states or implies something the canon contradicts, misstates
   a value the canon carries, names a place the arena manifest never named, or
   references a system the canon says is cut. Highest severity. Quote the canon
   line you are judging against.
2. **tone-drift** — does not sound like the shipped rows: wrong length, wrong
   register, exclamation marks, chattiness, jokes, marketing voice, invented
   narrative, or generic-shooter phrasing.
3. **schema-risk** — would break the game or the table on import.
4. **redundancy** — two surviving rows in the same slot say the same thing, so
   the variation they exist to provide does not exist.

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

Only `high` findings block a landing.
"""

REVISE_PROMPT = """{doctrine}

---

# THIS RUN — REVISION

Your selected rows were reviewed and the critic raised blocking findings. Apply
them. Return JSON only.

## RETRIEVED CONTEXT (unchanged)
{context}

## YOUR SELECTED ROWS
{content}

## BLOCKING FINDINGS TO APPLY
{findings}

Rules:
- Change ONLY the rows named in the findings. Every other row must come back
  byte-identical — silent drift in unreviewed rows is its own bug class.
- Keep the schema and the column set exactly as they are.
- Respect the {cap}-character cap on the content field.
- If you believe a finding is wrong, still return a row that satisfies the
  canon; disagreement is settled by evidence, not by ignoring review.

Return the COMPLETE row set as {{"rows": [ ... ]}}.
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

def run_job(engine, index: rag.Index, job: Job, scope: str | None,
            doctrine: dict, traces: list, critic_records: list,
            judge_records: list) -> list[dict]:
    log(f"\n=== {job.key}: {job.title}")
    if not job.slots:
        log("  no slots — every predicate this job wanted is unrecorded. Skipped.")
        return []

    # -- retrieve -----------------------------------------------------------
    hits = index.search(job.query, k=job.k, scope=scope, boost=job.boost)
    if not hits:
        sys.exit(f"error: retrieval returned nothing for {job.key}")
    context = rag.format_context(hits)
    log(f"  retrieved {len(hits)} chunks (scope={scope or 'all'}):")
    for c, s in hits:
        log(f"    {s:7.2f}  {c.canon:6}  {c.citation}")
    naive = index.search(job.query, k=job.k, scope=None, boost=None)

    slot_block = "\n".join(f"  - `{s.id}` — {s.brief}" for s in job.slots)
    total = job.pool_per_slot * len(job.slots)
    extra = ("".join(f', "{f}": "<...>"' for f in job.extra_fields)
             if job.extra_fields else "")

    # -- generate the pool --------------------------------------------------
    validate_c = make_candidate_validator(job)
    pool = ask(engine, "spotter", "generate", job.key,
               GENERATE_PROMPT.format(
                   doctrine=doctrine["spotter"], context=context, slots=slot_block,
                   pool=job.pool_per_slot, total=total, keep=job.keep_per_slot,
                   cap=job.char_cap, task=job.task, extra=extra),
               validate_c)
    cands = validate_c(pool)
    log(f"  pool: {len(cands)} candidates across {len(job.slots)} slots "
        f"(~{job.pool_per_slot} each) · gate A passed")

    # -- JUDGE: rank, then the PIPELINE selects -----------------------------
    validate_r = make_ranking_validator(job, cands)
    ranked = ask(engine, "critic", "judge", job.key,
                 JUDGE_PROMPT.format(doctrine=doctrine["critic"], context=context,
                                     keep=job.keep_per_slot,
                                     pool=json.dumps(cands, indent=1)),
                 validate_r)
    rankings = validate_r(ranked)

    by_id = {c["id"]: c for c in cands}
    slot_order = {s.id: i for i, s in enumerate(job.slots)}
    winners, seq = [], 0
    for r in sorted(rankings, key=lambda r: slot_order[r["slot"]]):
        for rank, cid in enumerate(r["ordered_ids"][:job.keep_per_slot]):
            seq += 1
            winners.append(job.to_row(job, by_id[cid], slot_order[r["slot"]], rank, seq))
    log(f"  judged · kept {len(winners)} of {len(cands)} "
        f"({job.keep_per_slot} per slot)")
    for r in rankings:
        log(f"    {r['slot']}: {r['ordered_ids'][0]} won — {r.get('why_top','')[:90]}")

    judge_records.append({
        "job": job.key, "pool_size": len(cands), "kept": len(winners),
        "candidates": cands, "rankings": rankings})

    # -- deterministic canon lint on the survivors --------------------------
    def lint_rows(rows):
        return canon_lint([(r["RowName"], " ".join(str(r.get(c, "")) for c in job.columns))
                           for r in rows])

    # The landed-row invariant runs on the winners too, not only after revision —
    # a rule that only fires on one path is a rule with a hole in it.
    if job.check_row:
        for r in winners:
            job.check_row(r)

    lint = lint_rows(winners)
    if lint:
        log(f"  canon lint: {len(lint)} finding(s)")
        for f in lint:
            log(f"    {f['row']}: {f['quote']!r} — {f['why']}")

    # -- REFUTER on the survivors -------------------------------------------
    before = {r["RowName"]: dict(r) for r in winners}
    review = ask(engine, "critic", "refute", job.key,
                 REFUTE_PROMPT.format(doctrine=doctrine["critic"], context=context,
                                      content=json.dumps(winners, indent=1)),
                 validate=lambda d: d["verdict"] in ("PASS", "FINDINGS")
                 or (_ for _ in ()).throw(ValueError("verdict must be PASS or FINDINGS")))
    findings = list(review.get("findings", [])) + lint
    blocking = [f for f in findings if f.get("severity") == "high"]
    log(f"  refuted: {review['verdict']} — {len(findings)} finding(s), "
        f"{len(blocking)} blocking")
    for f in findings:
        log(f"    [{f.get('severity','?'):6}] {f.get('kind','?'):11} "
            f"{f.get('row','?')}: {f.get('why','')[:95]}")

    # -- revise -------------------------------------------------------------
    rows = winners
    if blocking:
        def validate_rows(data):
            rs = data["rows"] if isinstance(data, dict) else data
            if len(rs) != len(winners):
                raise ValueError(f"expected {len(winners)} rows back, got {len(rs)}")
            for r in rs:
                missing = [c for c in job.columns if c not in r]
                if missing:
                    raise ValueError(f"row {r.get('RowName')} missing {missing}")
                field_name = "Text" if "Text" in job.columns else "Callsign"
                if len(str(r[field_name])) > job.char_cap:
                    raise ValueError(f"row {r['RowName']} is over the "
                                     f"{job.char_cap}-char cap")
                if job.check_row:
                    job.check_row(r)      # the same invariant the pool had to pass
            return rs
        revised = ask(engine, "spotter", "revise", job.key,
                      REVISE_PROMPT.format(doctrine=doctrine["spotter"], context=context,
                                           content=json.dumps(winners, indent=1),
                                           findings=json.dumps(blocking, indent=1),
                                           cap=job.char_cap),
                      validate_rows)
        rows = validate_rows(revised)
        relint = lint_rows(rows)
        if relint:
            log("  canon lint STILL failing after revision — not landing")
            for f in relint:
                log(f"    {f['row']}: {f['quote']!r} — {f['why']}")
            sys.exit(EXIT_NOT_LANDED)
        log("  revised · gate B passed · canon lint clean")

    after = {r["RowName"]: dict(r) for r in rows}
    changed = [rn for rn in before if rn in after and before[rn] != after[rn]]
    if changed:
        log(f"  corrected rows: {', '.join(changed)}")

    critic_records.append({
        "job": job.key, "title": job.title, "verdict": review["verdict"],
        "findings": findings, "blocking": len(blocking),
        "before": before, "after": after, "changed": changed})
    traces.append({
        "job": job.key, "query": job.query, "scope": scope or "all", "boost": job.boost,
        "hits": [{"citation": c.citation, "canon": c.canon, "score": round(s, 2),
                  "heading": c.heading, "text": c.text} for c, s in hits],
        "naive_hits": [{"citation": c.citation, "canon": c.canon} for c, _ in naive],
        "rows": rows,
        "sources_cited": sorted({s for c in cands for s in c.get("sources", [])})})
    return rows


# ===========================================================================
# Writers
# ===========================================================================

def write_rag_trace(traces: list, path: Path):
    out = ["# RAG trace — query, retrieved chunk, generated output, side by side", "",
           "One section per content type. The chunk text below is *verbatim* what the",
           "spotter received; the rows beneath are what survived judging and review.",
           "Every citation is `file:line-line` against this repo, so any claim here can",
           "be opened and checked.", ""]
    for t in traces:
        out += [f"## {t['job']}", "",
                f"**Query** (`scope={t['scope']}`, boost `{t['boost'] or '{}'}`)", "",
                "```", t["query"], "```", "", "### Retrieved chunks", ""]
        for i, h in enumerate(t["hits"], 1):
            out += [f"<details open><summary><b>[{i}] {h['citation']}</b> — canon "
                    f"<code>{h['canon']}</code>, BM25 {h['score']} — {h['heading']}"
                    f"</summary>", "", "```text", h["text"].rstrip(), "```", "",
                    "</details>", ""]
        out += ["### Sources the spotter cited across its pool", "",
                *[f"- `{s}`" for s in t["sources_cited"]], "",
                "### Generated output (post-judge, post-review, as landed)", "",
                "```json", json.dumps(t["rows"], indent=1), "```", ""]
        naive_only = [h["citation"] for h in t["naive_hits"]
                      if h["citation"] not in {x["citation"] for x in t["hits"]}]
        if naive_only:
            out += ["> **What the un-tweaked retriever would have returned instead:** "
                    + ", ".join(f"`{c}`" for c in naive_only)
                    + " — see README §The retrieval tweak.", ""]
    path.write_text("\n".join(out), encoding="utf-8")


def write_judge_log(records: list, path: Path):
    out = ["# Judge log — the pool, the ranking, and what was left behind", "",
           "`CREW_PLAYBOOK.md` §13: divergent work is **generate -> score -> select**.",
           "The spotter generates ~10 per slot, the critic in JUDGE mode ranks them, and",
           "**the pipeline** — not the model — slices the top K. That last part matters:",
           "selection is arithmetic over a ranking, so it cannot drift.", ""]
    for r in records:
        out += [f"## {r['job']} — {r['pool_size']} candidates generated, "
                f"{r['kept']} kept", ""]
        for rank in r["rankings"]:
            ordered = rank["ordered_ids"]
            out += [f"### slot `{rank['slot']}`", "",
                    f"**Winner:** `{ordered[0]}` — {rank.get('why_top','')}", "",
                    "| # | id | text | kept |", "|---:|---|---|---|"]
            by_id = {c["id"]: c for c in r["candidates"]}
            keep_n = r["kept"] // max(1, len(r["rankings"]))
            for i, cid in enumerate(ordered):
                c = by_id.get(cid, {})
                text = str(c.get("text", "")).replace("|", r"\|")
                kept = "✅" if i < keep_n else ""
                out.append(f"| {i+1} | `{cid}` | {text} | {kept} |")
            out.append("")
            if rank.get("rejected_outright"):
                out += ["**Rejected outright:**", ""]
                for rej in rank["rejected_outright"]:
                    out.append(f"- `{rej.get('id')}` — {rej.get('reason')}")
                out.append("")
    path.write_text("\n".join(out), encoding="utf-8")


def write_critic_log(records: list, path: Path):
    out = ["# Critic log — what was caught, and the correction", "",
           "This is the REFUTER pass, which runs on the survivors of the JUDGE pass",
           "(`output/judge_log.md`). `before` and `after` are the spotter's own rows,",
           "captured either side of review; the diff is computed by the pipeline.", ""]
    for r in records:
        out += [f"## {r['job']} — verdict `{r['verdict']}`, {len(r['findings'])} "
                f"finding(s), {r['blocking']} blocking", ""]
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
            out += ["### Correction applied", "", "| Row | Before | After |", "|---|---|---|"]
            for rn in r["changed"]:
                b, a = r["before"][rn], r["after"][rn]
                for k in [k for k in b if b.get(k) != a.get(k)]:
                    out.append(f"| `{rn}`.{k} | `{b.get(k)}` | `{a.get(k)}` |")
            out += [""]
        else:
            out += ["_No blocking finding, so no row changed._", ""]
    path.write_text("\n".join(out), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--live", action="store_true", help="make real model calls")
    ap.add_argument("--dry-run", action="store_true", help="assemble prompts, run no gates")
    ap.add_argument("--gaps", action="store_true", help="prove the gaps and exit")
    ap.add_argument("--job", choices=["announcer", "coach", "callsigns"])
    ap.add_argument("--scope", choices=["slice", "all"], default="slice")
    ap.add_argument("--naive", action="store_true",
                    help="the retriever BEFORE the tweak: no canon scope, no exemplar "
                         "boost. Kept runnable so the README's before/after is "
                         "reproducible rather than remembered.")
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
            filled = "" if not getattr(g, "not_filled_reason", None) else "  (not filled)"
            log(f"  {'PROVEN    ' if g.proven else 'NOT PROVEN'} {g.key}: "
                f"{g.title}{filled}")
        if args.gaps:
            log(f"\nwrote {OUT / 'gap_report.md'}")
            return
        closed = [g.key for g in proven_list if not g.proven]
        if closed:
            log(f"\nerror: gap(s) {closed} no longer provable — the table exists. "
                f"Generating content for a filled gap is waste; re-cut the job.")
            sys.exit(EXIT_GAP_CLOSED)
    else:
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

    # -- the crew ------------------------------------------------------------
    doctrine = {"spotter": crew.load_agent("spotter")[0],
                "critic": crew.load_agent("critic")[0]}
    caps = crew.spotter_char_caps()
    log(f"\ncrew: {crew.provenance()}")
    log(f"  spotter.md {len(doctrine['spotter']):,} chars · "
        f"critic.md {len(doctrine['critic']):,} chars · "
        f"char caps parsed from spotter.md: {caps}")

    fields = telemetry_fields()
    log(f"  shipped telemetry fields read from BRTelemetrySubsystem.h: {len(fields)}")

    # -- engine -------------------------------------------------------------
    if args.dry_run:
        engine = DryRunEngine(args.model, args.live, recording)
    elif args.live:
        engine = LiveEngine(args.model, recording)
    else:
        engine = ReplayEngine(recording)
    log(f"engine: {type(engine).__name__} · model {args.model} · "
        f"recording {recording.name}")

    index = rag.build_index(REPO)
    log(f"knowledge base: {len(index.chunks)} chunks from "
        f"{len({c.source for c in index.chunks})} sources "
        f"({sum(1 for c in index.chunks if c.canon == 'slice')} slice / "
        f"{sum(1 for c in index.chunks if c.canon == 'phase2')} phase2)")

    scope = None if (args.scope == "all" or args.naive) else "slice"
    jobs = [j for j in build_jobs(proven, fields) if not args.job or j.key == args.job]
    for j in jobs:
        if j.key == "coach":
            j.check_candidate = make_coach_check(fields)
            j.check_row = make_coach_row_check(fields)
        j.char_cap = caps["coach"] if j.key == "coach" else (
            caps["event"] if j.key == "announcer" else j.char_cap)
    if args.naive:
        for j in jobs:
            j.boost = {}
        log("retrieval: NAIVE (no canon scope, no exemplar boost)")

    traces, critic_records, judge_records, landed = [], [], [], []
    try:
        for job in jobs:
            rows = run_job(engine, index, job, scope, doctrine, traces,
                           critic_records, judge_records)
            if not rows:
                continue
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
    write_judge_log(judge_records, OUT / "judge_log.md")
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
    pool_total = sum(r["pool_size"] for r in judge_records)
    kept_total = sum(r["kept"] for r in judge_records)
    log(f"\npool: {pool_total} candidates generated, {kept_total} kept "
        f"({pool_total - kept_total} discarded by the judge)")
    total_findings = sum(len(r["findings"]) for r in critic_records)
    total_changed = sum(len(r["changed"]) for r in critic_records)
    log(f"critic: {total_findings} finding(s) across {len(critic_records)} job(s); "
        f"{total_changed} row(s) corrected before landing")
    if isinstance(engine, LiveEngine):
        t = summarize_usage(engine.exchanges)
        log(f"spend: {t['calls']} calls · {t['input_tokens']:,} in · "
            f"{t['output_tokens']:,} out · ${t['cost_usd']:.4f}")
    elif isinstance(engine, ReplayEngine) and engine.exchanges:
        # Priced from the recording's own usage blocks, NOT from its stored `totals`
        # (which predate cache itemisation -- see summarize_usage). This is the number
        # AUDIT.md quotes, so the run and the write-up cannot disagree on screen.
        t = summarize_usage(engine.exchanges)
        log(f"spend (recorded run): {t['calls']} calls · "
            f"{t['input_tokens']:,} in · {t['output_tokens']:,} out · "
            f"{t['cache_write_tokens']:,} cache-write · {t['cache_read_tokens']:,} cache-read")
        log(f"  priced at ${PRICE_PER_M['input']:.2f}/${PRICE_PER_M['output']:.2f}/"
            f"${PRICE_PER_M['cache_write']:.2f}/${PRICE_PER_M['cache_read']:.2f} per M "
            f"(in/out/cache-write/cache-read) = ${t['cost_usd']:.4f}")
    log(f"\ntrace: output/rag_trace.md · pool: output/judge_log.md · "
        f"review: output/critic_log.md · gaps: output/gap_report.md")


if __name__ == "__main__":
    main()
