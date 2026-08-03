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
DATA_DIR = REPO / "Content" / "Data"            # the CSVs readiness is checked against
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
# 8 since BP05's `UBRGE_GrenadeCost` (BRGameplayEffects.h, commit 40af3b5, 1 Aug 2026). The
# constant was not bumped with it, so `--all` has exited 2 on this line ever since and
# test_selfcheck's CONTROL case ("faithful copy must PASS") has been red -- the suite was
# reporting a failure of the tree, not of itself. BREACHPOINT-ARCHITECTURE.md still says
# "seven" in §3.3's library paragraph and §4's exclusion 1; that doc is BP61's owner_path, so
# BP60 bumps the code and files a contract_gap rather than reaching across.
GE_CLASS_COUNT_EXPECTED = 8

# §4 exclusion item 3, ruling D11(b), 3 Aug 2026: real C++ that is deliberately NOT a numbered
# unit. A types header and a curve-access helper. Kept as a NAME set, mirroring how the GE
# library is excluded by class name rather than by file -- and printed at run time with the
# rest of the declared exclusions, never silently applied.
RULED_EXCLUSIONS = {"BRUITypes", "BRCombatCurves"}

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
#
# R34 (1 Aug 2026) generalises that into the rule the three bugs shared: **a term expressing
# IMPOSSIBILITY is a gate, and a gate uses a magnitude no sum of preferences can overcome.**
# `state`, `tier` and now `readiness` are gates; `depth` and `blockers` are preferences and stay
# small. rank() PRINTS the dominance arithmetic every run -- the invariant is checked against the
# largest preference sum the live table can actually produce, not asserted in a comment.
STATE_SCORE = {"MISSING": 100, "STUB": 50, "BUILT": -1000}
TIER_SCORE = {"slice": 0, "phase2": -100}

# R33 + R34. "unknown" scores as READY on purpose: readiness is derived from disk, and a unit
# whose §3 entry names nothing checkable has not been shown to be blocked. Inventing a blocker
# out of an absence of evidence would bury real work behind a parsing gap.
READINESS_SCORE = {"ready": 0, "unknown": 0, "not_ready": -500}


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

# §4's `| **Total budget** | **N** |` row -- the ONE number the doc states about its own size.
BUDGET_RE = re.compile(r"^\|\s*\*\*Total budget\*\*\s*\|\s*\*\*(\d+)\*\*\s*\|", re.M)


