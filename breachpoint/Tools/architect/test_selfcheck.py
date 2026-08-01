#!/usr/bin/env python3
"""Red-then-green for architect.py: parse_manifest, classify, safe_unit_name/blackboard, rank.

BP15's Done-when says the scanner "exits nonzero on any mismatch." A self-check that has only
ever been observed PASSING is not evidence -- it is the same false comfort that let a two-folder
guard_laws hook, a self-gating BP14 Kickoff, and a Linux-only BP14 box all read as enforced.
**An enforcement mechanism proves nothing until it is tested with a case it should REJECT.**

Section 1 drives the real parser over synthetic manifests: one faithful, four corrupted. Each
corruption is a defect that has actually occurred on this project or is one edit away.

Sections 2-4 close **F7** (step 6): the five manifest cases all exercised `parse_manifest()`, and
`classify()`, `rank()` and `blackboard()` had ZERO coverage -- which is exactly the gap F4 (BUILT
reported for an empty shell, STUB reported for three finished headers) and F5 (a unit name from
`ranking.json` interpolated into a write path, overwriting any `.md` in the repo) were found in.
Those are fixed in `architect.py`; these cases are what makes them STAY fixed. Every case here is
a defect that was live in this file's history, so every one of them is red against the version
before the fix and green against the version after it:

    section 2  classify()  F4, both directions -- an empty/comment-only/`#if 0` `.cpp` must not
                           read BUILT, and a §3-declared header-only unit must not read STUB
    section 3  safe_unit_name()/blackboard()  F5 -- the traversal payload must be REFUSED, and
                           the refusal is what is asserted, never merely the absence of a write
    section 4  rank()      F6a -- a BUILT dependent is not a waiter, so it adds no blocker point

Everything drives the REAL `architect.py` with its module globals redirected at temp dirs and
restored afterwards; no logic is copied out of it, or the test would be checking itself.

    python3 test_selfcheck.py        -> prints a table; exit 0 if all cases behave
"""

import contextlib
import io
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):      # `Tools/` is where the cloud and the workstation run
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")   # the same file and nothing
if hasattr(sys.stderr, "reconfigure"):      # compiles it -- cp1252 has bitten this repo three
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")   # times in one day.

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
ARCH = REPO / "BREACHPOINT-ARCHITECTURE.md"

sys.path.insert(0, str(HERE))
import architect                                                            # noqa: E402


# --------------------------------------------------------------------------------------
# Harness. Two rules: drive the real module, and leave no global changed behind you --
# a case that leaks STATE_DIR into the next one turns this file into the thing it exists
# to catch.
# --------------------------------------------------------------------------------------

@contextlib.contextmanager
def patched(**globals_):
    """Redirect architect's module globals, then put every one of them back."""
    saved = {k: getattr(architect, k) for k in globals_}
    try:
        for k, v in globals_.items():
            setattr(architect, k, v)
        yield
    finally:
        for k, v in saved.items():
            setattr(architect, k, v)


@contextlib.contextmanager
def quiet():
    """architect.log() writes to stdout; keep its run logs out of this file's table."""
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
        yield buf


def inside(child, parent):
    try:
        Path(child).resolve().relative_to(Path(parent).resolve())
        return True
    except ValueError:
        return False


# ======================================================================================
# Section 1 -- parse_manifest(): §3 corruptions the scanner must REJECT
# ======================================================================================

# Each case: (name, transform, must_exit_nonzero, why it matters)
CASES = [
    ("faithful copy", lambda t: t, False,
     "the control -- an unmodified manifest must still PASS, or every red below is meaningless"),
    ("header/table disagree", lambda t: t.replace("### 3.9 `UI/` — 4", "### 3.9 `UI/` — 7"), True,
     "§3.9's header claims 7 while its table declares 4 -- exactly the stale-tree defect that "
     "printed `AI/ (4)` against six units on 31 Jul"),
    ("a unit silently dropped",
     lambda t: t.replace("`BRShieldSpec.cpp`", "BRShieldSpec.cpp-removed"), True,
     "Tests/ header says 3, table now declares 2 -- a unit deleted from the doc without the "
     "count following it"),
    ("budget no longer reaches 44",
     lambda t: t.replace("### 3.1 `Core/` — 2", "### 3.1 `Core/` — 3")
                .replace("`BRCore.h/.cpp`", "`BRCore.h/.cpp` and `BRExtra.h/.cpp`"), True,
     "counts stay self-consistent but the total becomes 44 in-slice + 1 = 45, breaking §4's "
     "printed composition"),
    ("§3's shape changed entirely",
     lambda t: re.sub(r"^### 3\.\d+ ", "### X ", t, flags=re.M), True,
     "no section header parses -- the scanner must fail loudly rather than report zero units"),
]


def run_against(manifest_text):
    """Run the REAL architect.py with ARCH pointed at a synthetic manifest."""
    with tempfile.TemporaryDirectory() as td:
        fake = Path(td) / "BREACHPOINT-ARCHITECTURE.md"
        fake.write_text(manifest_text, encoding="utf-8")
        shim = Path(td) / "shim.py"
        shim.write_text(
            "import sys, pathlib\n"
            f"sys.path.insert(0, r'{HERE}')\n"
            "import architect\n"
            f"architect.ARCH = pathlib.Path(r'{fake}')\n"
            f"architect.STATE_DIR = pathlib.Path(r'{td}') / 'state'\n"
            "sys.exit(0 if architect.scan() else 0)\n", encoding="utf-8")
        p = subprocess.run([sys.executable, str(shim)], capture_output=True, text=True)
        return p.returncode


