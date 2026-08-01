#!/usr/bin/env python3
"""BP15 step 5 -- regenerate docs/BUILD-STATE.md from the architect's own outputs.

    python3 build_state.py            write docs/BUILD-STATE.md
    python3 build_state.py --stdout   print it instead (for diffing)

WHY THIS IS A GENERATOR AND NOT A DOCUMENT. BP15 step 5: "a hand-edited state file is a state
file that lies." Every fact below is COPIED from an input, never restated:

    state (BUILT/STUB/MISSING)  <- Tools/architect/state/perception.json   (the disk scanner)
    score terms + rank order    <- Tools/architect/state/ranking.json      (the scorer)
    the commit that landed each <- `git log --diff-filter=A` over the unit's real files
    the run's date              <- the blackboard FILENAME, not the clock

That copy-not-restate rule is also the answer to step 6's question "can BUILD-STATE report BUILT
for a unit that is only a STUB?" -- no, because this file has no opinion about state; it prints
perception.json's `state` field verbatim. Corrupt the scanner and this lies; edit this and it
cannot.

DETERMINISM IS A DONE-WHEN, NOT A NICETY. BP15: "docs/BUILD-STATE.md regenerates byte-identically
from a clean checkout." So:
  * no datetime.now(), no wall clock, no host, no user, no cwd anywhere in the output;
  * every list is explicitly sorted -- rglob() order is filesystem order and is NOT stable
    across machines;
  * commit SHAs are `%H` truncated in Python to a fixed 7, never `%h` -- git's short-SHA length
    comes from core.abbrev/object count and can differ between two checkouts of one history;
  * `--date=short` is passed explicitly so a reader's `log.date` config cannot move a date;
  * a unit whose commit cannot be determined prints `-`. It never prints a guess. A fabricated
    SHA in a state file is worse than an absent one, because it looks checkable.

Exit codes: 0 ok · 1 an input is missing (run architect.py --all first).
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent                        # Tools/architect/ -> repo root
SRC = REPO / "Source" / "Breachpoint"
STATE_DIR = HERE / "state"
BLACKBOARD_DIR = HERE / "blackboard"
OUT = REPO / "docs" / "BUILD-STATE.md"

SHA_LEN = 7

# --------------------------------------------------------------------------------------
# Known blockers. Declared as DATA with the source of each claim, because the score has no
# term for "do this unit's inputs exist?" -- BP15's step-4 Log entry, 1 Aug 2026:
#
#   "The score ranks by dependency depth, blocker count, tier and state -- it has no term for
#    whether a unit's inputs exist. BRGA_WeaponFire is correctly the most valuable next unit
#    *and* is not startable today."
#
# So the NEXT section prints the ranked remainder (a number the scorer produced) and these
# blockers (facts the scorer cannot see) side by side, rather than implying the top of the list
# is startable. Every row cites where it came from; nothing here is inferred by this script.
BLOCKERS = {
    "BRGA_WeaponFire": [
        ("`FBRWeaponRow` / `CT_Combat` carry no trace range and no spread -- a hitscan ability "
         "would have to invent both, which is a law-3 violation by construction",
         "BP15 Log, 1 Aug 2026 (step 4, gap 2) -- OPEN"),
        ("`DT_Weapons.csv` has no `AbilitySet` column, so equip has nothing to grant and the "
         "unit could never be granted to anyone",
         "BP15 Log, 1 Aug 2026 (step 4, gap 3) -- OPEN"),
        ("`Ability.Weapon.Fire` + the three `GameplayCue.Weapon.*.Fire` tags were undeclared",
         "BP15 Log, 1 Aug 2026 -- RESOLVED under BP03 (R23 open families); 34 EXTERN/34 DEFINE"),
    ],
    "BRCombatSpec": [("`Source/Breachpoint/Tests/` holds only `.gitkeep`; rung 2 has nothing to run",
                      "BP15 Log, 1 Aug 2026 (step 4, rungs)")],
    "BRShieldSpec": [("`Source/Breachpoint/Tests/` holds only `.gitkeep`; rung 2 has nothing to run",
                      "BP15 Log, 1 Aug 2026 (step 4, rungs)")],
    "BRBotDeterminismSpec": [("`Source/Breachpoint/Tests/` holds only `.gitkeep`; rung 2 has "
                              "nothing to run",
                              "BP15 Log, 1 Aug 2026 (step 4, rungs)")],
    "BRGameLiftLifecycle": [("Phase-2 reserved -- expected MISSING for the entire slice and "
                             "ranked last by the tier term, so it is never selectable",
                             "ARCHITECTURE §3.8 + BP15 step 1")],
}

# Blockers that stop the RUNG LADDER rather than any one unit. Same rule: cited, not inferred.
LADDER_BLOCKERS = [
    ("rung 1 (UBT, three targets)",
     "BLOCKED -- a UE editor session and a build must not overlap (R29.3)",
     "BP15 Log, 1 Aug 2026"),
    ("rung 2 (specs)",
     "BLOCKED -- same editor lock, and `Source/Breachpoint/Tests/` holds only `.gitkeep`",
     "BP15 Log, 1 Aug 2026"),
    ("rung 4b (listen + 1 remote)",
     "BLOCKED upstream by BP00's Gauntlet/NuGet failure",
     "BP15 Log, 1 Aug 2026"),
]


def die(msg):
    print(f"FATAL: {msg}", file=sys.stderr)
    sys.exit(1)


def load(name):
    p = STATE_DIR / name
    if not p.exists():
        die(f"{p.relative_to(REPO).as_posix()} not found -- run `architect.py --all` first.")
    return json.loads(p.read_text(encoding="utf-8"))


def run_date():
    """The date this state describes -- read from the blackboard FILENAME, never the clock.

    A `datetime.now()` here would break the byte-identical Done-when on the second day of the
    file's life, which is the exact failure mode a generated state file is meant to remove.
    """
    if BLACKBOARD_DIR.is_dir():
        stamps = sorted(m.group(1) for p in BLACKBOARD_DIR.glob("*.md")
                        for m in [re.match(r"(\d{4}-\d{2}-\d{2})-", p.name)] if m)
        if stamps:
            return stamps[-1]
    return "-"


def git(args):
    try:
        r = subprocess.run(["git"] + args, cwd=str(REPO), capture_output=True, text=True)
    except (OSError, ValueError):
        return ""
    return r.stdout if r.returncode == 0 else ""


def unit_files(unit, folder):
    """The unit's real files, sorted. rglob() order is filesystem order and is not stable."""
    root = SRC / folder
    if not root.is_dir():
        return []
    return sorted({p.relative_to(REPO).as_posix()
                   for suffix in (".h", ".cpp") for p in root.rglob(unit + suffix)})