def stated_budget():
    """Read §4's stated total instead of hard-coding it.

    The 43/44 literals that used to sit in scan() made *declaring a unit* a CODE change:
    edit the doc, and the scanner failed until someone edited Python to agree. That is
    backwards for a tool whose whole premise is "§3 IS the manifest, parse it, do not infer
    it", and it is the whole of BP60.

    An ABSENT row is a FAILURE, never a pass. A check that quietly disables itself the
    moment the doc stops stating the number is the desync it exists to catch, wearing a hat.
    """
    m = BUDGET_RE.search(ARCH.read_text(encoding="utf-8"))
    if not m:
        log("\nSELF-CHECK FAILED: §4 states no `| **Total budget** | **N** |` row. The budget "
            "comes from the doc now; an absent row is a failure and never a free pass.")
        sys.exit(2)
    return int(m.group(1))


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
                # R33: readiness is judged against the unit's OWN §3 entry, so keep the row
                # the declaration sits on. UNIT_RE only matches a backticked path WITH an
                # extension, so this is always the declaring row and never a prose mention
                # of the unit inside some other unit's row.
                line_start = body.rfind("\n", 0, u.start()) + 1
                line_end = body.find("\n", u.start())
                spec = body[line_start:line_end if line_end > 0 else len(body)]
                units.append({"name": name, "form": form, "spec": spec})
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
    src = re.sub(r"^\s*#if\s+0\b.*?^\s*#(?:endif|else)\b.*?$", "", src,
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
    # The GE library and §4's item-3 named exclusions are both ANNOUNCED exclusions. Before
    # D11(b) only the GE one was machine-read, so a unit "excluded by class name with the
    # reason stated" in §4 went on being reported as undeclared forever -- the exclusion was
    # prose, and prose does not survive a scanner that disagrees with it. Anything named here
    # must also be named in §4 with its reason; this set is the machine half of that pair.
    skip = declared | {GE_HEADER.stem} | RULED_EXCLUSIONS
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


# --------------------------------------------------------------------------------------
# R33 -- READINESS. Computed MECHANICALLY, from disk, never from a model.
# --------------------------------------------------------------------------------------
#
# A unit whose declared inputs do not exist cannot be built, however valuable it is.
# `BRGA_WeaponFire` ranked #1 while three of its inputs were missing and the builder packet
# stopped at law 5 on contact -- the score had no term for "can this be started at all".
#
# THE EXTRACTION IS DELIBERATELY CONSERVATIVE, and its shape is the whole design:
#
#   * it reads ONLY the unit's own §3 row, so a reference is never attributed to a sibling;
#   * it matches four reference shapes that are UNAMBIGUOUS in this manifest's own notation,
#     and matches nothing else. A thing it does not match is NOT invented as a blocker;
#   * a family/placeholder reference (`Damage.*`, `Event.<Verb>.<Moment>`) names no concrete
#     input and is skipped;
#   * if the ground-truth file it would check against is itself absent, the reference is
#     UNKNOWN -- not missing. An absent oracle is not evidence of a missing input;
#   * every reference it looked for and every verdict is PRINTED. A silent cap reads as
#     "covered everything", and the cap here is large: most §3 rows are prose and name no
#     checkable input at all, so most units come back UNKNOWN. The run log says how many.
#
# UNKNOWN scores as READY (READINESS_SCORE above). The known cost is stated out loud in the
# run log: this term detects a MISSING INPUT THE MANIFEST NAMES. It cannot detect an input the
# manifest only implies -- §3's `BRGA_WeaponFire` row says "server validates (rate/ammo/cone/
# range)" in prose and names neither `Range_m`, `Spread_deg`, nor an `AbilitySet` column, so
# the three gaps that actually blocked BP03 step 2 are invisible to it in BOTH directions.

BACKTICK_RE = re.compile(r"`([^`]+)`")

# A gameplay tag reference: one of the manifest's tag roots followed by at least one dotted
# component. `Damage.Melee[.Rear]` is §3's notation for an OPTIONAL trailing component -- the
# conservative reading is the base tag `Damage.Melee`, which is what gets checked.
TAG_ROOTS = ("Ability", "InputTag", "State", "Damage", "Event", "GameplayCue",
             "SetByCaller", "Cooldown")
TAG_REF_RE = re.compile(r"^(?:" + "|".join(TAG_ROOTS) +
                        r")(?:\.[A-Za-z0-9_]+)+(?:\[\.[A-Za-z0-9_.]+\])?$")
# `FBRWeaponRow`-style row struct. The `Row` suffix is the discriminator: `FBRMatchTelemetry`
# and `FBRKillFeedEntry` are plain USTRUCTs that live with their owning unit, owe nothing to
# BRDataRows.h, and must not be reported as missing rows.
ROW_STRUCT_REF_RE = re.compile(r"^FBR[A-Za-z0-9]+Row$")
# A table reference: `DT_Weapons`, `DT_BotAmbitions.csv`, `CT_Combat`.
TABLE_REF_RE = re.compile(r"^((?:DT|CT)_[A-Za-z0-9]+)(?:\.csv)?$")
# A row field / CSV column. This project's data columns carry a UNIT SUFFIX (`ReloadTime_s`,
# `Range_m`, `Spread_deg`, `reaction_ms`, `sight_radius_m`) and that suffix is what makes the
# match safe: `COND_OwnerOnly`, `Mesh1P` and `MatchEndServerTime` are not columns and are not
# matched. A column named without a unit suffix is a reference this check CANNOT see.
COLUMN_REF_RE = re.compile(r"\b([A-Za-z][A-Za-z0-9]*_(?:m|s|ms|deg|cm|pct|x))\b")


def unit_references(spec):
    """The checkable inputs a unit's own §3 row NAMES -> {kind: [ref, ...]}."""
    refs = {"tag": set(), "row": set(), "table": set(), "column": set()}
    for span in BACKTICK_RE.findall(spec or ""):
        s = span.strip()
        if "*" in s or "<" in s:            # `Damage.*`, `Event.<Verb>.<Moment>` -- a family
            continue                         # or a placeholder names no concrete input
        if TAG_REF_RE.match(s):
            refs["tag"].add(s.split("[")[0])
            continue
        if ROW_STRUCT_REF_RE.match(s):
            refs["row"].add(s)
            continue
        m = TABLE_REF_RE.match(s)
        if m:
            refs["table"].add(m.group(1))
            continue
        refs["column"].update(COLUMN_REF_RE.findall(s))
    return {k: sorted(v) for k, v in refs.items()}


def declared_tags():
    """Every tag string DEFINED in BRGameplayTags.cpp, or None if that file is absent."""
    p = SRC / "Core" / "BRGameplayTags.cpp"
    if not p.exists():
        return None
    text = p.read_text(encoding="utf-8", errors="replace")
    return set(re.findall(r'UE_DEFINE_GAMEPLAY_TAG\w*\s*\(\s*[^,]+,\s*"([^"]+)"', text))


def declared_rows():
    """(row struct names, field names) from BRDataRows.h, or (None, None) if it is absent."""
    p = SRC / "Data" / "BRDataRows.h"
    if not p.exists():
        return None, None
    text = p.read_text(encoding="utf-8", errors="replace")
    structs = set(re.findall(r"^\s*struct\s+(?:\w+\s+)?(FBR[A-Za-z0-9]+)", text, re.M))
    fields = set(re.findall(r"UPROPERTY\s*\([^)]*\)\s*[\r\n]+\s*[^;\r\n]*?(\w+)\s*(?:=[^;]*)?;",
                            text))
    return structs, fields


def declared_columns():
    """({table stem: [column, ...]}, all columns), or (None, None) if there is no oracle.

    An EMPTY Content/Data/ counts as no oracle, not as an oracle that knows nothing. The
    difference decides whether a checkout without Content/ degrades to "cannot tell" or to
    "nothing in this project is buildable".
    """
    if not DATA_DIR.is_dir() or not any(DATA_DIR.glob("*.csv")):
        return None, None
    tables, columns = {}, set()
    for p in sorted(DATA_DIR.glob("*.csv")):
        head = p.read_text(encoding="utf-8", errors="replace").splitlines()
        cols = [c.strip().strip('"') for c in head[0].split(",")] if head else []
        tables[p.stem] = cols
        columns.update(c for c in cols if c)
    return tables, columns


def ground_truth():
    """Everything readiness is judged against, read once per run. All of it comes off disk."""
    tags = declared_tags()
    structs, fields = declared_rows()
    tables, columns = declared_columns()
    return {"tags": tags, "structs": structs, "fields": fields,
            "tables": tables, "columns": columns}


def readiness_of(spec, truth):
    """-> (verdict, [missing refs], [unknown refs], {kind: [refs looked for]}).

    verdict is "not_ready" if ANY named input is provably absent, "ready" if at least one was
    resolved and none was absent, and "unknown" if nothing checkable was named.
    """
    refs = unit_references(spec)
    missing, unknown, resolved = [], [], []

    def judge(kind, ref, oracle, present):
        tag = f"{kind}:{ref}"
        if oracle is None:
            unknown.append(tag)             # no oracle on disk -> NOT a blocker
        elif present:
            resolved.append(tag)
        else:
            missing.append(tag)

    for t in refs["tag"]:
        judge("tag", t, truth["tags"], truth["tags"] is not None and t in truth["tags"])
    for r in refs["row"]:
        judge("row", r, truth["structs"], truth["structs"] is not None and r in truth["structs"])
    for tb in refs["table"]:
        judge("table", tb, truth["tables"], truth["tables"] is not None and tb in truth["tables"])
    for c in refs["column"]:
        # A column counts as present if ANY CSV header or any BRDataRows.h field carries it --
        # §3 names the field, not which table it lands in, so demanding a specific table would
        # be inferring a fact the manifest does not state. The corollary is that "absent" can
        # only be asserted when BOTH oracles are readable: with one of them missing, the field
        # could be sitting in the half we cannot see. Caught by this file's own self-check,
        # which had `oracle = None` only when BOTH were absent -- so a checkout without
        # Content/ reported every named column as a missing input.
        oracle = None if (truth["columns"] is None or truth["fields"] is None) else True
        present = (c in (truth["columns"] or set())) or (c in (truth["fields"] or set()))
        judge("column", c, oracle, present)

    if missing:
        verdict = "not_ready"
    elif resolved:
        verdict = "ready"
    else:
        verdict = "unknown"
    return verdict, missing, unknown, refs


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
    log(f"  4. Ruled NOT units (D11(b), §4 exclusion 3): {', '.join(sorted(RULED_EXCLUSIONS))}")
    log(f"     A types header and a curve accessor. Excluded by NAME, like the GE library.")
    missing_rule = sorted(n for n in RULED_EXCLUSIONS
                          if not (SRC / "UI" / f"{n}.h").exists()
                          and not (SRC / "AbilitySystem" / f"{n}.h").exists())
    if missing_rule:
        log(f"\nSELF-CHECK FAILED: ruled exclusion(s) {missing_rule} name no header on disk. "
            f"An exclusion for a file that no longer exists hides nothing and misleads "
            f"everyone -- delete it from §4 and from RULED_EXCLUSIONS together.")
        sys.exit(2)

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
    stated = stated_budget()
    log(f"  §4 states ................. {stated}")
    if budget != stated:
        log(f"\nSELF-CHECK FAILED: §3's folders sum to {total_declared} + {len(PHASE2_UNITS)} "
            f"reserved = {budget}, but §4's composition table states {stated}. The doc "
            f"disagrees with itself -- declaring a unit means editing §3's header AND §4's "
            f"total, and this is the check that says so.")
        sys.exit(2)
    log("  SELF-CHECK PASSED -- §3 is internally consistent and reaches §4's budget.")

    log("\n-- Disk scan: Source/Breachpoint/ -------------------------------------------")
    truth = ground_truth()
    units, tally = [], {"BUILT": 0, "STUB": 0, "MISSING": 0}
    for folder, d in manifest.items():
        for entry in d["units"]:
            name, form = entry["name"], entry["form"]
            state = "MISSING" if name in PHASE2_UNITS else classify(folder, name, form)
            tally[state] += 1
            verdict, missing, unknown, refs = readiness_of(entry.get("spec"), truth)
            units.append({"unit": name, "folder": folder, "state": state, "form": form,
                          "ticket": UNIT_TICKET.get(name, FOLDER_TICKET.get(folder, "?")),
                          "tier": "phase2" if name in PHASE2_UNITS else "slice",
                          "readiness": verdict, "readiness_missing": missing,
                          "readiness_unknown": unknown,
                          "readiness_refs": {k: v for k, v in refs.items() if v}})
    for name in sorted(PHASE2_UNITS):
        if not any(u["unit"] == name for u in units):
            units.append({"unit": name, "folder": "Online", "state": "MISSING",
                          "ticket": "BP11", "tier": "phase2", "readiness": "unknown",
                          "readiness_missing": [], "readiness_unknown": [],
                          "readiness_refs": {}})
            tally["MISSING"] += 1
    log(f"  BUILT {tally['BUILT']} · STUB {tally['STUB']} · MISSING {tally['MISSING']} "
        f"· total {len(units)}")

    log("\n-- R33 READINESS: computed from disk, never from a model --------------------")
    log("  Ground truth consulted (all of it on disk; an absent oracle yields UNKNOWN, never")
    log("  a blocker):")
    n_tags = "ABSENT" if truth["tags"] is None else f"{len(truth['tags'])} tags declared"
    n_rows = ("ABSENT" if truth["structs"] is None
              else f"{len(truth['structs'])} structs / {len(truth['fields'])} fields")
    n_cols = ("ABSENT" if truth["tables"] is None
              else f"{len(truth['tables'])} tables / {len(truth['columns'])} columns")
    log(f"    tags    {rel(SRC / 'Core' / 'BRGameplayTags.cpp'):<52}{n_tags}")
    log(f"    rows    {rel(SRC / 'Data' / 'BRDataRows.h'):<52}{n_rows}")
    log(f"    columns {rel(DATA_DIR) + '/*.csv':<52}{n_cols}")
    log("  What it looks for, in the unit's OWN §3 row and nowhere else:")
    log(f"    tag      a backticked {'/'.join(TAG_ROOTS[:4])}/... rooted dotted token; "
        f"`X[.Y]` reads as `X`")
    log("    row      a backticked `FBR<Name>Row` -> must be a struct in BRDataRows.h")
    log("    table    a backticked `DT_*` / `CT_*`  -> must be a .csv in Content/Data/")
    log("    column   a unit-suffixed identifier (`Range_m`, `Spread_deg`, `reaction_ms`)")
    log("             -> must be a CSV header column or a BRDataRows.h field")
    log("    SKIPPED  family/placeholder refs (`Damage.*`, `Event.<Verb>.<Moment>`) -- they")
    log("             name no concrete input, so they can neither pass nor fail.")
    log(f"  {'unit':<30}{'verdict':<11}references looked for (kind:name)")
    r_tally = {"ready": 0, "not_ready": 0, "unknown": 0}
    for u in units:
        r_tally[u["readiness"]] += 1
        shown = sorted(f"{k}:{v}" for k, vs in u["readiness_refs"].items() for v in vs)
        detail = ", ".join(shown) if shown else "(none in its §3 row)"
        log(f"  {u['unit']:<30}{u['readiness'].upper():<11}{detail}")
        if u["readiness_missing"]:
            log(f"  {'':<30}{'':<11}ABSENT ON DISK -> {', '.join(u['readiness_missing'])}")
    log(f"  ready {r_tally['ready']} · not_ready {r_tally['not_ready']} · "
        f"unknown {r_tally['unknown']}  (of {len(units)})")
    log(f"  THE CAP, PRINTED: {r_tally['unknown']} units name NO checkable input in their §3")
    log("  row -- their rows are prose. Those are UNKNOWN and are treated as READY. This term")
    log("  detects a missing input the MANIFEST NAMES; it cannot detect one the manifest only")
    log("  implies. §3's BRGA_WeaponFire row says 'server validates (rate/ammo/cone/range)' and")
    log("  names no `Range_m`, no `Spread_deg` and no `AbilitySet` column, so the three gaps")
    log("  that actually blocked BP03 step 2 are invisible here in BOTH directions.")

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
        "readiness_tally": r_tally,
        "readiness_ground_truth": {
            "tags": rel(SRC / "Core" / "BRGameplayTags.cpp"),
            "rows": rel(SRC / "Data" / "BRDataRows.h"),
            "columns": rel(DATA_DIR) + "/*.csv",
            "declared_tags": 0 if truth["tags"] is None else len(truth["tags"]),
            "declared_row_structs": 0 if truth["structs"] is None else len(truth["structs"]),
            "declared_columns": 0 if truth["columns"] is None else len(truth["columns"]),
        },
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
    """Real `#include` edges between declared units -> (edges, discarded).

    The spec's second dependency source. Only BUILT/STUB units have files to parse, so this
    graph is partial BY CONSTRUCTION and the run log says so. It refines the ticket DAG; it
    never replaces it.

    R35: **an edge counts only if the included header EXISTS ON DISK.** Step 6's F3 added one
    line -- `#include "BRGA_Grenade.h"` -- to `BRCore.h` and moved that unit +27 and to #1,
    and the included header does not exist. The include cannot compile; a line that cannot
    compile is not evidence of a dependency, and accepting it made two of the score's terms
    writable by the same builder the score directs. Discarded edges are RETURNED, not dropped
    silently -- rank() prints how many and which, because a phantom include is a finding
    (either a broken build or an attempt to move the queue) and must not vanish into a filter.
    """
    names = {u["unit"] for u in units}
    on_disk = {n for n in names if next(SRC.rglob(f"{n}.h"), None) is not None}
    includes, discarded = {}, []
    for u in units:
        deps = set()
        for suffix in (".h", ".cpp"):
            for p in (SRC / u["folder"]).rglob(f"{u['unit']}{suffix}"):
                for m in INCLUDE_RE.finditer(p.read_text(encoding="utf-8", errors="replace")):
                    dep = m.group(1)
                    if dep not in names or dep == u["unit"]:
                        continue
                    if dep not in on_disk:
                        discarded.append({"from": u["unit"], "to": dep, "in": rel(p)})
                        continue
                    deps.add(dep)
        includes[u["unit"]] = deps
    return includes, discarded


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
    log("score = blockers + tier + state + readiness - depth   |   ties break on LOWEST ticket")
    log("number, never on a model's preference. Every term is printed; the total is never the")
    log("only thing you can see.")
    log("")
    log("  R32: depth is SUBTRACTED. Depth away from a DAG root is distance from being")
    log("       STARTABLE, so it must reduce the score. Adding it made the top pick")
    log("       BRSpotterSubsystem -- BP11, gated by BP08, gated by BP02+BP04 -- winning on")
    log("       depth 4 alone, while the three test specs (depth 0, startable) ranked 7-9 in")
    log("       the same table that reported rung 2 BLOCKED *because* Tests/ was empty.")
    log("       Deep units are not buried: `blockers` still lifts a unit many others wait on.")
    log("  R33: readiness is the fifth term, computed mechanically off disk (see --scan).")
    log("  R34: gates (state, tier, readiness) dominate preferences (depth, blockers).\n")

    units = [u for u in perception["units"]]
    counts = {}
    for u in units:
        counts[u["ticket"]] = counts.get(u["ticket"], 0) + 1

    includes, discarded = include_edges(units)
    parsed_edges = sum(len(v) for v in includes.values())
    log(f"  dependency sources: ticket DAG ({len(TICKET_DEPS)} tickets) + {parsed_edges} real")
    log(f"  #include edges. The include graph is PARTIAL by construction -- MISSING units have")
    log(f"  no files to parse -- so it refines the ticket DAG and never replaces it.")
    log(f"  R35 -- include edges DISCARDED because the included header is not on disk: "
        f"{len(discarded)}")
    for d in discarded:
        log(f"    {d['in']}: #include \"{d['to']}.h\" -> no such header. Cannot compile, so it")
        log(f"    is not evidence of a dependency; the edge {d['from']} -> {d['to']} is dropped.")
    if not discarded:
        log("    (none -- every include edge names a header that exists)")
    log("")

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
        # R33. A unit whose readiness was never computed (a synthetic row, or a perception.json
        # written before this term existed) is UNKNOWN, and UNKNOWN is READY: absence of a
        # readiness verdict is not a blocker.
        verdict = u.get("readiness") or "unknown"
        readiness = READINESS_SCORE[verdict]
        rows.append({**u, "depth": depth, "blockers": blockers, "tier": tier,
                     "state_score": state, "readiness": verdict, "readiness_score": readiness,
                     # R32: depth SUBTRACTED.
                     "total": blockers + tier + state + readiness - depth})

    rows.sort(key=lambda r: (-r["total"], int(re.sub(r"\D", "", r["ticket"]) or 99), r["unit"]))

    # R34, proved against the live table rather than asserted in a comment: the largest swing
    # the two PREFERENCE terms can produce here, versus the smallest gate magnitude in play.
    max_pref = max((r["blockers"] for r in rows), default=0) + max((r["depth"] for r in rows),
                                                                   default=0)
    gates = {"state MISSING->BUILT": abs(STATE_SCORE["MISSING"] - STATE_SCORE["BUILT"]),
             "tier slice->phase2": abs(TIER_SCORE["slice"] - TIER_SCORE["phase2"]),
             "readiness ready->not_ready": abs(READINESS_SCORE["ready"] -
                                               READINESS_SCORE["not_ready"])}
    log(f"  R34 dominance check: largest preference swing available in THIS table = "
        f"max blockers {max((r['blockers'] for r in rows), default=0)} + max depth "
        f"{max((r['depth'] for r in rows), default=0)} = {max_pref}")
    for name, mag in sorted(gates.items()):
        verdict34 = "DOMINATES" if mag > max_pref else "**FAILS -- a gate a preference can beat**"
        log(f"    gate {name:<28} magnitude {mag:>5}   {verdict34}")
    if any(mag <= max_pref for mag in gates.values()):
        log("\nSELF-CHECK FAILED (R34): a gate term no longer dominates the preference terms.")
        log("'This cannot be built' is not a preference to be outvoted.")
        sys.exit(2)
    log("")

    log(f"  {'#':>3}  {'unit':<30} {'tkt':<5}{'state':<9}"
        f"{'depth':>6}{'block':>6}{'tier':>6}{'state':>7}{'ready':>7}{'TOTAL':>8}")
    for i, r in enumerate(rows, 1):
        log(f"  {i:>3}  {r['unit']:<30} {r['ticket']:<5}{r['state']:<9}"
            f"{r['depth']:>6}{r['blockers']:>6}{r['tier']:>6}{r['state_score']:>7}"
            f"{r['readiness_score']:>7}{r['total']:>8}")

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
        f"| {i} | {r['unit']} | {r['ticket']} | {r['state']} | -{r['depth']} | "
        f"{r['blockers']} | {r['tier']} | {r['state_score']} | "
        f"{r.get('readiness_score', 0)} | **{r['total']}** |"
        for i, r in enumerate(rows[:12], 1))

    path.write_text(f"""# Blackboard — {top['unit']} — {stamp}

Written by `Tools/architect/architect.py --blackboard` BEFORE any generation. If this file is
absent, nothing was authorised. Nothing reaches the codebase unlogged.

## What it scored

Top 12 of {len(rows)}. Ties break on lowest ticket number. **Zero API calls** produced this order.

`score = blockers + tier + state + readiness − depth` (R32: depth is SUBTRACTED — depth away
from a DAG root is distance from being *startable*. R33: readiness is the fifth term, derived
from disk. R34: `state`, `tier` and `readiness` are gates and dominate; `depth` and `blockers`
are preferences.)

| # | unit | ticket | state | −depth | blockers | tier | state | ready | TOTAL |
|---|---|---|---|---|---|---|---|---|---|
{table}

**Selected: `{top['unit']}`** — {top['ticket']}, {top['state']}, readiness {top.get('readiness', 'unknown')}, total {top['total']}.

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
