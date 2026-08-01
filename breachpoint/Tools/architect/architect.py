#!/usr/bin/env python3
"""BP15 — The Architect: perception + deterministic scoring in front of the crew's gates.

    python3 architect.py --scan        step 1: parse the manifest, scan disk, emit perception
    python3 architect.py --rank        step 2: score every unit, print the ranked table
    python3 architect.py --blackboard  step 3: write the blackboard for the top-ranked unit
    python3 architect.py --all         all three, in order

THE LAW THIS FILE EXISTS TO ENFORCE: **an LLM never chooses the unit.** Every number below
comes out of deterministic Python with its terms printed. Steps 1 and 2 make ZERO API calls --
there is no network import in this file and no model is consulted. A ranking that came out of
a model is a ranking nobody can audit.

THE SECOND LAW: **the architect never writes game code.** It writes inside Tools/architect/ and
regenerates docs/BUILD-STATE.md. Nothing under Source/ is ever opened for writing here.

ARCHITECTURE §3 IS THE MANIFEST -- parse it, do not infer it (BP15 out-of-scope line). A file on
disk that §3 does not declare is reported as UNDECLARED; it is never silently adopted as a unit.

Exit codes: 0 ok · 1 usage/IO · 2 self-check failed (the doc and the scanner disagree).
"""

import argparse
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent                       # Tools/architect/ -> repo root
ARCH = REPO / "BREACHPOINT-ARCHITECTURE.md"
SRC = REPO / "Source" / "Breachpoint"
STATE_DIR = HERE / "state"
BLACKBOARD_DIR = HERE / "blackboard"

# --------------------------------------------------------------------------------------
# Declared exclusions. Every one is PRINTED at run time, never silently applied --
# `game-lead`: a silent cap reads as "covered everything".
# --------------------------------------------------------------------------------------

# The generic GE library: SEVEN classes in ONE header, under R18 a named library rather than
# numbered units. Corrected 1 Aug 2026 -- this was twice recorded as six classes across 17
# files. The lesson is encoded here rather than in a comment elsewhere: a UE header routinely
# declares several UCLASSes, so a unit scanner for this codebase counts CLASS DECLARATIONS,
# never files. `GE_CLASS_COUNT_EXPECTED` is asserted against the header at scan time so this
# number cannot drift again without the run failing loudly.
GE_HEADER = SRC / "AbilitySystem" / "Effects" / "BRGameplayEffects.h"
GE_CLASS_COUNT_EXPECTED = 7

# Phase-2 reserved: expected MISSING for the entire slice, and must rank LAST. A perpetually
# missing unit that scored high would be selected to build, inverting the ledger's intent.
PHASE2_UNITS = {"BRGameLiftLifecycle"}

# UE template variants the founder decided to KEEP (BP01 Log, 31 Jul). They are template code,
# not BR units; §3 does not declare them and they must not appear as UNDECLARED noise either.
TEMPLATE_DIRS = {"Variant_Horror", "Variant_Shooter"}

# --------------------------------------------------------------------------------------
# Step 2 inputs. Declared as data, with the source of each edge, so the score is auditable.
# --------------------------------------------------------------------------------------

# Ticket DAG, from each ticket's own STATUS/"Gated by" line in docs/tickets/.
TICKET_DEPS = {
    "BP00": [], "BP01": [], "BP07": [], "BP13": [],
    "BP02": ["BP01"], "BP03": ["BP02"], "BP04": ["BP02"], "BP05": ["BP02"],
    "BP06": ["BP05"], "BP09": ["BP05"], "BP08": ["BP02", "BP04"],
    "BP10": ["BP03"], "BP11": ["BP08"], "BP12": ["BP11"],
    "BP14": ["BP13"], "BP15": ["BP14"],
}

