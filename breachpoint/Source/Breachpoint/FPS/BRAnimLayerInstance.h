#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"

#include "FPS/BRAnimLayerInterface.h"

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
 *   and the per-bone aim weights (`alpha01_spine_01` … `alpha08_head`, which the base carries
 *   as `aimSpineWeights_UE5` = {head 0.1, neck_01 0.15, neck_02 0.2, spine_01 0.15,
 *   spine_02..05 0.1}). These are STRUCTURE and CONFIGURATION, not content, and they are
 *   declared here.
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

	/** Per-bone aim-offset weights. Empty means "the spine's default"; a layer overrides only if it differs. */
	const TMap<FName, float>& GetAimSpineWeights() const { return AimSpineWeights; }

	bool GetDisableHandIK() const { return bDisableHandIK; }
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

	/** `disableHandIK` on `ABP_ItemAnimLayersBase`. A knife needs it; a rifle does not. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Layer")
	bool bDisableHandIK = false;

	/** `aimOffsetBlendWeight`. How much of the aim offset this weapon's pose accepts. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Layer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimOffsetBlendWeight = 1.f;

	/**
	 * Per-bone aim weights, keyed by bone name.
	 *
	 * A map rather than eight floats because that is the shape the source actually had --
	 * `aimSpineWeights_UE5` came back from the inventory as an object keyed by bone, and it
	 * carried a DIFFERENT bone set than `aimSpineWeights_UE4` (8 bones vs 5). Eight named float
	 * members would have hard-coded one skeleton's spine into C++ and broken silently on the
	 * other -- the map does not care how many spine joints a skeleton has.
	 *
	 * Left empty by default on purpose: empty means "use the spine's distribution", so a layer
	 * that does not differ carries no data at all.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Layer")
	TMap<FName, float> AimSpineWeights;
};
