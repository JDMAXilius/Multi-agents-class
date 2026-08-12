"""gaps.py — prove the content gaps from disk, before generating anything.

The assignment asks for "three generated outputs that your game actually
needs." Needing something is a claim, and claims in this project carry
evidence or they are not claims (CLAUDE.md law 6). So the pipeline does not
open with a paragraph about what the game is missing — it opens with three
checks that either pass or fail against the real repo.

Each gap states what the GDD promises, then proves the promise is unkept by
reading the shipped files. Run it standalone:

    python3 gaps.py

If a gap ever stops being provable — someone lands the table — this prints
NOT PROVEN and the pipeline refuses to generate content for it. A generator
that fills an already-filled gap is producing waste, and the honest place to
catch that is before the model is called, not in review.
"""

from __future__ import annotations

import csv
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DATA = REPO / "breachpoint" / "Content" / "Data"
SOURCE = REPO / "breachpoint" / "Source" / "Breachpoint"
GDD = REPO / "breachpoint" / "BREACHPOINT-GDD-VERTICAL-SLICE.md"


@dataclass
class Evidence:
    check: str
    result: str
    supports_gap: bool


@dataclass
class Gap:
    key: str
    title: str
    promise: str          # what the GDD says the game has
    citation: str         # where it says it
    evidence: list[Evidence] = field(default_factory=list)

    @property
    def proven(self) -> bool:
        return bool(self.evidence) and all(e.supports_gap for e in self.evidence)

    def to_dict(self) -> dict:
        return {"key": self.key, "title": self.title, "promise": self.promise,
                "citation": self.citation, "proven": self.proven,
                "evidence": [vars(e) for e in self.evidence]}


# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------

