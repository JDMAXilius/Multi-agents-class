#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Animations/BRSwayAndLagTypes.h"

#include "BRAnimLayerInterface.generated.h"

/**
 * The CODE half of the layer contract that `ALI_ItemAnimLayers` used to hold alone.
 *
 * WHAT THIS IS NOT, said first so nobody is misled by the name. This is **not** an Anim Layer
 * Interface. UE 5.8 has no C++ path to declare one: an `AnimLayerInterface` asset's members are
 * AnimGraph layer *functions*, `AnimBlueprint.h` exposes no interface flag, and R18 already
 * names AnimGraph graphs as the one Tier 4 thing C++ cannot express. The pose-producing half of
 * the contract stays an asset because the engine leaves no alternative -- see
 * `docs/ANIM-PORT-LEDGER.md`, "The one thing that could not move".
 *
 * WHAT IT IS. Everything about a layer that is NOT a pose: what the layer is, and what code may
 * ask it. Amendment A §A.3's actual goal was never "delete the asset" -- it was
 *
 *     "the C++ hard-refs an AnimInstance class per weapon, which collides with governing idea 5
 *      of the rework -- C++ knows tags and row handles, never an asset."
 *
 * That goal is met here and it is checkable: grep this folder for `ABP_`. The layer class
 * arrives as a soft class off the weapon row, is handed to
 * `USkeletalMeshComponent::LinkAnimClassLayers()`, and no C++ file names an asset.
 *
 * Adding a weapon therefore stays a row plus a layer asset and ZERO C++, which is the whole
 * reason §A.3 rejected the Shooter template's swap-the-whole-AnimInstance shape.
 *
 * WHY `BlueprintNativeEvent` AND NOT PURE VIRTUAL C++. Because of who implements it: the layer
 * IS an AnimBlueprint, so the implementor is an asset, and a pure-virtual `BlueprintCallable`
 * would be an interface no layer could satisfy. UHT says the same thing as a warning, and this
 * project builds UHT with `-WarningsAsErrors`, so the first compile refused the pure-virtual
 * shape outright. Each function carries a C++ default, so a layer answers only what it differs on.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UBRAnimLayer : public UInterface
{
	GENERATED_BODY()
};

class BREACHPOINT_API IBRAnimLayer
{
	GENERATED_BODY()

public:

	/**
	 * Which weapon row this layer was linked for.
	 *
	 * Exists so a layer can be IDENTIFIED without comparing classes. Code that asks "is the
	 * rifle layer up?" by testing `IsA(ABP_RifleAnimLayers)` has re-introduced the asset
	 * dependency this interface exists to remove.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Breachpoint|Animation")
	FName GetLayerWeaponRow() const;
	virtual FName GetLayerWeaponRow_Implementation() const { return NAME_None; }

	/**
	 * Does this layer drive the hands, or does the spine keep hand IK?
	 *
	 * `ABP_ItemAnimLayersBase` carries `disableHandIK` and `enableLeftHandPoseOverride`
	 * (inventory, anim_spine set). Those are the layer's answer to a question the SPINE has to
	 * ask, which makes it a contract member rather than a private detail.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Breachpoint|Animation")
	bool GetOverridesHandPose() const;
	virtual bool GetOverridesHandPose_Implementation() const { return false; }

	/**
	 * This weapon's sway and lag profile.
	 *
	 * **This is what makes "sway is a property of the weapon" true rather than aspirational.** The
	 * spine previously held one `FBRSwayAndLagInfo` as a class-wide config member and its comment
	 * claimed the per-weapon profile was in force; nothing supplied one, so it was still a single
	 * profile for every gun — sourced from an ini instead of from hardcoded scalars, which is not
	 * the same thing as fixed.
	 *
	 * The source pack's `infoMap` carries nine distinct entries, and the spread is real:
	 * `lag_Rotation_Multiply` is 5 on the knife and unarmed, 11 on one pistol, 12 on the rest. A
	 * knife and a rifle should not trail the camera identically, and with this they do not.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Breachpoint|Animation")
	FBRSwayAndLagInfo GetSwayAndLagInfo() const;
	virtual FBRSwayAndLagInfo GetSwayAndLagInfo_Implementation() const { return FBRSwayAndLagInfo(); }
};
