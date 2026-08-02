#!/usr/bin/env python3
# =====================================================================================
# BREACHPOINT - selftest_no_editor.py :: check the render plan with NO Unreal
#
# Serves:   docs/ui/WEAPON-RENDER-PLAN.md. Runs in plain CPython:
#               python Tools/render_weapons/selftest_no_editor.py
#
# WHAT THIS IS, AND WHAT IT IS NOT (honesty ladder, stated up front).
# It exercises render_plan.py - the half of the pipeline that has no editor in it. It proves:
#   1. the committed table parses and validates against the real Content/Data/DT_Weapons.csv;
#   2. every mesh soft path is well-formed AND byte-equal to that row's MeshSoftPath;
#   3. output names are unique, correctly shaped, and keyed on the row name;
#   4. the MESH_PATH_DRIFT / ROW_MISSING / OUT_NAME_DUPLICATE refusals actually FIRE - a gate
#      nobody has seen fail is not a gate;
#   5. the derived uu_per_px is computed correctly from synthetic bounds, that it is SHARED
#      (a small weapon stays small), and that the projection math is sane at 0/90/180 degrees.
#
# It proves NOTHING about the engine: not one `unreal` binding name, not one mesh bound, not
# whether M_FlatCol is unlit, not whether anything renders. Those are editor-rung facts and
# this spike was forbidden from launching an editor. The framing numbers this checks are only
# as good as the bounds the editor will feed them - and the ORIENTATION VALUES IN THE TABLE
# ARE STILL GUESSES no arithmetic can validate.
#
# Exit codes: 0 all cases pass · 1 a case failed.
# =====================================================================================
"""No-editor self-test for the weapon render plan."""

import copy
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import render_plan as rp  # noqa: E402

TABLE = os.path.join(rp.repo_root(), "Content", "Data", "DT_Weapons.csv")

CASES = []


def case(fn):
    CASES.append(fn)
    return fn


def errors(findings, code=None):
    return [f for f in findings
            if f["severity"] == rp.ERROR and (code is None or f["code"] == code)]


# =====================================================================================
# 1-3. the committed table
# =====================================================================================

@case
def case_committed_plan_validates():
    """The table as committed must validate against the real DT_Weapons.csv."""
    plan = rp.build_plan(table_path=TABLE)
    assert plan["errors"] == 0, [f for f in plan["findings"] if f["severity"] == "error"]
    assert len(plan["weapons"]) == 3, plan["weapons"]
    return "3 rows, 0 errors, %d warning(s), digest %s" % (plan["warnings"], plan["digest"][:12])


@case
def case_soft_paths_are_well_formed_and_match_the_table():
    """Law 3 + section 4.3: every mesh is a soft /Game object path AND is the path the row
    that equips the weapon actually names."""
    table = rp.read_source_table(TABLE)
    for w in rp.WEAPONS:
        assert rp.SOFT_PATH_RE.match(w["mesh"]), w["mesh"]
        assert "/Game/" in w["mesh"] and not w["mesh"].endswith("/"), w["mesh"]
        pkg, obj = w["mesh"].split(".", 1)
        assert pkg.rsplit("/", 1)[-1] == obj, "not a Package.Object soft path: %s" % w["mesh"]
        assert w["mesh"] == table[w["row"]], (w["row"], w["mesh"], table[w["row"]])
    return "3/3 soft paths well-formed and byte-equal to DT_Weapons.csv"


@case
def case_output_names_are_unique_and_row_keyed():
    names = [w["out"] for w in rp.WEAPONS]
    assert len(names) == len(set(names)), names
    for w in rp.WEAPONS:
        assert rp.OUT_NAME_RE.match(w["out"]), w["out"]
        assert w["out"] == "T_WeaponSil_%s" % w["row"], w["out"]
    return ", ".join(names)


@case
def case_the_framing_values_are_still_guesses():
    """Not a pass/fail of the pipeline - a TRIPWIRE. When somebody runs the spike and writes
    real orientation values in, this case fails and forces them to update the UNVERIFIED
    banner and the SPIKE CHECKLIST rather than leaving a stale 'these are guesses' warning
    on top of determined values."""
    guessing = all(w["yaw_deg"] == 0.0 and w["roll_deg"] == 0.0 and not w["flip_x"]
                   for w in rp.WEAPONS)
    assert guessing, (
        "the orientation columns are no longer all-identity, so the spike has evidently run. "
        "Good - now delete the UNVERIFIED GUESS banner in render_plan.py, cite the receipt "
        "that determined each value, and delete this tripwire case.")
    return "all 3 rows still at the identity guess (0, 0, false) - the spike has not run yet"


# =====================================================================================
# 4. the refusals - a gate nobody has seen fail is not a gate
# =====================================================================================