# Unit -> ticket, where the unit's TICKET differs from its FOLDER's default. Tickets cut across
# folders: all six abilities live in AbilitySystem/, but the fire path is BP03, the Golden
# Triangle is BP05 and the Grappleshot is BP06. Mapping by folder alone put every ability in
# BP02 and made the first ranked pick BRGA_Grapple -- whose real ticket is gated behind BP05,
# which is gated behind BP02. The score was ranking a unit that cannot legally be started.
# Each entry below is the ticket whose TITLE names the unit's subject (docs/tickets/).
UNIT_TICKET = {
    "BRGA_WeaponFire": "BP03",      # BP03: Weapons — equipment, fire path, and the cheat tests
    "BRGA_WeaponUtility": "BP03",   # reload + swap, same fire-path packet
    "BRGA_Grenade": "BP05",         # BP05: Grenades + melee — completing the Golden Triangle
    "BRGA_Melee": "BP05",
    "BRGA_Grapple": "BP06",         # BP06: Grappleshot — the netcode packet
}

# Folder -> owning ticket, from ARCHITECTURE §9's owner-path map crossed with ticket subjects.
FOLDER_TICKET = {
    "Core": "BP01", "Input": "BP01", "Character": "BP01",
    "AbilitySystem": "BP02", "Weapons": "BP03", "Match": "BP04",
    "AI": "BP08", "Online": "BP11", "UI": "BP10",
    "Telemetry": "BP11", "Data": "BP02", "Tests": "BP00",
}

# State is a GATE, not a nudge. The first draft used 2/1/0 and the blocker term (up to 35)
# swamped it, so the top-ranked unit came back BUILT -- a "what to build next" scorer selecting
# something already built. Magnitudes now separate the classes the way the tier term already
# separates Phase-2: a BUILT unit can never outrank an unbuilt one, whatever else it scores.
STATE_SCORE = {"MISSING": 100, "STUB": 50, "BUILT": -1000}
TIER_SCORE = {"slice": 0, "phase2": -100}


def log(msg=""):
    print(msg, flush=True)


def rel(p):
    """Repo-relative path for display, or the absolute path if it sits outside the repo.

    `Path.relative_to` RAISES on a non-subpath, so using it directly in a log line makes a
    cosmetic call load-bearing -- it crashed the self-check's own control case, where STATE_DIR
    is redirected to a temp dir. A formatting helper must never be able to fail a run.
    """
    try:
        return Path(p).relative_to(REPO).as_posix()
    except ValueError:
        return str(p)


# --------------------------------------------------------------------------------------
# Step 1 -- perception
# --------------------------------------------------------------------------------------

SECTION_RE = re.compile(r"^### 3\.(\d+)\s+`([A-Za-z]+)/`\s+—\s+(\d+)", re.M)
# A declared unit is a backticked path with a real extension. `BRAbilitySet` (no extension,
# prose mention) and `FSavedMove_BR` are correctly NOT matched -- the extension is what
# distinguishes a declaration from a reference.
UNIT_RE = re.compile(r"`((?:[A-Za-z]+/)?BR[A-Za-z0-9_]+)\.(h/\.cpp|h|cpp)`")


def parse_manifest():
    """ARCHITECTURE §3 -> {folder: {"declared": int, "units": [name, ...]}}."""
    if not ARCH.exists():
        log(f"FATAL: manifest not found at {ARCH}")
        sys.exit(1)
    text = ARCH.read_text(encoding="utf-8")

    marks = list(SECTION_RE.finditer(text))
    if not marks:
        log("FATAL: no `### 3.N `Folder/` — N` section headers parsed. §3's shape changed.")
        sys.exit(2)

    out = {}
    for i, m in enumerate(marks):
        folder, declared = m.group(2), int(m.group(3))
        end = marks[i + 1].start() if i + 1 < len(marks) else text.find("\n## 4.", m.start())
        body = text[m.start():end if end > 0 else len(text)]

        seen, units = set(), []
        for u in UNIT_RE.finditer(body):
            name = u.group(1).split("/")[-1]      # Abilities/BRGA_Sprint -> BRGA_Sprint
            if name not in seen:
                seen.add(name)
                # The declared FORM is data: `BRDataRows.h` is header-only BY DESIGN (row
                # structs), `BRCombatSpec.cpp` is cpp-only, `BRGA_Sprint.h/.cpp` is a pair.
                # Reporting a by-design header-only unit as STUB offered BRDataRows (889
                # lines, 8 USTRUCTs) as work to do. F4, step 6.
                form = {"h/.cpp": "both", "h": "header", "cpp": "cpp"}[u.group(2)]
                units.append({"name": name, "form": form})
        out[folder] = {"declared": declared, "units": units}
    return out