def landing_commit(unit, folder):
    """(sha, date, subject) of the commit that ADDED this unit, or (None, None, None).

    --diff-filter=A over every file of the unit; git prints newest-first, so the OLDEST entry
    (the last line) is the landing. Anything undeterminable returns None and prints `-`; this
    function never invents a SHA.
    """
    files = unit_files(unit, folder)
    if not files:
        return (None, None, None)
    out = git(["log", "--diff-filter=A", "--format=%H\x1f%ad\x1f%s", "--date=short", "--"] + files)
    lines = [ln for ln in out.splitlines() if ln.strip()]
    if not lines:
        return (None, None, None)
    sha, date, subject = lines[-1].split("\x1f", 2)
    return (sha[:SHA_LEN], date, subject)


def cell(s):
    """Markdown-table-safe: a `|` inside a commit subject would silently add a column."""
    return s.replace("|", "\\|").strip()


def build():
    perception = load("perception.json")
    ranking = load("ranking.json")
    ranked = ranking["ranked"]
    tally = perception["tally"]
    budget = perception["budget"]
    date = run_date()

    by_unit = {r["unit"]: r for r in ranked}
    top = ranked[0]
    ties = [r for r in ranked[1:] if r["total"] == top["total"]]

    L = []
    w = L.append

    # -- header ------------------------------------------------------------------------
    w("# BREACHPOINT — BUILD STATE")
    w("")
    w("> **Generated — do not edit; edit the architecture or the scanner.**")
    w("> Written by `Tools/architect/build_state.py` from `Tools/architect/state/perception.json`,")
    w("> `Tools/architect/state/ranking.json` and `git log`. Edits made here are erased by the next")
    w("> run and are invisible until then, which is worse — a hand-edited state file is a state")
    w("> file that lies. To change a **state**, change the code the scanner reads; to change a")
    w("> **rank**, change `architect.py`'s terms; to change the **manifest**, change")
    w("> `BREACHPOINT-ARCHITECTURE.md` §3.")
    w("")
    w("Deterministic by construction: no clock, no host, no wall-clock date. The date below comes")
    w("from the blackboard filename, SHAs from `git log`, states and scores from the two JSON")
    w("inputs. Two runs from a clean checkout are byte-identical.")
    w("")
    w(f"| | |")
    w(f"|---|---|")
    w(f"| state of record | **{date}** (from `Tools/architect/blackboard/`) |")
    w(f"| manifest | `{perception['manifest_source']}` |")
    w(f"| budget | {budget['in_slice']} in-slice + {budget['reserved']} reserved = "
      f"**{budget['total']}** units |")
    w(f"| tally | **BUILT {tally['BUILT']}** · **STUB {tally['STUB']}** · "
      f"**MISSING {tally['MISSING']}** |")
    w(f"| API calls that produced this | **{perception.get('api_calls', 0)}** "
      f"(perception) · **{ranking.get('api_calls', 0)}** (ranking) |")
    w("")

    # -- BUILT -------------------------------------------------------------------------
    built = sorted((u for u in perception["units"] if u["state"] == "BUILT"),
                   key=lambda u: (u["folder"], u["unit"]))
    unattributed = []

    w("## BUILT")
    w("")
    w(f"{len(built)} units the scanner classified BUILT (a header plus a `.cpp` with a real body).")
    w("`commit` is the commit that **added** the unit's files (`git log --diff-filter=A`, oldest")
    w("entry). A unit whose landing commit cannot be determined shows `-` — never a guessed SHA.")
    w("")
    w("| unit | folder | ticket | commit | landed | what landed it |")
    w("|---|---|---|---|---|---|")
    for u in built:
        sha, cdate, subject = landing_commit(u["unit"], u["folder"])
        if sha is None:
            unattributed.append(u["unit"])
        w(f"| `{u['unit']}` | {u['folder']}/ | {u['ticket']} | "
          f"{('`' + sha + '`') if sha else '-'} | {cdate or '-'} | "
          f"{cell(subject) if subject else '-'} |")
    w("")
    if unattributed:
        w(f"**Unattributed ({len(unattributed)}):** " +
          ", ".join(f"`{n}`" for n in sorted(unattributed)) +
          " — files found, no adding commit in this history. Left as `-`.")
    else:
        w(f"**Every BUILT unit is attributed to a commit.** No `-` rows, and no SHA in this table")
        w("was written by hand.")
    w("")
    undeclared = perception.get("undeclared") or {}
    if undeclared:
        w("**UNDECLARED — real `BR*` source on disk that §3 does not declare.** Reported, never")
        w("adopted as units: adopting them would let the scanner rewrite the architecture it is")
        w("checked against.")
        w("")
        for folder in sorted(undeclared):
            w(f"- `{folder}/`: " + ", ".join(f"`{n}`" for n in sorted(undeclared[folder])))
        w("")

    # -- DECISIONS ---------------------------------------------------------------------
    w("## DECISIONS")
    w("")
    w(f"**Selected: `{top['unit']}`** — {top['ticket']}, {top['state']}, total **{top['total']}**.")
    w("")
    w("The terms, not the reasoning. `score = depth + blockers + tier + state`; ties break on")
    w("lowest ticket number, then unit name — never on a model's preference. An LLM did not")
    w("choose this unit and cannot: the numbers below came out of deterministic Python.")
    w("")
    w("| term | value | what it measures |")
    w("|---|---|---|")
    w(f"| depth | **{top['depth']}** | ticket-DAG depth ∪ `#include` depth |")
    w(f"| blockers | **{top['blockers']}** | units that transitively wait on this one |")
    w(f"| tier | **{top['tier']}** | GDD tier — slice `0`, Phase-2 `-100` |")
    w(f"| state | **{top['state_score']}** | MISSING `100` · STUB `50` · BUILT `-1000` |")
    w(f"| **TOTAL** | **{top['total']}** | |")
    w("")
    w("State is a **gate**, not a nudge, and tier is the same mechanism. The blocker term reaches")
    w("37, so a state term of 2/1/0 was swamped by it and the first draft ranked a BUILT unit")
    w("first — a *what to build next* scorer selecting something already built. The magnitudes")
    w("above make that arithmetically impossible.")
    w("")
    if ties:
        w("**Tied at the top, broken by the rule and not by a preference:**")
        w("")
        w("| unit | ticket | total | broken by |")
        w("|---|---|---|---|")
        w(f"| `{top['unit']}` | {top['ticket']} | {top['total']} | selected |")
        for r in ties:
            w(f"| `{r['unit']}` | {r['ticket']} | {r['total']} | "
              f"same ticket number, later unit name |")
        w("")
    w("**What the score does not measure:** whether the unit's *inputs* exist. Depth, blockers,")
    w("tier and state are the four terms this ticket specifies; readiness is not among them. See")
    w("NEXT — the selected unit is the most valuable next unit **and** is not startable today.")
    w("Those are different questions and only the first is scored.")
    w("")

    # -- NEXT --------------------------------------------------------------------------
    remainder = [r for r in ranked if r["state"] != "BUILT" and r["unit"] != top["unit"]]
    w("## NEXT")
    w("")
    w(f"The ranked remainder — {len(remainder)} selectable units below the selection, in score")
    w("order. BUILT units are omitted: they score `-1000` on state and can never be selected.")
    w("")
    w("| # | unit | ticket | state | depth | blockers | tier | state | TOTAL |")
    w("|---|---|---|---|---|---|---|---|---|")
    for i, r in enumerate(remainder, 2):
        w(f"| {i} | `{r['unit']}` | {r['ticket']} | {r['state']} | {r['depth']} | "
          f"{r['blockers']} | {r['tier']} | {r['state_score']} | **{r['total']}** |")
    w("")

    w("### Known blockers")
    w("")
    w("Facts the scorer cannot see. Each row cites its source; none is inferred by the generator.")
    w("")
    w("| unit | blocker | source |")
    w("|---|---|---|")
    for unit in [top["unit"]] + [r["unit"] for r in remainder]:
        for text, source in BLOCKERS.get(unit, []):
            w(f"| `{unit}` | {cell(text)} | {cell(source)} |")
    w("")
    w("### Ladder blockers")
    w("")
    w("| rung | status | source |")
    w("|---|---|---|")
    for rung, status, source in LADDER_BLOCKERS:
        w(f"| {cell(rung)} | {cell(status)} | {cell(source)} |")
    w("")
    w("A rung is never silently skipped — it is green or it is BLOCKED with a reason.")
    w("")

    return "\n".join(L) + "\n"


def main():
    ap = argparse.ArgumentParser(description="BP15 step 5: regenerate docs/BUILD-STATE.md.")
    ap.add_argument("--stdout", action="store_true", help="print instead of writing the file")
    a = ap.parse_args()
    text = build()
    if a.stdout:
        # Write BYTES, not str. Windows' console/pipe encoding is cp1252 here, and `—`/`·`/`∪`
        # are not in it -- `sys.stdout.write` raises UnicodeEncodeError on this exact file while
        # the utf-8 file write below succeeds. Same platform defect as Tools/data-crew (BP14 Log,
        # 1 Aug 2026): Tools/ is where the cloud box and the workstation run one file and nothing
        # compiles it, so the encoding has to be pinned rather than inherited.
        sys.stdout.buffer.write(text.encode("utf-8"))
        return 0
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {OUT.relative_to(REPO).as_posix()}  ({len(text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
