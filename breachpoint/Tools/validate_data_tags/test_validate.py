#!/usr/bin/env python3
"""
test_validate.py -- red-then-green proof for validate_data_tags.py.

This repo's standing lesson is that an enforcement mechanism proves nothing until it
has been shown a case it should REJECT. The original defect is the exhibit: the data
crew's verifier returned PASS on DT_Weapons.csv for three days while FireCueTag named
three cue tags that no C++ declared. It passed because nobody ever fed it a broken
input and watched it stay green.

So every case here asserts an EXIT CODE from the real validator, run as a subprocess
over synthetic fixtures in a temp dir. The exit code is the gate's actual contract --
a gate that prints "DANGLING" and exits 0 is a gate that blocks nothing, and only
checking the code catches that.

The fixtures are a miniature repo (Content/Data + Source/.../BRGameplayTags.cpp +
a row-struct header). Nothing in the real tree is read or written.

  python Tools/validate_data_tags/test_validate.py

Exit 0 = all cases behaved as specified.
"""

from __future__ import annotations

import io
import os
import shutil
import subprocess
import sys
import tempfile
from typing import Dict, List, Optional, Sequence, Tuple

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

HERE = os.path.dirname(os.path.abspath(__file__))
VALIDATOR = os.path.join(HERE, "validate_data_tags.py")

PASS_CODE = 0
FAIL_CODE = 1
ERROR_CODE = 2


# --------------------------------------------------------------------------------
# Fixture material
# --------------------------------------------------------------------------------

# A registry with the three cue tags declared -- the state of the repo as of today.
REGISTRY_WITH_CUES = '''
#include "Core/BRGameplayTags.h"

namespace BRGameplayTags
{
\tUE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_AR_Fire, "GameplayCue.Weapon.AR.Fire", "AR fire FX.");
\tUE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Magnum_Fire, "GameplayCue.Weapon.Magnum.Fire", "Magnum fire FX.");
\tUE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Rocket_Fire, "GameplayCue.Weapon.Rocket.Fire", "Rocket fire FX.");
\tUE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Kinetic, "Damage.Kinetic", "Kinetic damage.");
\tUE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Explosive, "Damage.Explosive", "Explosive damage.");
}
'''

# The same file as it stood 29 Jul - 31 Jul 2026: the cue tags are DISCUSSED in a
# comment but never declared. A validator that scrapes text without stripping comments
# would call this a pass, so this doubles as the comment-stripping proof.
REGISTRY_WITHOUT_CUES = '''
#include "Core/BRGameplayTags.h"

namespace BRGameplayTags
{
\t// KNOWN GAP: DT_Weapons.csv names "GameplayCue.Weapon.AR.Fire",
\t// "GameplayCue.Weapon.Magnum.Fire" and "GameplayCue.Weapon.Rocket.Fire",
\t// but ARCHITECTURE 3.1 enumerates no GameplayCue leaves, so we refuse to invent them.
\tUE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Kinetic, "Damage.Kinetic", "Kinetic damage.");
\tUE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Explosive, "Damage.Explosive", "Explosive damage.");
}
'''

# Row structs: FireCueTag is FGameplayTag, TriggerId is FName by design.
ROW_STRUCTS = '''
#pragma once
#include "GameplayTagContainer.h"

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRWeaponRow : public FTableRowBase
{
\tGENERATED_BODY()

\t/** Player-facing name. CSV: DisplayName. */
\tUPROPERTY(EditDefaultsOnly)
\tFString DisplayName;

\t/** Damage per shot. CSV: DamagePerShot. */
\tUPROPERTY(EditDefaultsOnly)
\tfloat DamagePerShot = 0.f;

\t/** SOFT mesh ref. CSV: MeshSoftPath. */
\tUPROPERTY(EditDefaultsOnly)
\tTSoftObjectPtr<UStaticMesh> MeshSoftPath;

\t/** GameplayCue tag played on fire. CSV: FireCueTag. */
\tUPROPERTY(EditDefaultsOnly)
\tFGameplayTag FireCueTag;
};

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRMedalRow : public FTableRowBase
{
\tGENERATED_BODY()

\t/** Medal name. CSV: MedalName. */
\tUPROPERTY(EditDefaultsOnly)
\tFString MedalName;

\t/** The event that awards it, e.g. `Kill.Multi.Double`. CSV: TriggerId.
\t *  FName, not FGameplayTag, by packet instruction. */
\tUPROPERTY(EditDefaultsOnly)
\tFName TriggerId;
};
'''