def has_statements(path, need=2):
    """Does this .cpp actually implement anything?

    The first version counted lines that were not blank/include/comment and required >3. F4
    (step 6) landed BUILT for all three of: an empty constructor with an empty override, a
    file that is 100% comments, and a file entirely inside `#if 0`. A body with no statement
    in it is not an implementation, so count SEMICOLONS outside comments, dead preprocessor
    blocks, and include lines -- an empty ctor has none, and every real translation unit has
    several.
    """
    src = path.read_text(encoding="utf-8", errors="replace")
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.DOTALL)          # block comments
    src = re.sub(r"^\s*#if\s+0.*?^\s*#(?:endif|else).*?$", "", src,
                 flags=re.DOTALL | re.M)                          # dead code
    n = 0
    for line in src.splitlines():
        line = line.split("//")[0].strip()
        if not line or line.startswith("#"):
            continue
        if ";" in line:
            n += 1
    return n >= need


def classify(folder, unit, form="both"):
    """BUILT / STUB / MISSING, judged against the form ARCHITECTURE §3 declares for the unit.

    `form` is parsed from the manifest, not guessed: `BRDataRows.h` is header-only by design
    and owes no .cpp, so demanding one reported an 889-line finished header as a STUB and
    offered it as work (F4's inverse, step 6).
    """
    root = SRC / folder
    hdr = list(root.rglob(f"{unit}.h"))
    cpp = list(root.rglob(f"{unit}.cpp"))

    if form == "header":
        return "BUILT" if hdr and len(hdr[0].read_text(encoding="utf-8", errors="replace")
                                       .splitlines()) > 8 else ("STUB" if hdr else "MISSING")
    if form == "cpp":
        if not cpp:
            return "MISSING"
        return "BUILT" if has_statements(cpp[0]) else "STUB"

    if not hdr and not cpp:
        return "MISSING"
    if not cpp:
        return "STUB"                       # header declared a pair; the .cpp never arrived
    return "BUILT" if any(has_statements(c) for c in cpp) else "STUB"


def undeclared_files(manifest):
    """Real BR* source on disk that §3 never declares. A finding, not a unit.

    `rglob("BR*.h")` is CASE-INSENSITIVE on Windows, so it also returns the kept UE template's
    `breachpointCharacter.h` and friends -- which are not BR units and would be reported as
    architecture drift on Windows and not on Linux. The same class of platform defect as the
    cp1252 one in Tools/data-crew (BP14 Log, 1 Aug): `Tools/` is where the cloud and the
    workstation run the same file, and nothing compiles it. Hence the explicit prefix test.
    """
    declared = {e["name"] for f in manifest.values() for e in f["units"]}
    skip = declared | {GE_HEADER.stem}          # the GE library is an announced exclusion
    found = {}
    for p in SRC.rglob("*.h"):
        stem = p.stem
        if not stem.startswith("BR") or stem in skip:
            continue
        if any(part in TEMPLATE_DIRS for part in p.parts):
            continue
        rel = p.relative_to(SRC).parts
        folder = rel[0] if len(rel) > 1 else "<module root>"
        found.setdefault(folder, []).append(stem)
    return {k: sorted(set(v)) for k, v in sorted(found.items())}