def section_manifest():
    original = ARCH.read_text(encoding="utf-8")
    print("\n-- 1. parse_manifest(): §3 corruptions the scanner must REJECT ---------------")
    print(f"  {'case':<44}{'expect':<10}{'exit':<8}result")
    failures = []
    for name, transform, must_fail, why in CASES:
        text = transform(original)
        if text == original and must_fail:
            print(f"  {name:<44}{'--':<10}{'--':<8}SETUP BROKEN: transform changed nothing")
            failures.append((name, why))
            continue
        code = run_against(text)
        ok = (code != 0) == must_fail
        print(f"  {name:<44}{'REJECT' if must_fail else 'ACCEPT':<10}{code:<8}"
              f"{'ok' if ok else 'FAILED'}")
        if not ok:
            print(f"      why this case exists: {why}")
            failures.append((name, why))
    return len(CASES), failures


# ======================================================================================
# Section 2 -- classify(): F4, in both directions
# ======================================================================================
#
# THE RED. Before the fix, `classify()` counted lines that were not blank and did not START
# WITH an include or a comment marker, and called >3 of them an implementation. Bare braces
# are lines, so the shell a builder leaves when it stops at a `contract_gap` -- an empty ctor
# plus one empty override -- scored six and shipped to `BUILD-STATE.md` as BUILT. A file that
# is 100% comment prose scored the same way (interior lines start with letters), and so did a
# file whose every function sits inside `#if 0`. In the other direction it demanded a `.cpp`
# from every unit, so `BRDataRows.h` -- 889 lines, 8 USTRUCTs, header-only BY DESIGN -- was
# reported STUB and offered as the tenth thing to build.
#
# Both directions are one root: the presence and line count of a `.cpp` is a fact about the
# C++ build model, not about whether a unit is implemented. So the fix has two halves, and
# each half needs its own reds: statements are counted as semicolons outside comments, dead
# preprocessor blocks and includes; and the expected FORM is parsed from §3 rather than
# assumed. The last pair below is the sharpest case in this file -- identical bytes on disk
# (a header, no `.cpp`) must classify BUILT or STUB purely on what §3 declares.

H_SUBSTANTIAL = '''#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BRProbe.generated.h"

/** The ONE header for every DT_ row struct. Pure USTRUCTs need no translation unit. */
USTRUCT(BlueprintType)
struct FBRWeaponRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    float DamagePerShot = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    int32 MagazineSize = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    TSoftObjectPtr<USkeletalMesh> Mesh;
};

USTRUCT(BlueprintType)
struct FBRArmourRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Armour")
    float ShieldCapacity = 0.f;
};
'''

H_NEAR_EMPTY = '''#pragma once

#include "CoreMinimal.h"

// TODO(BP02): the row structs go here.
'''

CPP_EMPTY_SHELL = '''#include "BRProbe.h"

UBRProbe::UBRProbe()
{
}

void UBRProbe::ActivateAbility(const FGameplayAbilitySpecHandle Handle)
{
}
'''

CPP_COMMENT_ONLY = '''#include "BRProbe.h"

/*
BLOCKED at a contract_gap; nothing is implemented here.
The file exists only so the module links; see the ticket.
Every prose line starts with a LETTER, not with a comment marker -- which is
precisely why a filter that looks at the first characters of a line missed them.
*/

// Nothing below this line does anything.
'''

CPP_IF_ZERO = '''#include "BRProbe.h"

#if 0
void UBRProbe::Fire()
{
    Ammo = Ammo - 1;
    ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, RecoilEffect, 1.f);
    UE_LOG(LogBR, Warning, TEXT("nothing the compiler ever sees"));
}
#endif
'''

CPP_REAL = '''#include "BRProbe.h"
#include "AbilitySystemComponent.h"

UBRProbe::UBRProbe()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UBRProbe::ActivateAbility(const FGameplayAbilitySpecHandle Handle)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, RecoilEffect, 1.f);
}
'''