WEAPON_HEADER = "Name,DisplayName,DamagePerShot,MeshSoftPath,FireCueTag"


def weapons_csv(cue_rows: Sequence[Tuple[str, str]]) -> str:
    """Build a DT_Weapons-shaped CSV from (row name, FireCueTag value) pairs."""
    lines = [WEAPON_HEADER]
    for name, cue in cue_rows:
        lines.append(
            "%s,%s Rifle,8,/Game/Weapons/%s/SM_%s.SM_%s,%s" % (name, name, name, name, name, cue)
        )
    return "\n".join(lines) + "\n"


MEDALS_CSV = (
    "RowName,MedalName,TriggerId\n"
    "M1,Double Kill,Kill.Multi.Double\n"
    "M2,Killing Spree,Kill.Spree\n"
    "M3,Headshot,Kill.Headshot\n"
)


# --------------------------------------------------------------------------------
# Harness
# --------------------------------------------------------------------------------


def write(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with io.open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def make_repo(root: str, csvs: Dict[str, str], registry: str, structs: str = ROW_STRUCTS) -> None:
    """Lay out a miniature repo the validator's default path logic will find."""
    for name, body in csvs.items():
        write(os.path.join(root, "Content", "Data", name), body)
    write(os.path.join(root, "Source", "Breachpoint", "Core", "BRGameplayTags.cpp"), registry)
    write(os.path.join(root, "Source", "Breachpoint", "Data", "BRDataRows.h"), structs)


def run_validator(root: str, extra: Optional[Sequence[str]] = None) -> Tuple[int, str]:
    """Invoke the REAL validator as a subprocess. Its exit code is the gate contract."""
    cmd = [sys.executable, VALIDATOR, "--repo-root", root]
    if extra:
        cmd.extend(extra)
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return proc.returncode, proc.stdout.decode("utf-8", errors="replace")


class Results:
    def __init__(self) -> None:
        self.passed: List[str] = []
        self.failed: List[str] = []

    def check(
        self,
        name: str,
        expectation: str,
        got_code: int,
        want_code: int,
        output: str,
        must_contain: Sequence[str] = (),
        must_not_contain: Sequence[str] = (),
    ) -> None:
        problems: List[str] = []
        if got_code != want_code:
            problems.append("exit %d, expected %d" % (got_code, want_code))
        for needle in must_contain:
            if needle not in output:
                problems.append("output missing %r" % needle)
        for needle in must_not_contain:
            if needle in output:
                problems.append("output unexpectedly contains %r" % needle)
        if problems:
            self.failed.append(name)
            print("  FAIL  %s" % name)
            print("        expected: %s" % expectation)
            for problem in problems:
                print("        problem:  %s" % problem)
            print("        ---- validator output ----")
            for line in output.splitlines():
                print("        | %s" % line)
        else:
            self.passed.append(name)
            print("  ok    %s  (exit %d -- %s)" % (name, got_code, expectation))


# --------------------------------------------------------------------------------
# Cases
# --------------------------------------------------------------------------------


def main() -> int:
    if not os.path.isfile(VALIDATOR):
        sys.stderr.write("cannot find validator at %s\n" % VALIDATOR)
        return 2

    results = Results()
    workdir = tempfile.mkdtemp(prefix="br_validate_tags_")
    print("BREACHPOINT -- validate_data_tags.py red/green proof")
    print("fixtures: %s\n" % workdir)

    try:
        # ---- GREEN: the control ------------------------------------------------
        # Every FireCueTag names a declared tag. This must pass, or every red case
        # below proves nothing (a validator that always fails is not a validator).
        print("CONTROLS (must PASS)")

        root = os.path.join(workdir, "control_pass")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([
                ("AR", "GameplayCue.Weapon.AR.Fire"),
                ("Magnum", "GameplayCue.Weapon.Magnum.Fire"),
                ("Rocket", "GameplayCue.Weapon.Rocket.Fire"),
            ])},
            REGISTRY_WITH_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "control_all_tags_declared",
            "3 declared cue tags resolve, gate opens",
            code, PASS_CODE, out,
            must_contain=[
                "VERDICT: PASS",
                "[TAG-VALUED] DT_Weapons.csv :: FireCueTag",
                "3 reference(s) checked: 3 resolve, 0 DANGLING",
            ],
            must_not_contain=["VERDICT: FAIL"],
        )

        # The false-positive control. DT_Medals' TriggerId is tag-SHAPED and none of
        # its values are declared tags -- because it is FName by design. If the
        # validator flags these, it is red on a clean repo and gets switched off.
        root = os.path.join(workdir, "control_fname_column")
        make_repo(
            root,
            {
                "DT_Weapons.csv": weapons_csv([("AR", "GameplayCue.Weapon.AR.Fire")]),
                "DT_Medals.csv": MEDALS_CSV,
            },
            REGISTRY_WITH_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "control_fname_column_is_not_a_dangle",
            "tag-shaped FName column set aside with a stated reason, not flagged",
            code, PASS_CODE, out,
            must_contain=[
                "VERDICT: PASS",
                "[NOT-A-TAG] DT_Medals.csv :: TriggerId",
                "is declared FName",
                "1 reference(s) checked: 1 resolve, 0 DANGLING",
            ],
            # None of the three tag-shaped FName values may reach the resolution
            # section at all -- they are set aside by type, not resolved-and-excused.
            must_not_contain=["VERDICT: FAIL", "Kill.Multi.Double", "Kill.Spree", "Kill.Headshot"],
        )

        # Empty / "-" cells are the authored way to say "this row has no cue".
        root = os.path.join(workdir, "control_empty_cells")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([
                ("AR", "GameplayCue.Weapon.AR.Fire"),
                ("Magnum", ""),
                ("Rocket", "-"),
            ])},
            REGISTRY_WITH_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "control_empty_and_dash_cells",
            "blank and '-' are absent references, not dangles",
            code, PASS_CODE, out,
            # 3 rows, but only 1 reference: the blank and the '-' are not references.
            must_contain=["VERDICT: PASS", "1 reference(s) checked: 1 resolve, 0 DANGLING"],
            must_not_contain=["VERDICT: FAIL"],
        )

        # UE implicitly registers ancestors of a declared tag, so this must resolve.
        root = os.path.join(workdir, "control_parent_tag")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([("AR", "GameplayCue.Weapon")])},
            REGISTRY_WITH_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "control_implicit_parent_resolves",
            "an implicit ancestor of a declared tag resolves, as it does in UE",
            code, PASS_CODE, out,
            must_contain=["RESOLVES (PARENT)"],
        )

        # ---- RED: cases it MUST reject ----------------------------------------
        print("\nCORRUPTIONS (must FAIL -- this is the part that proves the gate)")

        # RED 1: a tag that simply does not exist anywhere in C++.
        root = os.path.join(workdir, "red_dangling")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([
                ("AR", "GameplayCue.Weapon.AR.Fire"),
                ("Shotgun", "GameplayCue.Weapon.Shotgun.Fire"),
            ])},
            REGISTRY_WITH_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "red_dangling_tag",
            "an undeclared tag is DANGLING and closes the gate",
            code, FAIL_CODE, out,
            must_contain=[
                "VERDICT: FAIL",
                "GameplayCue.Weapon.Shotgun.Fire",
                "DANGLING",
                "1 DANGLING",
            ],
        )

        # RED 2: one character wrong. 'Magnum' -> 'Magnun'. This is the case a schema
        # check can never catch: the column is present, the cell is non-empty, the
        # value is a perfectly well-formed tag string. Only resolution catches it.
        root = os.path.join(workdir, "red_typo")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([
                ("AR", "GameplayCue.Weapon.AR.Fire"),
                ("Magnum", "GameplayCue.Weapon.Magnun.Fire"),
                ("Rocket", "GameplayCue.Weapon.Rocket.Fire"),
            ])},
            REGISTRY_WITH_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "red_one_character_typo",
            "a one-character typo is DANGLING, with the intended tag suggested",
            code, FAIL_CODE, out,
            must_contain=[
                "VERDICT: FAIL",
                "GameplayCue.Weapon.Magnun.Fire",
                "nearest declared tag: GameplayCue.Weapon.Magnum.Fire",
            ],
        )

        # RED 3: the historical defect, replayed. The real DT_Weapons.csv against the
        # BRGameplayTags.cpp of 29 Jul, where the cue tags exist only in a comment.
        # This is the run that should have happened three days ago.
        root = os.path.join(workdir, "red_historical")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([
                ("AR", "GameplayCue.Weapon.AR.Fire"),
                ("Magnum", "GameplayCue.Weapon.Magnum.Fire"),
                ("Rocket", "GameplayCue.Weapon.Rocket.Fire"),
            ])},
            REGISTRY_WITHOUT_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "red_historical_regression_29jul",
            "the real 29 Jul state fails: 3 dangling, and tags named only in a "
            "C++ comment do not count as declared",
            code, FAIL_CODE, out,
            must_contain=["VERDICT: FAIL", "3 DANGLING"],
        )

        # RED 4: casing. UE gameplay tags are case-insensitive on lookup but the
        # registry is the source of truth for the canonical string; a mismatch here
        # means data and code disagree about the same tag, which is worth failing.
        root = os.path.join(workdir, "red_case")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([("AR", "GameplayCue.Weapon.Ar.Fire")])},
            REGISTRY_WITH_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "red_case_mismatch",
            "a casing drift from the canonical declared string is reported",
            code, FAIL_CODE, out,
            must_contain=["VERDICT: FAIL", "GameplayCue.Weapon.Ar.Fire"],
        )

        # RED 5: a partially-good file. The gate must not be satisfied by the good rows.
        root = os.path.join(workdir, "red_mixed")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([
                ("AR", "GameplayCue.Weapon.AR.Fire"),
                ("Magnum", "GameplayCue.Weapon.Magnum.Fire"),
                ("Rocket", "GameplayCue.Weapon.Rocket.Blast"),
            ])},
            REGISTRY_WITH_CUES,
        )
        code, out = run_validator(root)
        results.check(
            "red_one_bad_row_among_good",
            "2 resolve and 1 dangles -> still FAIL",
            code, FAIL_CODE, out,
            must_contain=["3 reference(s) checked: 2 resolve, 1 DANGLING", "VERDICT: FAIL"],
        )

        # ---- Refusal: cannot-run must not read as a pass -----------------------
        print("\nREFUSALS (must ERROR, never silently pass)")

        # An empty registry would mark every reference DANGLING -- a loud but wrong
        # failure. An unparseable registry must be exit 2, distinct from both.
        root = os.path.join(workdir, "err_empty_registry")
        make_repo(
            root,
            {"DT_Weapons.csv": weapons_csv([("AR", "GameplayCue.Weapon.AR.Fire")])},
            "namespace BRGameplayTags { /* nothing declared yet */ }\n",
        )
        code, out = run_validator(root)
        results.check(
            "err_empty_registry_refuses_to_run",
            "an empty registry is a broken input (exit 2), not 34 findings",
            code, ERROR_CODE, out,
            must_contain=["refusing to run"],
        )

        root = os.path.join(workdir, "err_missing_data")
        make_repo(root, {}, REGISTRY_WITH_CUES)
        shutil.rmtree(os.path.join(root, "Content"), ignore_errors=True)
        code, out = run_validator(root)
        results.check(
            "err_missing_data_dir",
            "a missing data directory is exit 2, not a vacuous PASS",
            code, ERROR_CODE, out,
            must_contain=["data directory not found"],
        )

        # ---- The declared blind spot -------------------------------------------
        print("\nBLIND SPOT (declared limit, and the flag that closes it)")

        # A CSV no row struct binds. The validator cannot type-check its columns, so
        # by default it reports them UNBOUND and passes. This case exists so the gap
        # is a tested, named property rather than an unstated silent cap.
        unbound_csv = "Name,SomeCueTag\nRow1,GameplayCue.Totally.Invented\n"
        root = os.path.join(workdir, "unbound")
        make_repo(root, {"DT_Unknown.csv": unbound_csv}, REGISTRY_WITH_CUES)
        code, out = run_validator(root)
        results.check(
            "unbound_column_reported_not_silently_dropped",
            "no struct binding -> UNBOUND and visible, default gate stays open",
            code, PASS_CODE, out,
            must_contain=["[UNBOUND]", "UNBOUND and therefore unchecked"],
        )

        code, out = run_validator(root, ["--strict-unbound"])
        results.check(
            "unbound_column_fails_under_strict",
            "--strict-unbound promotes the blind spot to a gate failure",
            code, FAIL_CODE, out,
            must_contain=["VERDICT: FAIL (strict)"],
        )

    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    total = len(results.passed) + len(results.failed)
    print("\n" + "-" * 70)
    print("%d/%d cases behaved as specified" % (len(results.passed), total))
    if results.failed:
        print("FAILED: %s" % ", ".join(results.failed))
        return 1
    print("All controls passed and all corruptions were rejected.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