def count_ge_classes():
    if not GE_HEADER.exists():
        return 0
    return len(re.findall(r"^\s*class\s+\w*\s*UBRGE_\w+", GE_HEADER.read_text(encoding="utf-8"), re.M))


def scan():
    log("=" * 78)
    log("BP15 step 1 -- PERCEPTION (deterministic; ZERO API calls; no network import)")
    log("=" * 78)

    manifest = parse_manifest()

    log("\n-- Declared exclusions (printed, never silently applied) --------------------")
    ge = count_ge_classes()
    log(f"  1. Generic GE library: {ge} UBRGE_* classes in ONE header "
        f"({rel(GE_HEADER)}).")
    log(f"     Excluded as a named library under R18 -- NOT numbered units. Counted as CLASS")
    log(f"     DECLARATIONS, not files; expected {GE_CLASS_COUNT_EXPECTED}.")
    if ge != GE_CLASS_COUNT_EXPECTED:
        log(f"\nSELF-CHECK FAILED: expected {GE_CLASS_COUNT_EXPECTED} GE classes, found {ge}.")
        log("The exclusion rests on a count that no longer holds -- that is a finding.")
        sys.exit(2)
    log(f"  2. Phase-2 reserved, expected MISSING all slice: {', '.join(sorted(PHASE2_UNITS))}")
    log(f"  3. UE template variants kept by founder decision: {', '.join(sorted(TEMPLATE_DIRS))}")

    log("\n-- Manifest self-check (§3 headers vs §3 unit tables) -----------------------")
    log(f"  {'Folder':<16}{'declared':>9}{'parsed':>8}   status")
    total_declared = total_parsed = 0
    failures = []
    for folder, d in manifest.items():
        n = len(d["units"])
        ok = n == d["declared"]
        total_declared += d["declared"]
        total_parsed += n
        if not ok:
            failures.append((folder, d["declared"], n, d["units"]))
        log(f"  {folder + '/':<16}{d['declared']:>9}{n:>8}   {'ok' if ok else 'MISMATCH'}")
    log(f"  {'TOTAL':<16}{total_declared:>9}{total_parsed:>8}")

    budget = total_declared + len(PHASE2_UNITS)
    log(f"\n  §3 per-folder sum ......... {total_declared}")
    log(f"  + Phase-2 reserved ........ {len(PHASE2_UNITS)}  ({', '.join(sorted(PHASE2_UNITS))})")
    log(f"  = §4 composition budget ... {budget}")

    if failures:
        log("\nSELF-CHECK FAILED -- the doc and the scanner disagree. Not a rounding error.")
        for folder, dec, got, units in failures:
            log(f"  {folder}/: header says {dec}, its own table declares {got} -> {units}")
        sys.exit(2)
    if total_declared != 43 or budget != 44:
        log(f"\nSELF-CHECK FAILED: expected 43 in-slice + 1 reserved = 44, got "
            f"{total_declared} + {len(PHASE2_UNITS)} = {budget}.")
        sys.exit(2)
    log("  SELF-CHECK PASSED -- §3 is internally consistent and reaches §4's budget.")

    log("\n-- Disk scan: Source/Breachpoint/ -------------------------------------------")
    units, tally = [], {"BUILT": 0, "STUB": 0, "MISSING": 0}
    for folder, d in manifest.items():
        for entry in d["units"]:
            name, form = entry["name"], entry["form"]
            state = "MISSING" if name in PHASE2_UNITS else classify(folder, name, form)
            tally[state] += 1
            units.append({"unit": name, "folder": folder, "state": state, "form": form,
                          "ticket": UNIT_TICKET.get(name, FOLDER_TICKET.get(folder, "?")),
                          "tier": "phase2" if name in PHASE2_UNITS else "slice"})
    for name in sorted(PHASE2_UNITS):
        if not any(u["unit"] == name for u in units):
            units.append({"unit": name, "folder": "Online", "state": "MISSING",
                          "ticket": "BP11", "tier": "phase2"})
            tally["MISSING"] += 1
    log(f"  BUILT {tally['BUILT']} · STUB {tally['STUB']} · MISSING {tally['MISSING']} "
        f"· total {len(units)}")

    log("\n-- UNDECLARED: real BR* source that §3 does not declare ---------------------")
    undeclared = undeclared_files(manifest)
    if not undeclared:
        log("  (none)")
    for folder, names in undeclared.items():
        log(f"  {folder}/: {', '.join(names)}")
    log("  These are NOT adopted as units. §3 is the manifest; adopting them would let the")
    log("  scanner rewrite the architecture it is supposed to be checked against.")

    STATE_DIR.mkdir(parents=True, exist_ok=True)
    payload = {
        "generated_by": "Tools/architect/architect.py --scan",
        "api_calls": 0,
        "manifest_source": "BREACHPOINT-ARCHITECTURE.md §3",
        "budget": {"in_slice": total_declared, "reserved": len(PHASE2_UNITS), "total": budget},
        "exclusions": {"ge_classes": ge, "ge_header": rel(GE_HEADER),
                       "phase2": sorted(PHASE2_UNITS), "template_dirs": sorted(TEMPLATE_DIRS)},
        "tally": tally,
        "units": units,
        "undeclared": undeclared,
    }
    out = STATE_DIR / "perception.json"
    out.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    log(f"\n  wrote {rel(out)}")
    return payload


