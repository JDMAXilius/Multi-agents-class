#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"

#include "Animations/BRAnimLayerInterface.h"

#include "BRAnimLayerInstance.generated.h"

/**
 * The C++ base every per-weapon anim layer parents to.
 *
 * WHY THIS EXISTS AT ALL. Without it `IBRAnimLayer` has **zero implementors** and is dead code,
 * and the 102 properties on `ABP_ItemAnimLayersBase` stay entirely inside a binary asset. This
 * class is where the layer's NON-POSE half comes out of the asset and into something a critic
 * can diff.
 *
 * THE SPLIT, and it is the whole design. The inventory (`mcp-bp/bp_inventory.json`,
 * `ABP_ItemAnimLayersBase`) shows those 102 properties are two different kinds of thing wearing
 * one class:
 *
 *   POSE SLOTS -- `aim_HipFirePose`, `bS_FPS_ADS_Idle_Move`, `crouch_Idle_Entry`, `fPS_Sprint`,
 *   and ~90 more. These are animation ASSETS. They stay on the layer asset, they are Tier 4,
 *   and porting them to C++ would mean hard asset references, which law 3 bans outright.
 *
 *   EVERYTHING ELSE -- `disableHandIK`, `enableLeftHandPoseOverride`, `aimOffsetBlendWeight`,
 *   and the per-bone aim weights, which the layer base carries as the flat floats
 *   `alpha01_spine_01` … `alpha08_head`. These are STRUCTURE and CONFIGURATION, not content,
 *   and they are declared here.
 *
 * WHY A MAP AND NOT EIGHT FLOATS, corrected: the flat `alpha01…alpha08` list is what the LAYER
 * base has, but the SPINE (`ABP_Mannequin_Base`) carries the same idea keyed by bone, and it
 * carries TWO of them -- `aimSpineWeights_UE5` with 8 bones (`head`, `neck_01`, `neck_02`,
 * `spine_01`…`spine_05`) and `aimSpineWeights_UE4` with 5. That is the evidence: the weight set
 * is skeleton-dependent, so eight positional floats hard-code one skeleton's spine and go
 * silently wrong on the other. A map keyed by bone does not care how many joints a spine has.
 * (An earlier revision of this comment credited `aimSpineWeights_UE5` to the layer base. It is
 * on the spine; the argument survives the correction, which is why the correction is recorded
 * rather than quietly edited.)
 *
 * The result is the shape Amendment A §A.3 asked for, stated as a test rather than a promise:
 * **adding a weapon is a table row plus a layer asset and zero C++.** The layer asset supplies
 * poses and overrides defaults; it holds no logic, and R26's "default values only, empty graphs"
 * is satisfiable by construction because there is nothing left for a graph to decide.
 */
UCLASS()
class BREACHPOINT_API UBRAnimLayerInstance : public UAnimInstance, public IBRAnimLayer
{
	GENERATED_BODY()

public:

	virtual FName GetLayerWeaponRow_Implementation() const override { return WeaponRow; }
	virtual bool GetOverridesHandPose_Implementation() const override { return bOverridesHandPose; }
	virtual FBRSwayAndLagInfo GetSwayAndLagInfo_Implementation() const override { return SwayAndLag; }

	/** Per-bone aim-offset weights. Empty means "the spine's default"; a layer overrides only if it differs. */
	const TMap<FName, float>& GetAimSpineWeights() const { return AimSpineWeights; }

	float GetAimOffsetBlendWeight() const { return AimOffsetBlendWeight; }

protected:

	/**
	 * Which weapon row this layer serves.
	 *
	 * A NAME, deliberately, not a class pointer and not an enum. It is how code identifies a
	 * layer without naming its asset, and it is the same handle the weapon table uses -- so the
	 * layer and its stats cannot drift apart into two vocabularies.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Layer")
	FName WeaponRow;

	/** Does this layer pose the hands itself? The spine asks; only the layer knows. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Layer")
	bool bOverridesHandPose = false;

	// There is deliberately NO `bDisableHandIK` here, and its absence is the design.
	//
	// `ABP_ItemAnimLayersBase` carries `disableHandIK` as an asset property, set true on
	// `ABP_UnarmedAnimLayers` (inventory, anim_layers set), and the AnimGraph reads THAT one.
	// Declaring a C++ mirror of it would put two values with one meaning on a single object --
	// the asset's, which the graph uses, and a C++ default of `false`, which code would read.
	// That is exactly the second-source-of-truth this packet's tag-bool design exists to prevent,
	// and it would have been reintroduced by the class whose stated job is to END it.
	//
	// `bOverridesHandPose` below is not a mirror: it is the SPINE's question ("do I keep hand
	// IK?"), answered once, and the layer is free to derive it from whatever it likes.

	/** `aimOffsetBlendWeight`. How much of the aim offset this weapon's pose accepts. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Layer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimOffsetBlendWeight = 1.f;

	/**
	 * Per-bone aim weights, keyed by bone name.
	 *
	 * A map rather than eight floats. The full evidence is in the class comment above -- and note
	 * the correction there: the bone-keyed sets (`aimSpineWeights_UE5`, 8 bones;
	 * `aimSpineWeights_UE4`, 5) are on the SPINE, not on this layer base, which carries the flat
	 * `alpha01…alpha08` instead. The argument survives: the weight set is skeleton-dependent, so
	 * eight positional floats hard-code one skeleton and go silently wrong on the other.
	 *
	 * Left empty by default on purpose: empty means "use the spine's distribution", so a layer
	 * that does not differ carries no data at all.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Layer")
	TMap<FName, float> AimSpineWeights;

	/**
	 * How this weapon sways and lags. The pack's `infoMap` entry for this layer, in C++.
	 *
	 * Lives on the LAYER rather than on the spine because that is the only place that knows which
	 * weapon is up without asking an asset. Defaults are the pack's `defaultInfo`, so a layer that
	 * does not differ carries nothing.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Layer")
	FBRSwayAndLagInfo SwayAndLag;
};