# (name, form declared by §3, files on disk, expected state, why this case exists)
CLASSIFY_CASES = [
    ("empty ctor + empty override .cpp", "both",
     {"BRProbe.h": H_NEAR_EMPTY, "BRProbe.cpp": CPP_EMPTY_SHELL}, "STUB",
     "F4's first probe, and the exact shell a builder leaves when it stops at a contract_gap. "
     "It shipped as BUILT, which drops the unit from MISSING 100 to BUILT -1000 -- permanently "
     "out of the queue -- and tells BUILD-STATE.md the fire path is done"),
    ("100%-comment .cpp", "both",
     {"BRProbe.h": H_NEAR_EMPTY, "BRProbe.cpp": CPP_COMMENT_ONLY}, "STUB",
     "F4's second probe, and red twice over: its prose lines begin with letters, which is how "
     "the old first-characters filter waved them through, and two of them carry a semicolon, "
     "so break the block-comment strip and the file counts two statements and reads BUILT"),
    (".cpp entirely inside #if 0", "both",
     {"BRProbe.h": H_NEAR_EMPTY, "BRProbe.cpp": CPP_IF_ZERO}, "STUB",
     "F4's third probe -- three semicolons the compiler never sees. Nothing in this file is "
     "built, so nothing in it may be counted as built"),
    ("real .cpp, several statements", "both",
     {"BRProbe.h": H_NEAR_EMPTY, "BRProbe.cpp": CPP_REAL}, "BUILT",
     "the control for the three reds above -- tighten the statement test far enough and every "
     "unit reads STUB, which is just as wrong and much easier to miss"),
    ("§3 says header-only, substantial .h", "header",
     {"BRProbe.h": H_SUBSTANTIAL}, "BUILT",
     "F4's inverse, and it was live in BUILD-STATE.md: BRDataRows (889 lines, 8 USTRUCTs, no "
     ".cpp by design) ranked 10th as work to do. Pure USTRUCTs owe no translation unit"),
    ("§3 says header-only, .h is a stub", "header",
     {"BRProbe.h": H_NEAR_EMPTY}, "STUB",
     "the control for the case above -- 'header-only' must not become a blanket amnesty that "
     "reports every declared-but-unwritten header as finished"),
    ("SAME .h, but §3 declares a pair", "both",
     {"BRProbe.h": H_SUBSTANTIAL}, "STUB",
     "identical bytes on disk as the header-only case, opposite verdict. This is what proves "
     "the form comes from the MANIFEST and is not inferred from what happens to be on disk -- "
     "a declared .h/.cpp pair whose .cpp never arrived is a stub"),
    ("§3 says cpp-only, real body", "cpp",
     {"BRProbe.cpp": CPP_REAL}, "BUILT",
     "Tests/ is declared `3 (cpp only)`; demanding a header from a spec file would report the "
     "three units that block rung 2 for the whole project as missing work they do not owe"),
    ("§3 says cpp-only, empty shell", "cpp",
     {"BRProbe.cpp": CPP_EMPTY_SHELL}, "STUB",
     "the cpp-only path must run the same statement test as the pair path, not a cheaper one"),
    ("nothing on disk, declared a pair", "both", {}, "MISSING",
     "MISSING and STUB score 100 vs 50 -- collapsing them would silently halve the priority of "
     "every unit that does not exist yet"),
    ("nothing on disk, declared header", "header", {}, "MISSING",
     "the header-only path has its own return statement, so it needs its own MISSING red"),
]


def drive_classify(form, files):
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "Source" / "Breachpoint"
        (src / "Data").mkdir(parents=True)
        for name, body in files.items():
            (src / "Data" / name).write_text(body, encoding="utf-8")
        with patched(SRC=src), quiet():
            return architect.classify("Data", "BRProbe", form)


def section_classify():
    print("\n-- 2. classify(): F4, both directions ---------------------------------------")
    print(f"  {'case':<44}{'form':<9}{'expect':<9}{'got':<9}result")
    failures = []

    for name, form, files, expect, why in CLASSIFY_CASES:
        got = drive_classify(form, files)
        ok = got == expect
        print(f"  {name:<44}{form:<9}{expect:<9}{got:<9}{'ok' if ok else 'FAILED'}")
        if not ok:
            print(f"      why this case exists: {why}")
            failures.append((name, why))

    # Provenance: the `form` values above are only meaningful if §3 really declares them. Read
    # it out of the live manifest rather than trusting this file's own table.
    why = ("classify() trusts `form` completely; if parse_manifest stopped emitting 'header' for "
           "BRDataRows.h the F4 inverse comes straight back, and every case above would still "
           "pass because they pass their own form in")
    with quiet():
        units = architect.parse_manifest()["Data"]["units"]
    got = next((u["form"] for u in units if u["name"] == "BRDataRows"), "<absent>")
    ok = got == "header"
    print(f"  {'§3 itself declares BRDataRows.h header-only':<44}{'--':<9}{'header':<9}{got:<9}"
          f"{'ok' if ok else 'FAILED'}")
    if not ok:
        print(f"      why this case exists: {why}")
        failures.append(("§3 itself declares BRDataRows.h header-only", why))

    return len(CLASSIFY_CASES) + 1, failures


# ======================================================================================
# Section 3 -- safe_unit_name()/blackboard(): F5, the path traversal
# ======================================================================================
#
# THE RED. `blackboard()` re-reads `state/ranking.json` off disk on the --blackboard-only path
# and interpolated `ranked[0]["unit"]` straight into a write path. The name is validated when
# the MANIFEST is parsed and was never re-validated coming back out of JSON, so one edited
# string in a generated file -- a file that lives outside Source/ and Content/ and that every
# review habit on this board treats as noise -- overwrote any `.md` in the repo. The probe hit
# BREACHPOINT-ARCHITECTURE.md (the manifest the scanner itself parses) and this ticket's own
# 495-line Log. `guard_laws.py` is blind to it: it hooks TOOL CALLS, and this is a Python
# process writing through pathlib.
#
# WHY THE ASSERTION IS ON THE REFUSAL AND NOT ON THE ABSENCE OF A FILE, recorded in the ticket
# and worth repeating where the test can be edited: on Windows the naive payload
# `"../../../../Source/…"` FAILS by itself, because the first component becomes `2026-08-01-..`
# and Win32 strips trailing dots, so it is read as a directory name rather than a parent ref.
# One character (`x/` in front) restores clean `..` components and Win32 canonicalises them
# lexically before touching the filesystem. **A failed traversal is not evidence of safety.**
# So every case below asserts a nonzero SystemExit -- an explicit refusal -- and the canary
# check is only ever an extra. A case that "passed" because the write crashed is a FAILED case.

CANARY = "# TICKET_BP15_ARCHITECT (canary)\n\nIf a blackboard replaced this, F5 is back.\n"