# --------------------------------------------------------------------------------------
# Step 2 -- utility scoring
# --------------------------------------------------------------------------------------

def ticket_depth(t, seen=None):
    seen = seen or set()
    if t in seen or t not in TICKET_DEPS:
        return 0
    seen = seen | {t}
    deps = TICKET_DEPS.get(t) or []
    return 0 if not deps else 1 + max(ticket_depth(d, seen) for d in deps)


INCLUDE_RE = re.compile(r'^\s*#include\s+"(?:[^"]*/)?(BR[A-Za-z0-9_]+)\.h"', re.M)


def include_edges(units):
    """Real `#include` edges between declared units -- the spec's second dependency source.

    Only BUILT/STUB units have files to parse, so this graph is partial BY CONSTRUCTION and the
    run log says so. It refines the ticket DAG; it never replaces it.
    """
    names = {u["unit"] for u in units}
    includes = {}
    for u in units:
        deps = set()
        for suffix in (".h", ".cpp"):
            for p in (SRC / u["folder"]).rglob(f"{u['unit']}{suffix}"):
                for m in INCLUDE_RE.finditer(p.read_text(encoding="utf-8", errors="replace")):
                    if m.group(1) in names and m.group(1) != u["unit"]:
                        deps.add(m.group(1))
        includes[u["unit"]] = deps
    return includes


def include_depth(unit, includes, seen=None):
    seen = seen or set()
    if unit in seen:
        return 0
    deps = includes.get(unit) or set()
    return 0 if not deps else 1 + max(include_depth(d, includes, seen | {unit}) for d in deps)


def transitive_includers(unit, includes):
    """Units that transitively #include this one -- i.e. that genuinely wait on it."""
    out, changed = set(), True
    while changed:
        changed = False
        for k, deps in includes.items():
            if k in out or k == unit:
                continue
            if unit in deps or (deps & out):
                out.add(k)
                changed = True
    return out


def dependents_of(t):
    """Tickets that transitively wait on t."""
    out, changed = set(), True
    while changed:
        changed = False
        for k, deps in TICKET_DEPS.items():
            if k in out:
                continue
            if t in deps or any(d in out for d in deps):
                out.add(k)
                changed = True
    return out


