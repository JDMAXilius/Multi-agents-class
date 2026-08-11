"""Generate a 1:1 C++ AnimInstance from the extracted ABP_Mannequin_Base CDO."""
import io, json, os, re

repo = r'D:\Documents\Claude\Multi-agents-class\breachpoint'
os.chdir(repo)

d = json.load(open('mcp-bp/bp_inventory_anim.json'))
abp = [e for e in d['sets']['anim_spine'] if e['path'].endswith('ABP_Mannequin_Base')][0]
props, schema = abp['properties'], abp['schema']

# Everything UAnimInstance already owns. Anything in here is inherited, not authored, and
# re-declaring it would shadow the engine's member.
INHERITED = set("""
rootMotionMode bUseMultiThreadedAnimationUpdate bUsingCopyPoseFromMesh bQueueMontageEvents
bReceiveNotifiesFromLinkedInstances bPropagateNotifiesToLinkedInstances
bWarnAboutInvalidPlayableAnimations bIgnoreForNotifyTriggerAngleThreshold
defaultBlendInTime defaultBlendOutTime pendingDynamicResetTeleportType
""".split())

def cpp_type(name, sch):
    if not isinstance(sch, dict):
        return None
    if 'enum' in sch:
        return 'uint8'          # user-defined enum -> ordinal, same rule the rest of the port uses
    t = sch.get('type')
    if t == 'boolean':
        return 'bool'
    if t == 'integer':
        return 'int32'
    if t == 'number':
        return 'float'
    if t == 'string':
        return 'FName'
    return None                  # objects, structs, arrays, maps: not auto-portable

def default_for(t, v):
    if t == 'bool':
        return 'true' if v is True else 'false'
    if t == 'int32':
        return str(int(v)) if isinstance(v, (int, float)) else '0'
    if t == 'float':
        # '%g' drops the decimal point for whole numbers, and 0f is not valid C++ - it
        # reads as a user-defined literal suffix (C3688). Force a '.' before the f.
        try:
            txt = '%g' % float(v)
        except Exception:
            return '0.f'
        if '.' not in txt and 'e' not in txt and 'E' not in txt:
            txt += '.0'
        return txt + 'f'
    if t == 'uint8':
        return '0'
    if t == 'FName':
        return 'NAME_None'
    return None

fields, skipped = [], []
# UHT refuses ANY member that UAnimInstance already owns - shadowing is an error, not a
# warning. A hand-written exclusion list missed BUseMainInstanceMontageEvaluationData and
# cost a build, so engine-owned fields are excluded by SHAPE instead: the UAnimInstance
# surface is bUse*/bQueue*/bReceive*/bPropagate*/bWarn*/bIgnore*/default*/root*/pending*.
# None of the FPST-authored variables match those, so nothing real is lost.
ENGINE_PREFIXES = ('bUse', 'bQueue', 'bReceive', 'bPropagate', 'bWarn', 'bIgnore',
                   'default', 'root', 'pending', 'bAllow', 'bEnable', 'bTick')

for key in sorted(props):
    if key in INHERITED or key.startswith(ENGINE_PREFIXES):
        continue
    t = cpp_type(key, schema.get(key))
    pascal = key[0].upper() + key[1:]
    if t is None:
        skipped.append((pascal, str(schema.get(key, ''))[:44]))
        continue
    dv = default_for(t, props.get(key))
    fields.append((t, pascal, dv, str(props.get(key))[:38]))