# (name, payload, must_refuse, why)
NAME_CASES = [
    ("F5's payload, verbatim", "x/../../../../docs/tickets/TICKET_BP15_ARCHITECT", True,
     "the exact string from the step-6 probe. It replaced this ticket's 495 lines with a "
     "2,751-byte blackboard and exited 0"),
    ("absolute path, drive-rooted", "C:/Users/Public/BRPwned", True,
     "an absolute name ignores BLACKBOARD_DIR entirely -- no `..` needed, so a traversal guard "
     "that only looks for `..` is not a guard"),
    ("absolute path, posix-rooted", "/tmp/BRPwned", True,
     "the same escape on the cloud runner; this file runs on both and the repo has already "
     "shipped one Linux-only enforcement box"),
    ("forward-slash separator", "BRGA_WeaponFire/../../BRPwned", True,
     "starts with a legitimate unit name, so a prefix check instead of a full match passes it"),
    ("backslash separator", "BRGA_WeaponFire\\..\\..\\BRPwned", True,
     "the Windows spelling of the case above -- a guard written against `/` alone misses it"),
    ("a dot in the name", "BRDataRows.h", True,
     "no separator and no `..`, but `.` is the character every traversal is built out of; the "
     "rule is a bare BR* identifier, not 'nothing suspicious'"),
    ("empty name", "", True,
     "ranking.json is untrusted input on this path; a missing key must refuse, not build "
     "`<stamp>-.md` and call it authorised"),
    ("a legitimate unit", "BRGA_WeaponFire", False,
     "the control -- refuse everything and step 3 never writes a blackboard, which under this "
     "file's own law means nothing was authorised and nothing may be built"),
]


def blackboard_row(unit):
    """One `ranked` entry with every key blackboard() touches."""
    return {"unit": unit, "folder": "AbilitySystem", "ticket": "BP03", "state": "MISSING",
            "depth": 2, "blockers": 0, "tier": 0, "state_score": 100, "total": 102}


def drive_safe_name(payload):
    with quiet():
        try:
            architect.safe_unit_name(payload)
        except SystemExit as e:
            return f"exit {e.code}"
        except Exception as e:                      # noqa: BLE001 -- a crash is not a refusal
            return f"crash {type(e).__name__}"
    return "accepted"


def drive_blackboard(payload, td):
    """Drive the real blackboard() through F5's path: ranking.json re-read off disk.

    The sandbox is nested six deep so that even a SUCCESSFUL traversal lands inside the temp
    tree -- `x/../../../../docs/tickets/…` from `<bb>/<stamp>-x` resolves exactly onto the
    canary below. The escape is therefore observable without ever being able to touch the repo.
    """
    deep = Path(td) / "a" / "b" / "c" / "d" / "e"
    bb, state = deep / "blackboard", deep / "state"
    state.mkdir(parents=True, exist_ok=True)
    canary = Path(td) / "a" / "b" / "c" / "docs" / "tickets" / "TICKET_BP15_ARCHITECT.md"
    canary.parent.mkdir(parents=True, exist_ok=True)
    canary.write_text(CANARY, encoding="utf-8")
    (state / "ranking.json").write_text(
        json.dumps({"api_calls": 0, "ranked": [blackboard_row(payload)]}), encoding="utf-8")

    outcome = "wrote"
    with patched(STATE_DIR=state, BLACKBOARD_DIR=bb), quiet():
        try:
            architect.blackboard()                  # rows=None -> the JSON re-read path
        except SystemExit as e:
            outcome = f"exit {e.code}"
        except Exception as e:                      # noqa: BLE001
            outcome = f"crash {type(e).__name__}"
    written = sorted(p for p in bb.glob("*.md")) if bb.exists() else []
    return outcome, written, canary.read_text(encoding="utf-8") == CANARY


def section_traversal():
    print("\n-- 3. safe_unit_name()/blackboard(): F5 path traversal -----------------------")
    print(f"  {'case':<44}{'expect':<10}{'outcome':<16}result")
    failures = []

    for name, payload, must_refuse, why in NAME_CASES:
        outcome = drive_safe_name(payload)
        refused = outcome.startswith("exit ") and outcome != "exit 0"
        ok = refused == must_refuse
        print(f"  {name:<44}{'REFUSE' if must_refuse else 'ACCEPT':<10}{outcome:<16}"
              f"{'ok' if ok else 'FAILED'}")
        if not ok:
            print(f"      why this case exists: {why}")
            failures.append((name, why))

    # End-to-end, because safe_unit_name() being correct is not the claim -- the claim is that
    # blackboard() calls it BEFORE it builds a path, on the path that re-reads untrusted JSON.
    why = ("F5 end-to-end: the guard has to fire on the --blackboard-only path, where `top` "
           "comes back out of ranking.json and not out of UNIT_RE")
    with tempfile.TemporaryDirectory() as td:
        outcome, written, intact = drive_blackboard(NAME_CASES[0][1], td)
        refused = outcome.startswith("exit ") and outcome != "exit 0"
        ok = refused and intact and not written
        detail = outcome + ("" if intact else " CANARY OVERWRITTEN")
        print(f"  {'F5 through ranking.json, canary planted':<44}{'REFUSE':<10}{detail:<16}"
              f"{'ok' if ok else 'FAILED'}")
        if not ok:
            print(f"      why this case exists: {why}")
            print(f"      refused={refused} canary_intact={intact} files_written={written}")
            failures.append(("F5 through ranking.json, canary planted", why))

    why2 = ("the control -- a legitimate top-ranked unit must still produce a blackboard, and it "
            "must land INSIDE Tools/architect/blackboard/, which is the second half of the "
            "docstring's law that nothing under Source/ is ever opened for writing here")
    with tempfile.TemporaryDirectory() as td:
        outcome, written, intact = drive_blackboard("BRGA_WeaponFire", td)
        bb = Path(td) / "a" / "b" / "c" / "d" / "e" / "blackboard"
        ok = outcome == "wrote" and len(written) == 1 and inside(written[0], bb) and intact
        print(f"  {'a legitimate unit still gets a blackboard':<44}{'ACCEPT':<10}{outcome:<16}"
              f"{'ok' if ok else 'FAILED'}")
        if not ok:
            print(f"      why this case exists: {why2}")
            print(f"      outcome={outcome} files_written={written}")
            failures.append(("a legitimate unit still gets a blackboard", why2))

    return len(NAME_CASES) + 2, failures