def rank(perception=None):
    perception = perception or json.loads((STATE_DIR / "perception.json").read_text(encoding="utf-8"))
    log("\n" + "=" * 78)
    log("BP15 step 2 -- UTILITY SCORING (deterministic; ZERO API calls)")
    log("=" * 78)
    log("score = depth + blockers + tier + state   |   ties break on LOWEST ticket number,")
    log("never on a model's preference. Every term is printed; the total is never the only")
    log("thing you can see.\n")

    units = [u for u in perception["units"]]
    counts = {}
    for u in units:
        counts[u["ticket"]] = counts.get(u["ticket"], 0) + 1

    includes = include_edges(units)
    parsed_edges = sum(len(v) for v in includes.values())
    log(f"  dependency sources: ticket DAG ({len(TICKET_DEPS)} tickets) + {parsed_edges} real")
    log(f"  #include edges. The include graph is PARTIAL by construction -- MISSING units have")
    log(f"  no files to parse -- so it refines the ticket DAG and never replaces it.\n")

    by_unit = {u["unit"]: u for u in units}
    rows = []
    for u in units:
        t = u["ticket"]
        depth = max(ticket_depth(t), include_depth(u["unit"], includes))
        # Blockers = units that wait on this one, from BOTH sources, unioned so a unit that is
        # both an includer and in a downstream ticket is counted once.
        waiters = set(transitive_includers(u["unit"], includes))
        for dep_ticket in dependents_of(t):
            waiters |= {x["unit"] for x in units if x["ticket"] == dep_ticket}
        waiters.discard(u["unit"])
        # F6a (step 6): the first version counted BUILT units as waiters. BRGA_WeaponFire's
        # ENTIRE blocker score of 4 was four already-BUILT BP10 widgets -- nothing was waiting
        # on it at all. A BUILT unit is not blocked by anything; counting it inflates exactly
        # the units whose consumers are already done.
        waiters = {w for w in waiters if by_unit.get(w, {}).get("state") != "BUILT"}
        blockers = len(waiters)
        tier = TIER_SCORE[u["tier"]]
        state = STATE_SCORE[u["state"]]
        rows.append({**u, "depth": depth, "blockers": blockers, "tier": tier,
                     "state_score": state, "total": depth + blockers + tier + state})

    rows.sort(key=lambda r: (-r["total"], int(re.sub(r"\D", "", r["ticket"]) or 99), r["unit"]))

    log(f"  {'#':>3}  {'unit':<30} {'tkt':<5}{'state':<9}"
        f"{'depth':>6}{'block':>6}{'tier':>6}{'state':>7}{'TOTAL':>8}")
    for i, r in enumerate(rows, 1):
        log(f"  {i:>3}  {r['unit']:<30} {r['ticket']:<5}{r['state']:<9}"
            f"{r['depth']:>6}{r['blockers']:>6}{r['tier']:>6}{r['state_score']:>7}{r['total']:>8}")

    out = STATE_DIR / "ranking.json"
    out.write_text(json.dumps({"api_calls": 0, "ranked": rows}, indent=2) + "\n", encoding="utf-8")
    log(f"\n  wrote {rel(out)}")
    log(f"  TOP-RANKED: {rows[0]['unit']} ({rows[0]['ticket']}, {rows[0]['state']}, "
        f"total {rows[0]['total']})")
    return rows


# --------------------------------------------------------------------------------------
# Step 3 -- the blackboard, written BEFORE anything else happens
# --------------------------------------------------------------------------------------

# F5 (step 6, high): `blackboard()` re-reads ranking.json and interpolated ranked[0]["unit"]
# straight into a path. A crafted name traverses out of blackboard/ and overwrites any .md in
# the repo -- this ticket's own file and BREACHPOINT-ARCHITECTURE.md were both hit in the
# probe. guard_laws.py cannot see it: it hooks TOOL CALLS, and this is a Python process
# writing through pathlib. Unit names come from UNIT_RE and are always bare identifiers, so
# validating that is free and closes the JSON-re-read path too.
UNIT_NAME_RE = re.compile(r"^BR[A-Za-z0-9_]+$")