def _rows(name: str) -> list[dict]:
    path = DATA / name
    if not path.exists():
        return []
    with path.open(encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def _grep_source(pattern: str) -> list[str]:
    """Case-insensitive search across the runtime module. Returns file:line hits."""
    rx = re.compile(pattern, re.IGNORECASE)
    hits = []
    for path in sorted(SOURCE.rglob("*")):
        if path.suffix not in (".cpp", ".h") or "Variant_" in str(path):
            continue
        try:
            for n, line in enumerate(path.read_text(encoding="utf-8",
                                                    errors="ignore").splitlines(), 1):
                if rx.search(line):
                    hits.append(f"{path.relative_to(REPO)}:{n}")
        except OSError:
            continue
    return hits


def _tables_matching(pattern: str) -> list[str]:
    rx = re.compile(pattern, re.IGNORECASE)
    return sorted(p.name for p in DATA.glob("*.csv") if rx.search(p.name))


# --------------------------------------------------------------------------
# Gap 1 — medals that fire with no announcer line
# --------------------------------------------------------------------------

def gap_announcer_coverage() -> Gap:
    gap = Gap(
        key="announcer_coverage",
        title="Four of eleven medals award in silence",
        promise="Feed: killfeed + medals (Double Kill, Killing Spree, Grapple Kill, "
                "Rocket multi-kill) — canned, instant, deterministic.",
        citation="BREACHPOINT-GDD-VERTICAL-SLICE.md §2.9 (HUD)")

    medals = _rows("DT_Medals.csv")
    lines = _rows("DT_SpotterLines.csv")
    covered = {r["TriggerId"] for r in lines}
    orphans = [r for r in medals if r["TriggerId"] not in covered]

    gap.evidence.append(Evidence(
        check="join DT_Medals.TriggerId -> DT_SpotterLines.TriggerId",
        result=(f"{len(orphans)} of {len(medals)} medals have no announcer line: "
                + ", ".join(f"{r['MedalName']} ({r['TriggerId']})" for r in orphans)),
        supports_gap=bool(orphans)))

    # The GDD names Rocket multi-kill in the HUD's own medal list, so this is not
    # a nice-to-have: the document promises the player hears it.
    named_in_gdd = "Rocket multi-kill" in GDD.read_text(encoding="utf-8")
    gap.evidence.append(Evidence(
        check="GDD §2.9 names 'Rocket multi-kill' among shipped medals",
        result="present in the GDD" if named_in_gdd else "not found",
        supports_gap=named_in_gdd and any(
            r["TriggerId"] == "Kill.Rocket.Multi" for r in orphans)))

    gap.orphans = orphans   # type: ignore[attr-defined]
    return gap


# --------------------------------------------------------------------------
# Gap 2 — the coach fallback table the GDD says ships in the build
# --------------------------------------------------------------------------

MATCH_END_TRIGGERS = ("Match.End", "Coach")


def gap_coach_fallback() -> Gap:
    gap = Gap(
        key="coach_fallback",
        title="The offline coach line has no canned table to fall back to",
        promise="Host-side, async, <= 12 calls/match, 3 s timeout, canned-line "
                "DataTable fallback shipped in the build. No connectivity => the "
                "game is identical minus flavor.",
        citation="BREACHPOINT-GDD-VERTICAL-SLICE.md §3.3 (The Runtime Agent — Spotter)")

    tables = _tables_matching(r"coach")
    gap.evidence.append(Evidence(
        check="Content/Data/*coach*.csv exists",
        result=", ".join(tables) if tables else "no coach table on disk",
        supports_gap=not tables))

    code = _grep_source(r"\bcoach")
    gap.evidence.append(Evidence(
        check="grep -i 'coach' across Source/Breachpoint (.cpp/.h)",
        result=f"{len(code)} hits" + (f": {code[:3]}" if code else ""),
        supports_gap=not code))

    # DT_SpotterLines does carry Match.End rows, but they are result stingers
    # ("Match won."), not telemetry-grounded coaching. The distinction is the
    # GDD's own: a coach line must reference >= 1 telemetry stat.
    lines = _rows("DT_SpotterLines.csv")
    match_end = [r for r in lines if any(t in r["TriggerId"] for t in MATCH_END_TRIGGERS)]
    with_stat = [r for r in match_end if re.search(r"\d", r["Text"])]
    gap.evidence.append(Evidence(
        check="DT_SpotterLines rows at match end that reference a telemetry stat",
        result=(f"{len(match_end)} match-end rows, {len(with_stat)} reference a stat "
                f"(GDD requires the coach line to reference >= 1)"),
        supports_gap=not with_stat))
    return gap


# --------------------------------------------------------------------------
# Gap 3 — bots have no names, and bots are most of the killfeed
# --------------------------------------------------------------------------

def gap_bot_callsigns() -> Gap:
    gap = Gap(
        key="bot_callsigns",
        title="Bots fill up to 7 of 8 slots and none of them has a name",
        promise="Bots fill every unfilled slot. Solo play is Team Slayer with "
                "seven bots.",
        citation="BREACHPOINT-GDD-VERTICAL-SLICE.md §1.3 (Mode) + §2.8 (Bots)")

    reads = _grep_source(r"GetPlayerName\(\)")
    gap.evidence.append(Evidence(
        check="killfeed/scoreboard reads a player name",
        result=(f"{len(reads)} read sites: {reads}" if reads else "no read sites"),
        supports_gap=bool(reads)))

    writes = _grep_source(r"SetPlayerName|BotName|Callsign")
    gap.evidence.append(Evidence(
        check="anything ever sets a bot's display name",
        result=f"{len(writes)} write sites" + (f": {writes[:3]}" if writes else ""),
        supports_gap=not writes))

    tables = _tables_matching(r"callsign|botname|roster")
    gap.evidence.append(Evidence(
        check="Content/Data table of bot names exists",
        result=", ".join(tables) if tables else "no callsign table on disk",
        supports_gap=not tables))
    return gap


# --------------------------------------------------------------------------

# --------------------------------------------------------------------------
# Gap 4 — arena landmarks the announcer never names
# --------------------------------------------------------------------------
#
# Proven, and deliberately NOT filled by this pipeline. spotter.md permits a
# line to name a place the manifest named, so the content is legal — but a
# per-zone callout needs a zone-entry trigger, and no such trigger exists in
# DT_SpotterLines or in the runtime. Generating lines for an event the game
# cannot raise is the same defect as the coach thresholds: content ahead of the
# system that would fire it. The pipeline reports it so the gap is on record
# for whoever cuts that ticket.

def gap_arena_callouts() -> Gap:
    gap = Gap(
        key="arena_callouts",
        title="Six of seven named arena landmarks are never spoken",
        promise="Footstep and weapon audio are the information system … the 35 m "
                "sightline cap and open three-level sightlines mean threats are "
                "usually visible before they are lethal.",
        citation="BREACHPOINT-GDD-VERTICAL-SLICE.md §2.7 (Information Without Radar)")

    manifest = DATA / "arena_manifest.json"
    names = [l["name"] for l in json.loads(
        manifest.read_text(encoding="utf-8"))["landmarks"]] if manifest.exists() else []
    spoken = " ".join(r["Text"] for r in _rows("DT_SpotterLines.csv")).lower()
    silent = [n for n in names if n.lower() not in spoken]

    gap.evidence.append(Evidence(
        check="arena_manifest landmarks named by any line in DT_SpotterLines",
        result=(f"{len(silent)} of {len(names)} never spoken: " + ", ".join(silent)),
        supports_gap=bool(silent)))

    # The reason it is not filled, checked rather than asserted.
    triggers = {r["TriggerId"] for r in _rows("DT_SpotterLines.csv")}
    zone = [t for t in triggers if "zone" in t.lower() or "callout" in t.lower()]
    gap.evidence.append(Evidence(
        check="a zone/callout trigger exists to fire a per-landmark line",
        result=(f"{len(zone)} zone triggers: {zone}" if zone
                else "none — no event the game raises would play such a line"),
        supports_gap=not zone))

    gap.not_filled_reason = (      # type: ignore[attr-defined]
        "No zone-entry trigger exists, so a per-landmark line has nothing to fire "
        "it. Filed for the ticket that adds the trigger; generating the lines "
        "first would be content ahead of its system.")
    return gap


BUILDERS = [gap_announcer_coverage, gap_coach_fallback, gap_bot_callsigns,
            gap_arena_callouts]


def repo_available() -> bool:
    """The gaps are proven against the live game repo. A submitted zip has no
    `Source/` tree to grep, so the pipeline says so and falls back to the
    committed report rather than pretending it re-proved anything."""
    return SOURCE.is_dir() and DATA.is_dir() and GDD.exists()


def prove_all() -> list[Gap]:
    return [b() for b in BUILDERS]


def to_json(gaps: list[Gap]) -> str:
    """The machine-readable record, including the orphan medals the announcer
    job needs. Committed so the zip layout can still build its jobs."""
    payload = {g.key: g.to_dict() for g in gaps}
    orphans = getattr(gaps[0], "orphans", [])
    payload["announcer_coverage"]["orphans"] = orphans
    return json.dumps(payload, indent=2)


def render(gaps: list[Gap]) -> str:
    out = ["# Gap report — proven from the repo, not asserted", "",
           f"Generated by `gaps.py` against `{REPO.name}/breachpoint/`.", ""]
    for g in gaps:
        out.append(f"## {'PROVEN' if g.proven else 'NOT PROVEN'} — {g.title}")
        out.append("")
        out.append(f"**The GDD promises:** {g.promise}  ")
        out.append(f"**Source:** {g.citation}")
        out.append("")
        if getattr(g, "not_filled_reason", None):
            out.append(f"> **Proven but deliberately NOT filled.** {g.not_filled_reason}")
            out.append("")
        out.append("| Check | Result | Supports the gap |")
        out.append("|---|---|---|")
        for e in g.evidence:
            out.append(f"| {e.check} | {e.result} | {'yes' if e.supports_gap else 'no'} |")
        out.append("")
    return "\n".join(out)


if __name__ == "__main__":
    gaps = prove_all()
    print(render(gaps))
    print(json.dumps({g.key: g.proven for g in gaps}, indent=2))