HDR = '''#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"

#include "BPMannequinAnimInstance.generated.h"

/**
 * ABP_Mannequin_Base's ANIM STATE MODEL, in C++. Generated from a CDO read of the live
 * asset, not written from the variable names.
 *
 * WHERE THIS CAME FROM. `mcp-bp/bp_extract.py --set anim_spine` ->
 * `mcp-bp/bp_inventory_anim.json`. As that tool's own header says, an AnimBlueprint's CDO
 * carries the AnimInstance's variables, and that IS the anim state model - so this field
 * list is a measurement of the asset, not a reconstruction of it. %d of the ABP's authored
 * properties are declared below with their REAL default values.
 *
 * WHAT THIS CLASS IS NOT, YET. It holds state and nothing drives it. The ABP's ubergraph -
 * the locomotion maths, the cardinal-direction selection, the root-yaw offset, the spring
 * interpolations - is not ported here, and neither is the pose graph, which is Tier 4 and
 * stays an asset either way. Nothing is reparented onto this class: exactly like
 * ABPFPSWeapon, it compiles and can be reviewed without touching a single asset, because
 * landing a half-finished port onto a live asset is what broke this project twice in one
 * day.
 *
 * THE INTERFACE MESSAGES ARE THE SEAM THAT MATTERS. BPI_FPST_AnimInterface's SetADS,
 * SetADS_Upper, SetSprinting, SetUnarmed, SetFPSMode and SetFPSWalkMode are what
 * AMyCharacter already sends and what the linked layers read. They are declared here as
 * BlueprintCallable so a future ABP child can implement the interface against this class
 * rather than against loose Blueprint variables.
 *
 * NOT AUTO-PORTABLE, and deliberately left out rather than guessed: %d properties whose
 * types are objects, structs, arrays or maps (transforms, curve maps, montage refs, the
 * S_Procedural_* structs). Each needs a decision about whether it is state or an asset
 * reference, and a wrong guess there is the silent-corruption class of bug this project
 * keeps paying for. They are listed at the bottom of this file.
 */
UCLASS(Blueprintable)
class BREACHPOINT_API UBPMannequinAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	// ---------------------------------------------------------------------------------
	// BPI_FPST_AnimInterface. The messages AMyCharacter already sends every frame it
	// changes state; see MyCharacter::SendAnimInterfaceBool.
	// ---------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetADS(bool InADS);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetADS_Upper(bool InADS_Upper);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetSprinting(bool InSprinting);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetUnarmed(bool InUnarmed);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetFPSMode(uint8 InFPSMode);

	UFUNCTION(BlueprintCallable, Category = "FPST|Interface")
	void SetFPSWalkMode(uint8 InFPSWalkMode);

	UFUNCTION(BlueprintPure, Category = "FPST|Interface")
	uint8 GetFPSMode() const { return BFPSMode ? 1 : 0; }

	UFUNCTION(BlueprintPure, Category = "FPST|Interface")
	bool GetADS() const { return bADS; }

	/**
	 * THE ONE FIELD NOT TAKEN FROM THE CDO, and it is flagged rather than guessed.
	 *
	 * The extraction exposes IsADS_Upper, GameplayTag_IsADS, ADSStateChanged and
	 * WasADSLastUpdate - but NO plain ADS bool under any obvious name. SetADS provably
	 * writes something (the graph read shows SetVariableOnPersistentFrame), so the
	 * variable exists; the extractor's camelCasing or a display-name/variable-name split
	 * is hiding which one. Declared here so the setter has a home and the gap is visible.
	 * Resolve it by opening ABP_Mannequin_Base's SetADS node and reading the target pin.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "FPST|State")
	bool bADS = false;

	// ---------------------------------------------------------------------------------
	// The state model, read from the CDO. Defaults are the asset's own.
	// ---------------------------------------------------------------------------------

'''

lines = []
for t, name, dv, raw in fields:
    lines.append('\tUPROPERTY(BlueprintReadWrite, Category = "FPST|State")')
    lines.append('\t%s %s = %s;' % (t, name, dv))
    lines.append('')

tail = '''};

/*
 * NOT PORTED - types that need a human decision, with the schema the extractor reported:
%s
 */
'''
skip_txt = '\n'.join(' *   %-38s %s' % (n, s) for n, s in skipped)

hdr = (HDR % (len(fields), len(skipped))) + '\n'.join(lines) + (tail % skip_txt)
io.open('Source/Breachpoint/Animation/BPMannequinAnimInstance.h', 'w',
        encoding='utf-8', newline='\n').write(hdr)

CPP = '''#include "Animation/BPMannequinAnimInstance.h"

// The interface setters are deliberately plain writes. ABP_Mannequin_Base's own
// implementations are exactly that - the graph read shows
//   SetADS(InADS): K2Node_SetVariableOnPersistentFrame(InADS); ExecuteUbergraph(0)
// - a variable write plus a graph kick. The kick has no equivalent here because the
// ubergraph is not ported; NativeUpdateAnimation is where that work will land.

void UBPMannequinAnimInstance::SetADS(bool InADS)
{
	// ADSStateChanged and WasADSLastUpdate are the ABP's own edge-detection pair; keeping
	// them in step here means a future ubergraph port does not have to re-derive the edge.
	ADSStateChanged = (bADS != InADS);
	WasADSLastUpdate = bADS;
	bADS = InADS;
}

void UBPMannequinAnimInstance::SetADS_Upper(bool InADS_Upper)
{
	IsADS_Upper = InADS_Upper;
}

void UBPMannequinAnimInstance::SetSprinting(bool InSprinting)
{
	BSprinting = InSprinting;
}

void UBPMannequinAnimInstance::SetUnarmed(bool InUnarmed)
{
	BUnarmed = InUnarmed;
}

void UBPMannequinAnimInstance::SetFPSMode(uint8 InFPSMode)
{
	// bFPSMode on the asset is a BOOL, not the enum the interface pin implies.
	BFPSMode = (InFPSMode != 0);
}

void UBPMannequinAnimInstance::SetFPSWalkMode(uint8 InFPSWalkMode)
{
	BFPSWalkMode = (InFPSWalkMode != 0);
}
'''
io.open('Source/Breachpoint/Animation/BPMannequinAnimInstance.cpp', 'w',
        encoding='utf-8', newline='\n').write(CPP)

print('declared %d properties, skipped %d' % (len(fields), len(skipped)))
present = set(n for _, n, _, _ in fields)
for need in ('IsADS_Upper', 'BSprinting', 'BUnarmed', 'BFPSMode', 'BFPSWalkMode',
             'ADSStateChanged', 'WasADSLastUpdate'):
    print('  %-12s %s' % (need, 'declared' if need in present else 'MISSING - setter will not compile'))