def safe_unit_name(name):
    if not UNIT_NAME_RE.match(name or ""):
        log(f"FATAL: refusing to build a path from unit name {name!r} -- not a bare BR* "
            f"identifier. The architect never writes outside Tools/architect/.")
        sys.exit(1)
    return name


def blackboard(rows=None):
    rows = rows or json.loads((STATE_DIR / "ranking.json").read_text(encoding="utf-8"))["ranked"]
    top = rows[0]
    safe_unit_name(top.get("unit"))
    BLACKBOARD_DIR.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    path = BLACKBOARD_DIR / f"{stamp}-{safe_unit_name(top['unit'])}.md"

    folder = top["folder"]
    owner = f"Source/Breachpoint/{folder}/"
    table = "\n".join(
        f"| {i} | {r['unit']} | {r['ticket']} | {r['state']} | {r['depth']} | "
        f"{r['blockers']} | {r['tier']} | {r['state_score']} | **{r['total']}** |"
        for i, r in enumerate(rows[:12], 1))

    path.write_text(f"""# Blackboard — {top['unit']} — {stamp}

Written by `Tools/architect/architect.py --blackboard` BEFORE any generation. If this file is
absent, nothing was authorised. Nothing reaches the codebase unlogged.

## What it scored

Top 12 of {len(rows)}. Ties break on lowest ticket number. **Zero API calls** produced this order.

| # | unit | ticket | state | depth | blockers | tier | state | TOTAL |
|---|---|---|---|---|---|---|---|---|
{table}

**Selected: `{top['unit']}`** — {top['ticket']}, {top['state']}, total {top['total']}.

## What it will issue

Prompt handed to **builder** (verbatim):

> Implement `{top['unit']}` in `{owner}` per `BREACHPOINT-ARCHITECTURE.md` §3's entry for it.
> Server-authoritative; clients send intent. Attributes mutate only via GameplayEffects; the
> engine damage API is banned. Tuning numbers live in `Content/Data/*.csv` and reach C++ through
> `Source/Breachpoint/Data/BRDataRows.h` as SOFT refs. No gameplay Tick. Write ONLY inside
> `{owner}`. If you are blocked, file a `contract_gap` in the ticket and STOP.

Contracts attached: `gas-purity.md`, `data-and-assets.md`, `netcode.md` (if the unit adds a
replicated surface).

## What it will generate

| | |
|---|---|
| target | `{owner}{top['unit']}.h` / `.cpp` |
| owner_path | `{owner}` |
| ticket | {top['ticket']} |
| gate A | diff confined to owner_path — a diff outside is auto-rejected |

## Rungs owed

Rung 1 all three targets · rung 2 its spec · rung 4 green or **BLOCKED with a reason** — never
silently skipped. Per R30, a networked surface owes 4a (dedicated) and, if the path differs
host-vs-remote, 4b (listen + 1 remote); 4a green is not 4b evidence.
""", encoding="utf-8")
    log("\n" + "=" * 78)
    log("BP15 step 3 -- BLACKBOARD")
    log("=" * 78)
    log(f"  wrote {rel(path)}")
    log("  Written BEFORE generation. If the write fails, the run aborts and nothing is built.")
    return path


def main():
    ap = argparse.ArgumentParser(description="BP15 architect: perceive, score, log.")
    ap.add_argument("--scan", action="store_true")
    ap.add_argument("--rank", action="store_true")
    ap.add_argument("--blackboard", action="store_true")
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    if not any([a.scan, a.rank, a.blackboard, a.all]):
        ap.print_help()
        return 1
    p = scan() if (a.scan or a.all) else None
    r = rank(p) if (a.rank or a.all) else None
    if a.blackboard or a.all:
        blackboard(r)
    return 0


if __name__ == "__main__":
    sys.exit(main())
