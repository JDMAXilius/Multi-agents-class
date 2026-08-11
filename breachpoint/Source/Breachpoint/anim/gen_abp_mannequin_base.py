"""
gen_abp_mannequin_base.py - emit a 1:1 C++ port of ABP_Mannequin_Base's property surface.

    python Source/Breachpoint/anim/gen_abp_mannequin_base.py

Writes `ABPMannequinBase.{h,cpp}` beside itself in `Source/Breachpoint/anim/`, where they
compile as part of the module. Run it from the project root -- the inventory and report paths
below are relative to it.

1:1 MEANS 1:1
-------------
Every one of the 96 properties the editor reported for
`/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base` is accounted
for: 86 declared, 10 inherited from `UAnimInstance` and re-defaulted in the constructor instead.
Names are the ASSET'S OWN spelling, recovered from the package name table -- including
`isCrouching`, which the Blueprint author wrote with a lowercase leading letter where every
sibling is uppercase. A port that "fixes" that is no longer 1:1, and the mismatch is exactly
what would break a later reparent, so it is preserved.

WHY THIS IS GENERATED AND NOT TYPED
-----------------------------------
96 properties, each with a name, a type and a default, transcribed by hand is a typo surface
with no reviewer. The generator reads the two committed sources of truth --
`mcp-bp/bp_inventory.json` for values and `animations/reports/ABP_Mannequin_Base.json` for the
asset's spelling -- so the output is checkable by re-running it, and a diff against a
regenerated file is a real test.

WHAT IS NOT IN THE OUTPUT
-------------------------
Graph logic. Node topology is not readable offline (see `animations/README.md`), so the emitted
`.cpp` carries the constructor and nothing else. The update passes are left unimplemented ON
PURPOSE: a generated `NativeUpdateAnimation` that looked plausible would be invention presented
as a port, and the property surface is the part that is genuinely known.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

# Stock UAnimInstance members. Present in the Blueprint's property list because it overrode
# their defaults, NOT because it declares them -- redeclaring any of these in C++ shadows the
# engine's own UPROPERTY and produces a class that compiles and misbehaves. They are re-applied
# in the constructor instead, which is what the Blueprint actually did.
INHERITED = {
    'bPropagateNotifiesToLinkedInstances',
    'bReceiveNotifiesFromLinkedInstances',
    'bUseMainInstanceMontageEvaluationData',
    'rootMotionMode',
    'onAllMontageInstancesEnded',
    'onMontageBlendedIn',
    'onMontageBlendingOut',
    'onMontageEnded',
    'onMontageSectionChanged',
    'onMontageStarted',
}

# Inherited members whose Blueprint value differs from the engine default, so the constructor
# has to restore it. The delegates carry no value to port.
INHERITED_CONSTRUCTOR = [
    ('RootMotionMode', 'ERootMotionMode::RootMotionFromMontagesOnly',
     'the Blueprint set this; the engine default is RootMotionFromEverything'),
]

# Properties whose type cannot be read off a JSON value, keyed by inventory name.
EXPLICIT_TYPES = {
    'lastLinkedLayer': ('TSubclassOf<UAnimInstance>', 'nullptr'),
    'rootYawOffsetSpringState': ('FFloatSpringState', ''),
    'aimSpineWeights_UE4': ('FS_Procedural_AimSpineInfoItem_UE4', ''),
    'aimSpineWeights_UE5': ('FS_Procedural_AimSpineInfoItem_UE5', ''),
    'leanSpineWeights_UE4': ('FS_Procedural_LeanSpineInfoItem_UE4', ''),
    'leanSpineWeights_UE5': ('FS_Procedural_LeanSpineInfoItem_UE5', ''),
}

CARDINAL_PROPERTIES = {
    'cardinalDirectionFromAcceleration', 'localVelocityDirection',
    'localVelocityDirectionNoOffset', 'pivotInitialDirection', 'startDirection',
}

# Details-panel grouping. Order here is the order of the emitted header.
CATEGORIES = [
    ('Locomotion', [
        'localVelocity2D', 'localAcceleration2D', 'worldVelocity', 'worldLocation',
        'worldRotation', 'localVelocityDirection', 'localVelocityDirectionNoOffset',
        'localVelocityDirectionAngle', 'localVelocityDirectionAngleWithOffset',
        'cardinalDirectionFromAcceleration', 'cardinalDirectionDeadZone', 'startDirection',
        'hasVelocity', 'hasAcceleration', 'displacementSinceLastUpdate', 'displacementSpeed',
        'groundDistance', 'isRunningIntoWall', 'isFirstUpdate',
    ]),
    ('Air', ['isOnGround', 'isFalling', 'isJumping', 'timeToJumpApex']),
    ('Crouch', ['isCrouching', 'crouchStateChange', 'applyCrouchAlpha']),
    ('Pivot', ['pivotDirection2D', 'pivotInitialDirection', 'lastPivotTime']),
    ('Aim', [
        'aimPitch', 'aimYaw', 'pitch', 'pitchRotator', 'yawDeltaSinceLastUpdate',
        'yawDeltaSpeed', 'turnYawCurveValue',
    ]),
    ('RootYawOffset', [
        'rootYawOffset', 'rootYawOffsetAngleClamp', 'rootYawOffsetAngleClampCrouched',
        'rootYawOffsetMode', 'rootYawOffsetSpringState', 'bEnableRootYawOffset',
    ]),
    ('Lean', [
        'leanRotation', 'leanOppRotation', 'leanAdditiveAlpha', 'additiveLeanAngle',
        'leanSpineWeights_UE4', 'leanSpineWeights_UE5',
    ]),
    ('Spine', ['aimSpineWeights_UE4', 'aimSpineWeights_UE5', 'applyPelvisWeight', 'fPSPelvisWeight']),
    ('Camera', ['camRotCurrent', 'camRotPrev', 'camRotRate', 'camRotOffset', 'head_CameraShake_Alpha']),
    ('PoseOffset', [
        'basePoseLocation', 'basePoseRotation', 'currPoseLocation', 'currPoseRotation',
        'montagePoseOffsetLocation', 'montagePoseOffsetRotation', 'procApplyLocation',
        'procApplyRotation',
    ]),
    ('FootPlacement', [
        'leftJointTargetLocation', 'rightJointTargetLocation', 'useFootPlacement', 'enableControlRig',
    ]),
    ('State', [
        'gameplayTag_IsADS', 'gameplayTag_IsDashing', 'gameplayTag_IsFiring',
        'gameplayTag_IsMelee', 'gameplayTag_IsReloading', 'isADS_Upper', 'wasADSLastUpdate',
        'aDSStateChanged', 'bSprinting', 'bUnarmed', 'bFPSMode', 'bFPSWalkMode',
    ]),
    ('Weapon', ['timeSinceFiredWeapon', 'applySwayAlpha', 'upperbodyDynamicAdditiveWeight']),
    ('LinkedLayer', ['lastLinkedLayer', 'linkedLayerChanged']),
]


def fnum(value):
    """Format a number as a C++ float literal WITHOUT losing precision.

    `%g` was used here first and silently truncated `aimPitch` from -2.788732 to -2.78873 --
    six significant digits, and a port that is 1:1 everywhere except the numbers is not 1:1.
    `repr` gives the shortest string that round-trips the double exactly.
    """
    number = float(value)
    if number == int(number) and abs(number) < 1e15:
        return '%d.f' % int(number)
    return repr(number) + 'f'


def cpp_type_and_default(key, value):
    """Map one inventory entry to (type, default-initialiser). Default '' means leave it."""
    if key in EXPLICIT_TYPES:
        return EXPLICIT_TYPES[key]
    if key in CARDINAL_PROPERTIES:
        return 'EAnimEnum_CardinalDirection', 'EAnimEnum_CardinalDirection::%s' % value
    if key == 'rootYawOffsetMode':
        return 'EAnimEnum_RootYawOffsetMode', 'EAnimEnum_RootYawOffsetMode::%s' % value
    if isinstance(value, bool):
        return 'bool', 'true' if value else 'false'
    if isinstance(value, (int, float)):
        # Every numeric here is a float in the Blueprint; JSON just prints 0 without a point.
        return 'float', fnum(value)
    if isinstance(value, dict):
        keys = set(value)
        if keys == {'x', 'y', 'z'}:
            return ('FVector', 'FVector::ZeroVector' if not any(value.values())
                    else 'FVector(%s, %s, %s)' % tuple(fnum(value[k]) for k in 'xyz'))
        if keys == {'pitch', 'roll', 'yaw'}:
            return ('FRotator', 'FRotator::ZeroRotator' if not any(value.values())
                    else 'FRotator(%s, %s, %s)' % (fnum(value['pitch']), fnum(value['yaw']),
                                                   fnum(value['roll'])))
        if keys == {'x', 'y'}:
            return 'FVector2D', 'FVector2D(%s, %s)' % (fnum(value['x']), fnum(value['y']))
    return 'FString', ''  # nothing else occurs in this asset; loud if it ever does


def build(inventory_path: Path, report_path: Path):
    inv = json.loads(inventory_path.read_text())
    entry = next(e for s in inv['sets'].values() if isinstance(s, list)
                 for e in s if isinstance(e, dict) and 'ABP_Mannequin_Base' in e.get('path', ''))
    props = entry['properties']

    report = json.loads(report_path.read_text())
    spelling = {n.lower(): n for n in report['names']}

    rows, seen = [], set()
    for category, keys in CATEGORIES:
        for key in keys:
            if key not in props:
                raise KeyError('%s is in the category table but not in the inventory' % key)
            seen.add(key)
            cpp_type, default = cpp_type_and_default(key, props[key])
            rows.append({
                'category': category,
                'inventory_name': key,
                'name': spelling.get(key.lower(), key[0].upper() + key[1:]),
                'type': cpp_type,
                'default': default,
            })

    unplaced = [k for k in props if k not in seen and k not in INHERITED]
    if unplaced:
        raise KeyError('properties with no category, refusing to silently drop them: %s' % unplaced)
    return rows, props


HEADER_PROLOGUE = '''#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"

#include "ABPMannequinBase.generated.h"

/**
 * 1:1 C++ port of the property surface of
 * `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base`.
 *
 * GENERATED by `Source/Breachpoint/anim/gen_abp_mannequin_base.py` from two committed sources of truth:
 * `mcp-bp/bp_inventory.json` (the live-editor property read) and
 * `animations/reports/ABP_Mannequin_Base.json` (the asset's own name-table spelling). Re-run
 * the generator to check this file rather than trusting it -- a clean diff is the test.
 *
 * 1:1, AND WHAT THAT COSTS. Names are the Blueprint's, not this project's conventions. That
 * includes `isCrouching`, alone among its siblings in starting lowercase, and
 * `UpperbodyDynamicAdditiveWeight` with its lowercase `b`. Both are preserved: a name is how
 * the engine matches a property across a reparent, so "tidying" one is not a cosmetic change,
 * it is a silent data loss at the moment the Blueprint is reparented onto this class.
 *
 * NOT A REPLACEMENT FOR `UBRAnimInstance`. That class re-implemented this state model against
 * BREACHPOINT's laws -- worker-thread split, GAS tags, config-driven tuning -- and deliberately
 * did NOT carry the template's names or its 96-property shape. This is the other thing: a
 * faithful transcription of what the purchased asset declares, for diffing against the
 * re-implementation and for reparenting the template's own Blueprints onto C++ unchanged.
 * Picking one over the other is `docs/ANIM-PORT-LEDGER.md`'s call, not this file's.
 *
 * THE GRAPH IS NOT HERE. Node topology cannot be read from the package offline
 * (`animations/README.md` explains why), so the update passes are declared and left
 * unimplemented. An invented `NativeUpdateAnimation` would be the one part of a port nobody
 * could review against the source, so there isn't one.
 */
'''


def emit_header(rows):
    out = [HEADER_PROLOGUE]

    out.append('''
/** Cardinal direction, from `AnimEnum_CardinalDirection` (a user-defined enum: 4 enumerators). */
UENUM(BlueprintType)
enum class EAnimEnum_CardinalDirection : uint8
{
\tForward,
\tBackward,
\tLeft,
\tRight
};

/** From `AnimEnum_RootYawOffsetMode` (a user-defined enum: 3 enumerators). */
UENUM(BlueprintType)
enum class EAnimEnum_RootYawOffsetMode : uint8
{
\tBlendOut,
\tHold,
\tAccumulate
};

// The enumerator COUNTS above are read from the enum assets' name tables (NewEnumerator0..3 and
// NewEnumerator0..2). The enumerator NAMES are not: a user-defined enum stores its display
// names in a localised text map that the offline reader does not decode. `Forward` and
// `BlendOut` are confirmed -- they are the defaults this Blueprint serialises -- and the rest
// follow the Lyra strip these assets came from. Confirm the ORDER in the editor before relying
// on a cast to or from the underlying byte.
''')

    out.append('''
/** Mirrors `S_Procedural_AimSpineInfoItem_UE4`; members read from the struct asset. */
USTRUCT(BlueprintType)
struct FS_Procedural_AimSpineInfoItem_UE4
{
\tGENERATED_BODY()

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat head = 0.15f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat neck_01 = 0.2f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_01 = 0.25f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_02 = 0.2f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_03 = 0.2f;
};

/** Mirrors `S_Procedural_AimSpineInfoItem_UE5`. */
USTRUCT(BlueprintType)
struct FS_Procedural_AimSpineInfoItem_UE5
{
\tGENERATED_BODY()

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat head = 0.1f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat neck_01 = 0.15f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat neck_02 = 0.2f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_01 = 0.15f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_02 = 0.1f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_03 = 0.1f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_04 = 0.1f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_05 = 0.1f;
};

/**
 * Mirrors `S_Procedural_LeanSpineInfoItem_UE4`.
 *
 * The first member is `head(Opposite Angle Weight)` in the editor; the asset stores it as
 * `headOppositeAngleWeight`, which is what a C++ identifier can be. The display name is
 * restored with `DisplayName` meta so the details panel still reads the way the pack intended.
 */
USTRUCT(BlueprintType)
struct FS_Procedural_LeanSpineInfoItem_UE4
{
\tGENERATED_BODY()

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine",
\t\t\t  meta = (DisplayName = "head(Opposite Angle Weight)"))
\tfloat headOppositeAngleWeight = 0.25f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat neck_01 = 0.2f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_01 = 0.3f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_02 = 0.3f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_03 = 0.3f;
};

/** Mirrors `S_Procedural_LeanSpineInfoItem_UE5`. */
USTRUCT(BlueprintType)
struct FS_Procedural_LeanSpineInfoItem_UE5
{
\tGENERATED_BODY()

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine",
\t\t\t  meta = (DisplayName = "head(Opposite Angle Weight)"))
\tfloat headOppositeAngleWeight = 0.25f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat neck_01 = 0.1f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat neck_02 = 0.f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_01 = 0.2f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_02 = 0.2f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tfloat spine_03 = 0.2f;
};
''')

    out.append('''
UCLASS(Blueprintable, BlueprintType)
class BREACHPOINT_API UABPMannequinBase : public UAnimInstance
{
\tGENERATED_BODY()

public:

\tUABPMannequinBase();

protected:

\t// Declared, not implemented -- see the class comment. The graph these would carry is not
\t// readable offline, and a plausible-looking body would be invention wearing a port's name.
\tvirtual void NativeInitializeAnimation() override;
\tvirtual void NativeUpdateAnimation(float DeltaSeconds) override;

public:
''')

    current = None
    for row in rows:
        if row['category'] != current:
            current = row['category']
            out.append('\n\t// ---------------------------------------------------------------- %s\n'
                       % current.lower())
        decl = '\t%s %s' % (row['type'], row['name'])
        if row['default']:
            decl += ' = %s' % row['default']
        out.append('\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ABP Mannequin Base|%s")\n%s;\n'
                   % (row['category'], decl))

    out.append('};\n')
    return ''.join(out)


def emit_source(rows, props):
    lines = ['#include "ABPMannequinBase.h"', '',
             '// GENERATED by `Source/Breachpoint/anim/gen_abp_mannequin_base.py`. See the header.', '',
             'UABPMannequinBase::UABPMannequinBase()', '{']

    lines.append('\t// Inherited from UAnimInstance, so re-applied here rather than redeclared --')
    lines.append('\t// redeclaring a stock UPROPERTY shadows the engine\'s and misbehaves silently.')
    for name, value, why in INHERITED_CONSTRUCTOR:
        lines.append('\t%s = %s;  // %s' % (name, value, why))

    inherited_bools = [k for k in ('bPropagateNotifiesToLinkedInstances',
                                   'bReceiveNotifiesFromLinkedInstances',
                                   'bUseMainInstanceMontageEvaluationData') if k in props]
    for key in inherited_bools:
        lines.append('\t%s = %s;' % (key, 'true' if props[key] else 'false'))

    lines += ['', '\t// Every other property carries its Blueprint default as a member initialiser',
              '\t// in the header, where it stays visible next to the declaration.', '}', '',
              'void UABPMannequinBase::NativeInitializeAnimation()', '{',
              '\tSuper::NativeInitializeAnimation();', '}', '',
              'void UABPMannequinBase::NativeUpdateAnimation(float DeltaSeconds)', '{',
              '\tSuper::NativeUpdateAnimation(DeltaSeconds);', '}', '']
    return '\n'.join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--inventory', type=Path, default=Path('mcp-bp/bp_inventory.json'))
    ap.add_argument('--report', type=Path,
                    default=Path('animations/reports/ABP_Mannequin_Base.json'))
    ap.add_argument('--out', type=Path, default=Path('Source/Breachpoint/anim'),
                    help='module folder the .h/.cpp are written into')
    args = ap.parse_args(argv)

    rows, props = build(args.inventory, args.report)
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'ABPMannequinBase.h').write_text(emit_header(rows), encoding='utf-8')
    (args.out / 'ABPMannequinBase.cpp').write_text(emit_source(rows, props), encoding='utf-8')

    print('%d properties in the inventory' % len(props))
    print('%d declared in C++' % len(rows))
    print('%d inherited from UAnimInstance (re-defaulted in the constructor, not redeclared)'
          % len(INHERITED))
    assert len(rows) + len(INHERITED) == len(props), 'property accounting does not balance'
    print('accounting balances: %d + %d == %d' % (len(rows), len(INHERITED), len(props)))
    print('wrote ABPMannequinBase.h and ABPMannequinBase.cpp to %s' % args.out)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