# ======================================================================================
# Section 4 -- rank(): F6a, a BUILT unit is not waiting on anything
# ======================================================================================
#
# THE RED. `blockers` is "units that wait on this one", built from the ticket DAG plus the
# include graph. `dependents_of()` returns downstream TICKETS and the loop added EVERY unit of
# those tickets regardless of state. BRGA_WeaponFire's entire blocker score of 4 was four
# already-BUILT BP10 widgets: BRActivatableWidget, BRHUDLayout, BRUIManagerSubsystem,
# BRViewModels. Nothing was waiting on it at all, and 4 was exactly the margin that put it #1 --
# which is where the Log's strongest claim came from, that "a deterministic scorer with no
# knowledge of the handoff reproduced the human board's next move." It did not; it counted four
# finished UI widgets as blocked work.
#
# The payload below is that exact shape, minimised: one MISSING unit in BP03, and BP10 -- the
# only ticket that transitively waits on BP03 -- holding nothing but BUILT units. Its blockers
# term must be 0 and its total must be 102, which is the number the fixed run reports. The
# second case flips ONE waiter to STUB and requires 1, because a filter that returns 0 for
# everything would pass the first case and destroy the term.

BP10_WIDGETS = ["BRActivatableWidget", "BRHUDLayout", "BRUIManagerSubsystem", "BRViewModels"]


def f6a_units(states):
    units = [{"unit": "BRGA_WeaponFire", "folder": "AbilitySystem", "ticket": "BP03",
              "state": "MISSING", "tier": "slice"}]
    for name, state in zip(BP10_WIDGETS, states):
        units.append({"unit": name, "folder": "UI", "ticket": "BP10",
                      "state": state, "tier": "slice"})
    return units


def drive_rank(units):
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "Source" / "Breachpoint"
        for u in units:
            (src / u["folder"]).mkdir(parents=True, exist_ok=True)
        state = Path(td) / "state"
        state.mkdir(parents=True, exist_ok=True)
        with patched(SRC=src, STATE_DIR=state), quiet():
            rows = architect.rank({"units": [dict(u) for u in units]})
        return {r["unit"]: r for r in rows}


def section_rank():
    print("\n-- 4. rank(): F6a, BUILT dependents are not waiters --------------------------")
    print(f"  {'case':<44}{'expect':<10}{'got':<16}result")
    failures = []

    def check(name, expect, got, why):
        ok = expect == got
        print(f"  {name:<44}{str(expect):<10}{str(got):<16}{'ok' if ok else 'FAILED'}")
        if not ok:
            print(f"      why this case exists: {why}")
            failures.append((name, why))

    rows = drive_rank(f6a_units(["BUILT"] * 4))
    check("all dependents BUILT -> blockers", 0, rows["BRGA_WeaponFire"]["blockers"],
          "F6a exactly: four finished BP10 widgets were counted as waiting on the fire path and "
          "produced its entire blocker score, and that score produced the #1 pick")
    # VERDICT CHANGED BY A RULING, 1 Aug 2026 -- recorded rather than deleted. This case
    # expected 102 = depth 2 + blockers 0 + tier 0 + MISSING 100 while depth was ADDED. R32
    # subtracts it, so the same fixture is now -2 + 0 + 0 + 100 + readiness 0 = 98. The case
    # still fails at 106 (the four phantom waiters back) and still fails at 102 (R32 reverted),
    # which is the whole reason it is edited and not dropped.
    check("...and its total (R32: depth subtracted)", 98, rows["BRGA_WeaponFire"]["total"],
          "-depth 2 (BP03<-BP02<-BP01) + blockers 0 + tier 0 + MISSING 100 + readiness 0. 106 "
          "means the four phantom waiters are back; 102 means depth is being ADDED again")

    rows = drive_rank(f6a_units(["BUILT", "BUILT", "BUILT", "STUB"]))
    check("one dependent STUB -> blockers", 1, rows["BRGA_WeaponFire"]["blockers"],
          "the control. A filter that dropped every waiter, or a term wired to a constant, "
          "passes the two cases above and silently deletes criticality from the score")

    built = max(r["total"] for r in rows.values() if r["state"] == "BUILT")
    unbuilt = min(r["total"] for r in rows.values() if r["state"] != "BUILT")
    check("no BUILT unit outranks an unbuilt one", True, built < unbuilt,
          "STATE_SCORE's magnitudes are a GATE, not a nudge: the first draft used 2/1/0, the "
          "blocker term swamped it, and a 'what to build next' scorer returned something that "
          "was already built")

    return 4, failures