@case
def case_mesh_path_drift_is_refused():
    """THE assertion of section 4.3. Point a row at a different mesh; the run must refuse."""
    weapons = copy.deepcopy(rp.WEAPONS)
    weapons[0]["mesh"] = "/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol"
    f = [x.as_dict() for x in rp.validate(weapons, rp.read_source_table(TABLE))]
    assert errors(f, "MESH_PATH_DRIFT"), [x["code"] for x in f]
    return "AR pointed at the pistol mesh -> MESH_PATH_DRIFT"


@case
def case_a_missing_row_is_refused():
    """A fourth weapon with no icon must fail the run, not ship a blank slot."""
    table = rp.read_source_table(TABLE)
    table["Shotgun"] = "/Game/Weapons/Shotgun/Meshes/SM_Shotgun.SM_Shotgun"
    f = [x.as_dict() for x in rp.validate(copy.deepcopy(rp.WEAPONS), table)]
    assert errors(f, "ROW_MISSING"), [x["code"] for x in f]
    return "a 4th DT_Weapons row with no plan entry -> ROW_MISSING"


@case
def case_duplicate_and_malformed_outputs_are_refused():
    weapons = copy.deepcopy(rp.WEAPONS)
    weapons[1]["out"] = weapons[0]["out"]
    f = [x.as_dict() for x in rp.validate(weapons, rp.read_source_table(TABLE))]
    assert errors(f, "OUT_NAME_DUPLICATE"), [x["code"] for x in f]

    weapons = copy.deepcopy(rp.WEAPONS)
    weapons[0]["out"] = "WeaponSilAR"
    f = [x.as_dict() for x in rp.validate(weapons, rp.read_source_table(TABLE))]
    assert errors(f, "OUT_NAME_SHAPE"), [x["code"] for x in f]

    weapons = copy.deepcopy(rp.WEAPONS)
    weapons[0]["mesh"] = "SM_Rifle"
    f = [x.as_dict() for x in rp.validate(weapons, rp.read_source_table(TABLE))]
    assert errors(f, "MESH_PATH_SHAPE"), [x["code"] for x in f]
    return "duplicate name, wrong shape, and a non-/Game mesh path all refused"


@case
def case_an_unjustified_scale_override_is_refused():
    """Section 3.2's family rule can be departed from - but not silently."""
    weapons = copy.deepcopy(rp.WEAPONS)
    weapons[1]["scale_override"] = 1.6
    f = [x.as_dict() for x in rp.validate(weapons, rp.read_source_table(TABLE))]
    assert errors(f, "SCALE_OVERRIDE_UNJUSTIFIED"), [x["code"] for x in f]
    weapons[1]["justification"] = "magnum unreadable at true relative scale, run 2026-08-02"
    f = [x.as_dict() for x in rp.validate(weapons, rp.read_source_table(TABLE))]
    assert not errors(f, "SCALE_OVERRIDE_UNJUSTIFIED"), [x["code"] for x in f]
    return "refused without a justification, accepted with one"


@case
def case_a_per_row_canvas_is_refused():
    weapons = copy.deepcopy(rp.WEAPONS)
    weapons[2]["out_w"] = 256
    f = [x.as_dict() for x in rp.validate(weapons, rp.read_source_table(TABLE))]
    assert errors(f, "OUT_SIZE_DEPARTURE"), [x["code"] for x in f]
    return "a per-row output size -> OUT_SIZE_DEPARTURE (one canvas, as one camera)"


# =====================================================================================
# 5. the framing math - the only part of this pipeline arithmetic CAN settle
# =====================================================================================

@case
def case_projection_is_sane_at_the_cardinal_angles():
    """A 100x20x30 uu box (a plausible rifle: long in X, thin in Y, medium in Z)."""
    half = (50.0, 10.0, 15.0)
    w0, h0 = rp.project_extent(half, 0.0, 0.0)
    assert abs(w0 - 100.0) < 1e-9 and abs(h0 - 30.0) < 1e-9, (w0, h0)

    # yawed 90 deg the barrel points AT the camera: the silhouette collapses to the width of
    # the receiver. This is the shape of the failure the first render is most likely to show.
    w90, h90 = rp.project_extent(half, 90.0, 0.0)
    assert abs(w90 - 20.0) < 1e-9 and abs(h90 - 30.0) < 1e-9, (w90, h90)

    w180, h180 = rp.project_extent(half, 180.0, 0.0)
    assert abs(w180 - w0) < 1e-9 and abs(h180 - h0) < 1e-9, (w180, h180)

    # rolled 90 deg about the camera axis: width and height swap.
    wr, hr = rp.project_extent(half, 0.0, 90.0)
    assert abs(wr - 30.0) < 1e-9 and abs(hr - 100.0) < 1e-9, (wr, hr)

    # 45 deg is the conservative AABB of the rotated box - larger than either axis alone.
    w45, _ = rp.project_extent(half, 45.0, 0.0)
    assert w45 > w0 * 0.5 and w45 < w0 + 20.0, w45
    return "0/90/180/roll-90/45 all as specified (90deg yaw collapses a rifle to 20uu)"


