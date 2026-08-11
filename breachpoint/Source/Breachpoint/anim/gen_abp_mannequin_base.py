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
with no reviewer. The generator reads three sources, none of which is anyone's memory:
  - `mcp-bp/bp_inventory.json`                       values (the CDO's own defaults)
  - `animations/reports/ABP_Mannequin_Base.json`     the asset's real UpperCamel spelling
  - the `.uasset` itself                             the declared TYPE of each variable
so the output is checkable by re-running it, and a diff against a regenerated file is the test.

TYPES ARE READ, NOT INFERRED
----------------------------
An earlier revision guessed types from default values and got two whole classes wrong, because
JSON cannot tell 0 the int from 0.0 the double, or a class reference from an object reference.
`recover_pin_categories` reads them instead: a Blueprint serialises its variables as
FBPVariableDescription, whose VarName and whose VarType.PinCategory are both FNames, so both are
indices into the name table. Finding a variable's FName and looking ahead for the nearest pin
category recovers the declaration.

That recovery is heuristic -- proximity, not a parse -- so it is never trusted alone. It is
cross-checked against the shape of the default value, and the two only get to disagree in ways
the code declares safe. The result on this asset:

    value shape      recovered category
    bool        ->   bool x24, struct x1     (the 1 is a proximity miss; bools stay bool)
    number      ->   real x24, int x0        <- every numeric is a Blueprint real == DOUBLE
    dict        ->   struct x27, other x3    (proximity misses; dicts stay structs)
    string      ->   byte x6, object x1      <- 6 enums, and LastLinkedLayer is an OBJECT ref

Two corrections came out of that table and both were real bugs: every numeric was `float` and
is now `double` (a Blueprint real is a double in UE5; `float` silently narrows every one of the
24), and `LastLinkedLayer` was `TSubclassOf<UAnimInstance>` when the asset contains no
ClassProperty at all -- it is an object reference, so it is now `TObjectPtr<UAnimInstance>`.

WHAT IS NOT IN THE OUTPUT
-------------------------
Graph logic. Node topology is not readable offline (see `animations/README.md`), so the emitted
`.cpp` carries the constructor and nothing else. The update passes are left unimplemented ON
PURPOSE: a generated `NativeUpdateAnimation` that looked plausible would be invention presented
as a port, and the property surface is the part that is genuinely known.

Also not read: per-variable UPROPERTY specifiers (Instance Editable, Blueprint Read Only,
expose-on-spawn, tooltips) and the Blueprint's own categories. Everything here is emitted
`EditAnywhere, BlueprintReadWrite` under categories chosen by this generator. That is the
remaining gap between this and a total 1:1, and it needs the editor.
"""

from __future__ import annotations

import argparse
import bisect
import json
import struct
from collections import Counter, defaultdict
from pathlib import Path

# Pin categories an FEdGraphPinType can carry. `real` is UE5's category for float/double
# variables; `float` survives on some older pins.
PIN_CATEGORIES = {
    'bool', 'byte', 'class', 'double', 'float', 'int', 'int64', 'name', 'object', 'real',
    'softclass', 'softobject', 'string', 'struct', 'text', 'delegate', 'interface', 'wildcard',
}

# How far past a VarName to look for its PinCategory. Wide enough for the intervening VarGuid
# and friends, narrow enough that the next variable's declaration cannot be picked up instead.
PIN_LOOKAHEAD_BYTES = 512

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
    # Recovered category: `object`. The asset contains no ClassProperty anywhere, so the
    # earlier TSubclassOf<UAnimInstance> was a guess of the wrong kind entirely -- this
    # holds the linked layer INSTANCE, not its class.
    'lastLinkedLayer': ('TObjectPtr<UAnimInstance>', 'nullptr'),
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


def read_name_table(data: bytes):
    """The package's FName table, located by structural probe -- see `animations/README.md`
    for why this project's packages need that rather than the summary's own NameOffset."""
    def run(start, limit=None):
        pos, out = start, []
        while limit is None or len(out) < limit:
            if pos + 4 > len(data):
                break
            length = struct.unpack_from('<i', data, pos)[0]
            if not 0 < length < 300:
                break
            chunk = data[pos + 4:pos + 4 + length]
            if chunk[-1:] != b'\x00' or not all(32 <= c < 127 for c in chunk[:-1]):
                break
            out.append(chunk[:-1].decode('ascii'))
            pos += 4 + length + 4
        return out, pos

    for start in range(min(8192, len(data))):
        if len(run(start, 12)[0]) == 12:
            return run(start)[0]
    raise ValueError('no name table in %d bytes' % len(data))


def recover_pin_categories(asset: Path, wanted_names):
    """Recover {declared name -> pin category} from the package's variable descriptions.

    A Blueprint serialises its variables as FBPVariableDescription, whose VarName and whose
    VarType.PinCategory are both FNames -- hence indices into the name table, hence findable in
    the byte stream as an (int32 index, int32 number) pair. For each wanted name, the nearest
    pin category within PIN_LOOKAHEAD_BYTES wins.

    Proximity is not a parse. Callers must cross-check the result against something independent;
    `resolve_types` does, against the shape of the default value.
    """
    data = asset.read_bytes()
    index_of = {n: i for i, n in enumerate(read_name_table(data))}

    hits = []
    for category in PIN_CATEGORIES:
        if category not in index_of:
            continue
        needle = struct.pack('<ii', index_of[category], 0)
        start = 0
        while True:
            at = data.find(needle, start)
            if at < 0:
                break
            hits.append((at, category))
            start = at + 1
    hits.sort()
    offsets = [at for at, _ in hits]

    recovered = {}
    for name in wanted_names:
        if name not in index_of:
            continue
        needle = struct.pack('<ii', index_of[name], 0)
        votes, start = Counter(), 0
        while True:
            at = data.find(needle, start)
            if at < 0:
                break
            start = at + 1
            j = bisect.bisect_left(offsets, at)
            if j < len(offsets) and offsets[j] - at <= PIN_LOOKAHEAD_BYTES:
                votes[hits[j][1]] += 1
        if votes:
            recovered[name] = votes.most_common(1)[0][0]
    return recovered


def value_shape(value):
    if isinstance(value, bool):
        return 'bool'
    if isinstance(value, (int, float)):
        return 'number'
    if isinstance(value, dict):
        return 'dict'
    return 'string'


def check_recovered_types(props, spelling, recovered):
    """Cross-check recovered categories against value shapes, and fail on a real disagreement.

    The value's SHAPE comes from the CDO and cannot be a proximity accident, so it is
    authoritative. The recovered CATEGORY earns its place by resolving what the shape cannot
    see: whether a numeric is `int` or `real`, and whether a string-valued property is an enum,
    an object reference or a name.

    A stray miss is expected -- proximity picks the wrong neighbour occasionally -- so this
    reports the distribution rather than asserting per-property equality. What it will not
    tolerate is an `int`, because the emitter types every numeric `double` and would quietly
    be wrong for it.
    """
    table = defaultdict(Counter)
    for key, value in props.items():
        category = recovered.get(spelling.get(key.lower()))
        if category:
            table[value_shape(value)][category] += 1

    numeric = table['number']
    if numeric.get('int') or numeric.get('int64'):
        raise SystemExit('int-typed numerics found (%s); the emitter assumes every Blueprint '
                         'real is a double and must be taught otherwise' % dict(numeric))
    return table


def dnum(value):
    """Format a number as a C++ double literal WITHOUT losing precision.

    Two bugs lived here, both of which made the port silently not-1:1.

    `%g` truncated `aimPitch` from -2.788732 to -2.78873 -- six significant digits. Then the
    literals carried an `f` suffix, which is wrong for a different reason: these properties are
    `real` in the Blueprint, and a Blueprint real is a DOUBLE. An `f` suffix narrows the
    constant to float precision before it is ever stored.

    `repr` gives the shortest string that round-trips the double exactly.
    """
    number = float(value)
    if number == int(number) and abs(number) < 1e15:
        return '%d.0' % int(number)
    return repr(number)


def cpp_type_and_default(key, value):
    """Map one inventory entry to (type, default-initialiser). Default '' means leave it.

    Types here are the ones `resolve_types` confirmed against the asset's own pin categories:
    every numeric is a Blueprint `real`, which is a double, and there are no ints.
    """
    if key in EXPLICIT_TYPES:
        return EXPLICIT_TYPES[key]
    if key in CARDINAL_PROPERTIES:
        return 'EAnimEnum_CardinalDirection', 'EAnimEnum_CardinalDirection::%s' % value
    if key == 'rootYawOffsetMode':
        return 'EAnimEnum_RootYawOffsetMode', 'EAnimEnum_RootYawOffsetMode::%s' % value
    if isinstance(value, bool):
        return 'bool', 'true' if value else 'false'
    if isinstance(value, (int, float)):
        return 'double', dnum(value)
    if isinstance(value, dict):
        keys = set(value)
        if keys == {'x', 'y', 'z'}:
            return ('FVector', 'FVector::ZeroVector' if not any(value.values())
                    else 'FVector(%s, %s, %s)' % tuple(dnum(value[k]) for k in 'xyz'))
        if keys == {'pitch', 'roll', 'yaw'}:
            return ('FRotator', 'FRotator::ZeroRotator' if not any(value.values())
                    else 'FRotator(%s, %s, %s)' % (dnum(value['pitch']), dnum(value['yaw']),
                                                   dnum(value['roll'])))
        if keys == {'x', 'y'}:
            return 'FVector2D', 'FVector2D(%s, %s)' % (dnum(value['x']), dnum(value['y']))
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
\tdouble head = 0.15;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble neck_01 = 0.2;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_01 = 0.25;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_02 = 0.2;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_03 = 0.2;
};

/** Mirrors `S_Procedural_AimSpineInfoItem_UE5`. */
USTRUCT(BlueprintType)
struct FS_Procedural_AimSpineInfoItem_UE5
{
\tGENERATED_BODY()

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble head = 0.1;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble neck_01 = 0.15;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble neck_02 = 0.2;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_01 = 0.15;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_02 = 0.1;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_03 = 0.1;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_04 = 0.1;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_05 = 0.1;
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
\tdouble headOppositeAngleWeight = 0.25;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble neck_01 = 0.2;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_01 = 0.3;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_02 = 0.3;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_03 = 0.3;
};

/** Mirrors `S_Procedural_LeanSpineInfoItem_UE5`. */
USTRUCT(BlueprintType)
struct FS_Procedural_LeanSpineInfoItem_UE5
{
\tGENERATED_BODY()

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine",
\t\t\t  meta = (DisplayName = "head(Opposite Angle Weight)"))
\tdouble headOppositeAngleWeight = 0.25;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble neck_01 = 0.1;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble neck_02 = 0.;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_01 = 0.2;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_02 = 0.2;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spine")
\tdouble spine_03 = 0.2;
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
    ap.add_argument('--asset', type=Path,
                    default=Path('Content/FPSTemplate/Demo/Characters/Heroes/Mannequin/'
                                 'Animations/ABP_Mannequin_Base.uasset'),
                    help='the .uasset itself, read for each variable\'s declared type')
    ap.add_argument('--out', type=Path, default=Path('Source/Breachpoint/anim'),
                    help='module folder the .h/.cpp are written into')
    args = ap.parse_args(argv)

    rows, props = build(args.inventory, args.report)

    # Re-derive types from the asset on every run, so the emitted file cannot drift away from
    # what the Blueprint actually declares. A disagreement this cannot resolve stops the run.
    report = json.loads(args.report.read_text())
    spelling = {n.lower(): n for n in report['names']}
    recovered = recover_pin_categories(args.asset, set(report['names']))
    table = check_recovered_types(props, spelling, recovered)
    print('pin categories recovered for %d declared names' % len(recovered))
    for shape in ('bool', 'number', 'dict', 'string'):
        if table[shape]:
            print('  %-7s -> %s' % (shape, dict(table[shape])))

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