# ======================================================================================
# Section 5 -- R32/R33/R34/R35: the four rulings landed 1 Aug 2026
# ======================================================================================
#
# Each ruling was made because a REAL run came out wrong, so each gets a case shaped like the
# run that was wrong -- not a case shaped like the code that fixes it.
#
#   R32  depth was ADDED, so the score rewarded distance from a DAG root, which is distance
#        from being STARTABLE. The top pick was BRSpotterSubsystem (BP11 <- BP08 <- BP02+BP04)
#        winning on depth 4 alone, in the same table that reported rung 2 BLOCKED because
#        Tests/ was empty and ranked the three test specs 7-9. A deeper unit must now score
#        LOWER, all else equal.
#   R33  BRGA_WeaponFire ranked #1 while three of its declared inputs did not exist and the
#        builder packet stopped at law 5 on contact. Readiness is derived from DISK -- declared
#        tags, row structs, tables, columns -- and never from a model. Its most important
#        property is the one it must NOT have: it must not invent a blocker. A family
#        reference, an unmatched shape, or an absent oracle yields UNKNOWN, and UNKNOWN is
#        treated as READY.
#   R34  three bugs, one shape: `state` at 2/1/0 was swamped by a blocker term reaching 35 and
#        the score picked an already-BUILT unit; `tier` needed -100 to stop a perpetually
#        MISSING Phase-2 unit being selected; readiness needs the same. "This cannot be built"
#        is not a preference to be outvoted, so the case below stacks the preference terms as
#        high as they go and requires the gate to win anyway.
#   R35  step 6's F3: one line -- `#include "BRGA_Grenade.h"` in BRCore.h -- moved that unit
#        +27 and to #1, and the header does not exist. The pair of cases below is the sharp
#        one: IDENTICAL include line, and the only difference is whether the target header is
#        on disk. It must be worth 0 blockers absent and 1 present, or the filter is either
#        not firing or has deleted the include graph.

TAGS_CPP = '''#include "BRGameplayTags.h"

namespace BRTags
{
\tUE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Melee, "Damage.Melee", "Damage type: melee.");
\tUE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
}
'''

ROWS_H = '''#pragma once
#include "CoreMinimal.h"

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRWeaponRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    float Range_m = 0.f;
};
'''

WEAPONS_CSV = "Name,DisplayName,DamagePerShot,Spread_deg\nAR,Assault Rifle,12,1.5\n"


def readiness_tree(td, with_tags=True, with_rows=True, with_csv=True):
    """A minimal repo whose ground truth is entirely under our control."""
    src = Path(td) / "Source" / "Breachpoint"
    data = Path(td) / "Content" / "Data"
    (src / "Core").mkdir(parents=True)
    (src / "Data").mkdir(parents=True)
    data.mkdir(parents=True)
    if with_tags:
        (src / "Core" / "BRGameplayTags.cpp").write_text(TAGS_CPP, encoding="utf-8")
    if with_rows:
        (src / "Data" / "BRDataRows.h").write_text(ROWS_H, encoding="utf-8")
    if with_csv:
        (data / "DT_Weapons.csv").write_text(WEAPONS_CSV, encoding="utf-8")
    return src, data


def drive_readiness(spec, **tree):
    with tempfile.TemporaryDirectory() as td:
        src, data = readiness_tree(td, **tree)
        with patched(SRC=src, DATA_DIR=data), quiet():
            verdict, missing, unknown, refs = architect.readiness_of(spec,
                                                                     architect.ground_truth())
        return verdict, missing


# (name, §3 row text, expected verdict, why this case exists)
READINESS_CASES = [
    ("a declared tag", "| `BRX.h/.cpp` | applies `Damage.Melee` on hit. |", "ready",
     "the control. If a reference that DOES resolve came back not_ready or unknown, the term "
     "would either block everything or measure nothing"),
    ("an UNdeclared tag", "| `BRX.h/.cpp` | grants `State.Airborne` while falling. |", "not_ready",
     "the R33 case verbatim: BRGA_WeaponFire's cue tags were undeclared this morning, and the "
     "score ranked it #1 anyway because it had no term for 'its inputs do not exist'"),
    ("a declared column", "| `BRX.h/.cpp` | trace stops at `Range_m`. |", "ready",
     "`Range_m` landed in FBRWeaponRow today -- a field present in the row struct must resolve "
     "even when it is not in a CSV header, because §3 names the field, not the table"),
    ("a column present only in a CSV", "| `BRX.h/.cpp` | cone from `Spread_deg`. |", "ready",
     "the other half of the same rule: a CSV column with no C++ field yet is still an input "
     "that EXISTS, and the check must read both oracles or half of BP03's debt is invisible"),
    ("an absent column", "| `BRX.h/.cpp` | falloff over `Dropoff_m`. |", "not_ready",
     "the shape of BP03's real gap -- a tuning number the manifest names and the data does not "
     "carry. Nothing can be built against it"),
    ("an absent row struct", "| `BRX.h/.cpp` | reads `FBRGhostRow`. |", "not_ready",
     "`FBRMatchRules` vs `FBRMatchRulesRow` is one character from being live in §3 right now; "
     "a near-miss row name is a unit that cannot compile"),
    ("an absent table", "| `BRX.h/.cpp` | coefficients from `DT_Nowhere`. |", "not_ready",
     "the table check is what makes `CT_Combat` load-bearing for BRDamageExecCalc; if a missing "
     "table passed, the term would be blind to the entire Content/Data/ half of law 3"),
    ("prose only, no reference", "| `BRX.h/.cpp` | Server validates rate, ammo, cone, range. |",
     "unknown",
     "THE CAP, asserted rather than described. This is BRGA_WeaponFire's actual §3 row shape: "
     "it names its inputs in prose and the extraction cannot see them. It must come back "
     "UNKNOWN -- inventing `Range_m` out of the word 'range' would be the model choosing"),
    ("a family reference", "| `BRX.h/.cpp` | dynamic `Damage.*` tags. |", "unknown",
     "`Damage.*` names no concrete tag, so it can neither pass nor fail. Expanding a family "
     "into its members would manufacture blockers out of notation"),
    ("a placeholder reference", "| `BRX.h/.cpp` | fires `Event.<Verb>.<Moment>`. |", "unknown",
     "§3 writes generic event names this way; a literal check would report every unit that "
     "mentions the convention as blocked on a tag nobody ever intended to declare"),
]