@case
def case_uu_per_px_is_derived_from_the_largest_and_is_shared():
    """Section 3.2. Hand-computed: the launcher's 188 uu long axis must fill 96% of the
    188 px safe box, so uu_per_px = 188 / (188 * 0.96) = 1/0.96 = 1.0416667."""
    projected = [("AR", 100.0, 30.0), ("Magnum", 25.0, 18.0), ("Rocket", 188.0, 40.0)]
    uupp, driver = rp.derive_uu_per_px(projected)
    assert driver == "Rocket", driver
    assert abs(uupp - (188.0 / (rp.SAFE_W * rp.FILL_FRACTION))) < 1e-12, uupp
    assert abs(uupp - 1.0416666666) < 1e-9, uupp

    px = dict((row, w / uupp) for row, w, _h in projected)
    assert abs(px["Rocket"] - rp.SAFE_W * rp.FILL_FRACTION) < 1e-9, px
    assert abs(px["Rocket"] - 180.48) < 1e-9, px
    # THE POINT OF THE SHARED SCALAR: relative size survives. A magnum drawn at the same
    # visual weight as a launcher would be a bug the tray shows and no single icon does.
    assert abs(px["Magnum"] - 24.0) < 1e-9, px
    assert px["Magnum"] < px["AR"] < px["Rocket"], px
    assert abs(px["Magnum"] / px["Rocket"] - 25.0 / 188.0) < 1e-12, px
    return "uu/px %.7f from Rocket; Rocket %.2fpx, AR %.2fpx, Magnum %.2fpx (ratios preserved)" \
           % (uupp, px["Rocket"], px["AR"], px["Magnum"])


@case
def case_ortho_width_covers_the_canvas_not_the_safe_box():
    """The camera's OrthoWidth spans the full 192 px canvas - the 2 px margin is part of the
    picture, not cropped out of it. Getting this wrong is a silent 2% scale error."""
    w = dict(rp.WEAPONS[2])
    frame = rp.frame_row(w, (94.0, 10.0, 20.0), 1.0)
    assert abs(frame["ortho_width_uu"] - rp.CANVAS_W * 1.0) < 1e-9, frame
    assert frame["render_px"] == [rp.CANVAS_W * rp.SUPERSAMPLE, rp.CANVAS_H * rp.SUPERSAMPLE]
    assert abs(frame["projected_px"][0] - 188.0) < 1e-9, frame
    assert abs(frame["fill_fraction_w"] - 1.0) < 1e-9, frame
    return "ortho width %.1f uu over the %dpx canvas, render %s" \
           % (frame["ortho_width_uu"], rp.CANVAS_W, frame["render_px"])


@case
def case_scale_override_reads_the_way_it_is_named():
    """The documented departure: scale_override is a SIZE multiplier (2.0 = twice as big),
    implemented as a divisor on uu_per_px. If this ever inverts, a knob turned to make the
    magnum readable will make it invisible instead."""
    assert rp.row_uu_per_px(1.0, None) == 1.0
    assert rp.row_uu_per_px(1.0, 2.0) == 0.5, "2.0 must draw the weapon LARGER"
    w = dict(rp.WEAPONS[1])
    w["scale_override"] = 2.0
    plain = rp.frame_row(rp.WEAPONS[1], (25.0, 5.0, 8.0), 1.0)
    bigger = rp.frame_row(w, (25.0, 5.0, 8.0), 1.0)
    assert bigger["projected_px"][0] == plain["projected_px"][0] * 2.0
    return "scale_override 2.0 -> 2x the pixels"


@case
def case_degenerate_bounds_refuse_rather_than_divide_by_zero():
    try:
        rp.derive_uu_per_px([("AR", 0.0, 0.0)])
    except rp.PlanError as exc:
        return "PlanError: %s" % str(exc).split(";")[0]
    raise AssertionError("a zero-width set must raise, not return a plausible number")


# =====================================================================================
# main
# =====================================================================================

def main():
    print("=" * 88)
    print("BREACHPOINT weapon render plan :: NO-EDITOR self-test")
    print("=" * 88)
    print("table : %s" % TABLE)
    print("")

    failures = 0
    for fn in CASES:
        name = fn.__name__.replace("case_", "")
        try:
            detail = fn()
            print("PASS  %-46s %s" % (name, detail or ""))
        except AssertionError as exc:
            failures += 1
            print("FAIL  %-46s %s" % (name, exc))
        except Exception as exc:  # noqa: BLE001
            failures += 1
            import traceback
            print("ERROR %-46s %s" % (name, exc))
            traceback.print_exc()

    print("")
    print("%d/%d cases passed." % (len(CASES) - failures, len(CASES)))
    print("RUNG: this is arithmetic and a CSV, checked with no engine. It proves the plan is")
    print("      self-consistent and the framing math is right GIVEN correct bounds. It proves")
    print("      nothing about Unreal, and it cannot tell you the orientation guesses are")
    print("      wrong - only a first render can. See WEAPON-RENDER-PLAN.md section 11.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