# (name, tree kwargs, spec, expected, why)
ORACLE_CASES = [
    ("tags file absent -> UNKNOWN, not blocked", {"with_tags": False},
     "| `BRX.h/.cpp` | applies `Damage.Melee`. |", "unknown",
     "R33 says: if you cannot determine readiness, mark UNKNOWN and do NOT invent a blocker. "
     "A missing ORACLE is not a missing input -- and this is the failure mode that would bite "
     "hardest, because it turns one deleted file into 44 units that 'cannot be built'"),
    ("row header absent -> UNKNOWN, not blocked", {"with_rows": False},
     "| `BRX.h/.cpp` | reads `FBRWeaponRow`. |", "unknown",
     "same rule on the row oracle; BRDataRows.h is one file and the whole Data/ folder hangs "
     "off it"),
    ("Content/Data absent -> UNKNOWN, not blocked", {"with_csv": False, "with_rows": False},
     "| `BRX.h/.cpp` | cone from `Spread_deg`. |", "unknown",
     "the cloud runner checks out the repo without Content/ in some configurations; that must "
     "degrade to 'cannot tell', never to 'nothing is buildable'"),
]


def r32_units(ticket):
    return [{"unit": "BRProbe", "folder": "Core", "ticket": ticket,
             "state": "MISSING", "tier": "slice"}]


def r34_units(readiness, n_waiters=40):
    """One gated unit with the maximum blocker score the preference term can produce."""
    units = [{"unit": "BRBlocked", "folder": "Core", "ticket": "BP01", "state": "MISSING",
              "tier": "slice", "readiness": readiness}]
    for i in range(n_waiters):
        units.append({"unit": f"BRWaiter{i:02d}", "folder": "Core", "ticket": "BP02",
                      "state": "STUB", "tier": "slice", "readiness": "ready"})
    return units


def drive_include_edges(ghost_on_disk):
    """F3's shape, minimised: one include line, and the target header exists or does not.

    Returns (edges out of BRHost, discarded count, blockers scored for BRGhost).
    """
    units = [{"unit": "BRHost", "folder": "Core", "ticket": "BP99", "state": "STUB",
              "tier": "slice"},
             {"unit": "BRGhost", "folder": "Core", "ticket": "BP99", "state": "MISSING",
              "tier": "slice"}]
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "Source" / "Breachpoint"
        (src / "Core").mkdir(parents=True)
        (src / "Core" / "BRHost.h").write_text(
            '#pragma once\n#include "CoreMinimal.h"\n#include "BRGhost.h"\n', encoding="utf-8")
        if ghost_on_disk:
            units[1]["state"] = "STUB"
            (src / "Core" / "BRGhost.h").write_text("#pragma once\n", encoding="utf-8")
        state = Path(td) / "state"
        state.mkdir(parents=True)
        with patched(SRC=src, STATE_DIR=state), quiet():
            edges, discarded = architect.include_edges(units)
            rows = {r["unit"]: r for r in architect.rank({"units": [dict(u) for u in units]})}
        return sorted(edges["BRHost"]), len(discarded), rows["BRGhost"]["blockers"]


def section_rulings():
    print("\n-- 5. R32/R33/R34/R35: the rulings of 1 Aug 2026 -----------------------------")
    print(f"  {'case':<52}{'expect':<12}{'got':<12}result")
    failures = []

    def check(name, expect, got, why):
        ok = expect == got
        print(f"  {name:<52}{str(expect):<12}{str(got):<12}{'ok' if ok else 'FAILED'}")
        if not ok:
            print(f"      why this case exists: {why}")
            failures.append((name, why))
        return ok

    n = 0

    # ---- R32: depth is SUBTRACTED -----------------------------------------------------
    # A synthetic three-ticket chain so that DEPTH is the only term that moves: the same unit
    # is placed at depth 0, 1 and 2 and nothing else about it changes.
    chain = {"BP01": [], "BP02": ["BP01"], "BP03": ["BP02"]}
    totals, depths = [], []
    for tkt in ("BP01", "BP02", "BP03"):
        with patched(TICKET_DEPS=chain):
            row = drive_rank(r32_units(tkt))["BRProbe"]
        totals.append(row["total"])
        depths.append(row["depth"])
    n += 3
    check("R32 depth term reads 0/1/2 down the chain", [0, 1, 2], depths,
          "if the depths are not 0/1/2 the fixture is broken and the sign test below proves "
          "nothing -- this is the setup assertion, not the ruling")
    check("R32 deeper unit scores STRICTLY LOWER", True,
          totals[0] > totals[1] > totals[2],
          "the ruling itself. Depth away from a DAG root is distance from being STARTABLE; "
          "added, it made BRSpotterSubsystem (depth 4) the #1 pick over three depth-0 test "
          "specs in a table that reported rung 2 BLOCKED because Tests/ was empty")
    check("R32 depth enters with a MINUS sign", [100, 99, 98], totals,
          "the magnitudes, not just the order: MISSING 100 minus depth. If these come back "
          "100/101/102 the sign is flipped and the ordering test above still passes on a "
          "reversed comparison one edit away")

    # ---- R33: readiness, derived from disk --------------------------------------------
    for name, spec, expect, why in READINESS_CASES:
        got, missing = drive_readiness(spec)
        n += 1
        check(f"R33 {name}", expect, got, why)
    for name, tree, spec, expect, why in ORACLE_CASES:
        got, missing = drive_readiness(spec, **tree)
        n += 1
        check(f"R33 {name}", expect, got, why)

    # The live-repo probe named in the ruling. BRGA_WeaponFire was NOT ready this morning --
    # undeclared cue tags, no Range_m, no Spread_deg -- and the tags and both row fields landed
    # today. Whatever the extraction sees, it must not report the unit blocked now.
    with quiet():
        manifest = architect.parse_manifest()
        truth = architect.ground_truth()
    spec = next((e.get("spec") for f in manifest.values() for e in f["units"]
                 if e["name"] == "BRGA_WeaponFire"), None)
    verdict, missing, unknown, refs = architect.readiness_of(spec, truth)
    n += 1
    ok = check("R33 BRGA_WeaponFire is not reported blocked today", True,
               verdict != "not_ready",
               "the ruling's own ground truth: its undeclared tags were declared and both row "
               "fields landed today, so a not_ready verdict here would be the term reporting a "
               "gap that has been closed")
    named = sorted(f"{k}:{v}" for k, vs in refs.items() for v in vs)
    print(f"      verdict {verdict.upper()}; §3 row names {named or '(no checkable input)'}")
    if missing:
        print(f"      absent on disk: {', '.join(missing)}")

    # ---- R34: a gate is not a preference ----------------------------------------------
    rows = drive_rank(r34_units("not_ready"))
    blocked, waiter = rows["BRBlocked"], rows["BRWaiter00"]
    n += 3
    check("R34 the gated unit really has max blockers", 40, blocked["blockers"],
          "the setup assertion. If the blocker term were 0 here the gate would 'win' against "
          "nothing and the ruling would be untested")
    check("R34 not-ready cannot outrank ready", True, blocked["total"] < waiter["total"],
          "R34 exactly: 40 blocker points plus every preference available must not overcome "
          "'this cannot be built'. state at 2/1/0 lost to a blocker term reaching 35 once "
          "already, and the score returned a unit that was already BUILT")
    check("R34 top of the table is a READY unit", "ready",
          max(rows.values(), key=lambda r: r["total"])["readiness"],
          "the consequence that matters: the thing the architect hands to a builder")
    # Control: the ONLY change is the readiness verdict. If the gated unit does not win here,
    # the blocker term has been destroyed rather than gated, and criticality is gone from the
    # score -- which is exactly the failure a filter-everything fix would produce.
    rows = drive_rank(r34_units("ready"))
    n += 1
    check("R34 control: same fixture, readiness ready -> wins", True,
          rows["BRBlocked"]["total"] > rows["BRWaiter00"]["total"],
          "40 units genuinely waiting on it is the strongest preference signal the score has, "
          "and with the gate open it must still decide the pick")

    # ---- R35: an include edge to a header that is not on disk ---------------------------
    edges, discarded, ghost_blockers = drive_include_edges(ghost_on_disk=False)
    n += 3
    check("R35 phantom include -> no edge", [], edges,
          "F3: one line naming a header that does not exist moved its target +27 and to #1. "
          "The include cannot compile, so it is not evidence of a dependency")
    check("R35 phantom include -> counted as discarded", 1, discarded,
          "the discard must be REPORTED, not silently filtered: a phantom include is either a "
          "broken build or an attempt to move the queue, and both are findings")
    check("R35 phantom include -> 0 blocker points", 0, ghost_blockers,
          "the term F3 actually moved. If this is 1 the edge is still being scored and the "
          "score remains writable by the builder it directs")
    edges, discarded, ghost_blockers = drive_include_edges(ghost_on_disk=True)
    n += 3
    check("R35 control: real header -> edge kept", ["BRGhost"], edges,
          "the control. A filter that dropped every edge would pass all three cases above and "
          "delete the entire second dependency source the spec requires")
    check("R35 control: nothing discarded", 0, discarded,
          "a discard counter that only ever goes up is as misleading as no counter at all")
    check("R35 control: real header -> 1 blocker point", 1, ghost_blockers,
          "identical include line to the phantom case; the ONLY difference is the header being "
          "on disk, which is precisely what R35 makes the edge conditional on")

    return n, failures


# ======================================================================================

def main():
    if not ARCH.exists():
        print(f"FATAL: {ARCH} not found")
        return 1

    print("=" * 78)
    print("architect.py self-check -- RED-THEN-GREEN")
    print("=" * 78)
    print("Five sections. Sections 1-4 are one per surface the architect makes a decision on;")
    print("every case is a defect this file has actually shipped, so every case is red before")
    print("its fix and green after it. F7 (step 6) is the reason sections 2-4 exist: five")
    print("manifest cases proved nothing about classify(), rank() or blackboard(), and F4, F5")
    print("and F6a were all found in that gap. Section 5 is the four rulings of 1 Aug 2026")
    print("(R32 depth's sign, R33 readiness, R34 gates dominate, R35 include edges must point")
    print("at a header that exists) -- each shaped like the wrong RUN that forced the ruling.")

    total, failures = 0, []
    for section in (section_manifest, section_classify, section_traversal, section_rank,
                    section_rulings):
        n, f = section()
        total += n
        failures += f

    print()
    if failures:
        print(f"{len(failures)} of {total} case(s) FAILED -- the self-check does not reject what")
        print("it claims to. Named, so the failure cannot be read as flakiness:")
        for name, _ in failures:
            print(f"  - {name}")
        return 1
    print(f"All {total} cases behaved. The self-check has now been proven against cases it")
    print("must reject, not only against the one it passes -- across all four surfaces, not")
    print("only the manifest parser, and against each of the four rulings of 1 Aug 2026 by a")
    print("case shaped like the run that forced it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
