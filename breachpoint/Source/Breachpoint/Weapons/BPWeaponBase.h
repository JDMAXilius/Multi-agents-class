#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "BPWeaponBase.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class USkeletalMeshComponent;

/**
 * C++ base for the FPS template's weapon actors. `BP_FPST_BaseWeapon` is reparented onto this,
 * and its four children (Pistol, Rifle, Shotgun, Knife) inherit it.
 *
 * WHAT THIS CLASS IS, AFTER THE 11 Aug 2026 PORT.
 *
 * The eight functions of `BP_FPST_BaseWeapon`, transferred 1:1 from an exec-order read of the
 * live asset (`mcp-bp/weapon_graphs.py` -> `mcp-bp/weapon_graphs.json`; the DSL for each is in
 * the ticket Log). The extraction measured what the class actually is: six of the eight bodies
 * are ONE EXPRESSION. The logic is about thirty lines. Everything else in that Blueprint is
 * per-weapon DATA.
 *
 * THE DATA DELIBERATELY STAYS ON THE ASSETS (founder's call, 11 Aug).
 *
 * `AvailableFireModes`, `CurFireModeIndex`, the montage sets, `Scope`, `DefaultScope`,
 * `SkeletalMesh` and the rest remain Blueprint variables, and these functions read them BY
 * REFLECTION off the instance's own generated class. Two reasons that is right rather than
 * lazy:
 *
 *   1. Declaring a `UPROPERTY` here whose name matches a Blueprint variable is a COMPILE
 *      ERROR on all five weapon assets at once, not an override. Moving ~48 variables down
 *      would have required deleting them from the Blueprint in the same pass, and there is no
 *      remove-graph tool, so the graphs referencing them could not be cleaned by script.
 *   2. Those values are per-weapon tuning that the four children override anyway. That is a
 *      DT_Weapons.csv row, not a class member (`data-and-assets.md`).
 *
 * So: LOGIC in C++, DATA on the asset, and the seam between them is reflection by NAME - a
 * renamed variable fails loudly at the lookup instead of corrupting a stack.
 *
 * STILL TRUE, AND STILL THE REASON TO CHECK THE ASSET FIRST: a Blueprint member whose name
 * matches a parent UFUNCTION or UPROPERTY is a compile error. The eight functions below now
 * carry those names, so `BP_FPST_BaseWeapon`'s own eight graphs MUST be deleted for the asset
 * to compile. That is hand work in the editor - `remove_variable` exists over MCP, a
 * remove-graph tool does not - and it is the one step of this port a script cannot do.
 */
UCLASS()
class BREACHPOINT_API ABPWeaponBase : public AActor
{
	GENERATED_BODY()

public:

	/**
	 * `AvailableFireModes[CurFireModeIndex]`.
	 *
	 * The graph is a bare array Get with NO bounds check - an out-of-range index there returns
	 * a default-constructed enum rather than crashing. This returns FireModeSingle in that
	 * case, which is the same observable behaviour, and logs it, because silently reading a
	 * bad index is how the shotgun/knife swap bugs of 9 Aug hid.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|FireMode")
	uint8 GetCurrentFireMode() const;

	/** `CurFireModeIndex = IncrementInt(CurFireModeIndex) % AvailableFireModes.Length()`. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|FireMode")
	void NextFireMode();

	/**
	 * Switch on E_FPST_SkeletalType, then the aiming branch:
	 *   0 (UE4) -> InAiming ? AimFireAnimMontage_UE4 : FireAnimMontage_UE4
	 *   1 (UE5) -> InAiming ? AimFireAnimMontage     : FireAnimMontage
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Anim")
	UAnimMontage* GetFireAnimMontage(uint8 InSkeletalType, bool InAiming) const;

	/** The identical switch, over the ReloadBolt montage set. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Anim")
	UAnimMontage* GetReloadBoltAnimMontage(uint8 InSkeletalType, bool InAiming) const;

	/** `IsValid(Scope) ? Scope->GetScopeType() : enumerator 0`. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Scope")
	uint8 GetScopeType() const;

	/** `SkeletalMesh->PlayAnimation(NewAnimToPlay, bLooping)`. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Anim")
	void PlayAnim(UAnimSequenceBase* NewAnimToPlay, bool bLooping);

	/**
	 * Destroys `DefaultScope` then `Scope`, each guarded by IsValid.
	 *
	 * It only DESTROYS. An earlier reading of this asset's string table saw
	 * `K2_AttachToComponent` and concluded it re-attached; the graph says otherwise and those
	 * tokens belong elsewhere in the package.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Scope")
	void ResetAttachments();

	/**
	 * `UserConstructionScript` is EMPTY in the asset and is not ported. It is listed here so
	 * the next reader does not go looking for the ninth function.
	 */

	/** The weapon's skeletal mesh, by SCS name first then by class. Never created here. */
	USkeletalMeshComponent* GetWeaponMesh() const;

protected:

	/**
	 * E_FPST_FireMode, read from the asset: three named enumerators in this order. Kept as
	 * named constants rather than magic numbers, and NOT as a C++ enum - the authoritative
	 * enum is the Blueprint's user-defined one and duplicating it would create two sources of
	 * truth for the same ordinals.
	 */
	static constexpr uint8 FireModeSingle = 0;

	/** E_FPST_AttachmentScopeType's first enumerator - what GetScopeType returns with no scope. */
	static constexpr uint8 ScopeTypeNone = 0;

	// -------------------------------------------------------------------------------------
	// Reflection accessors for the Blueprint-owned data. Every one looks the member up BY
	// NAME on this instance's generated class, so a rename fails at the lookup and is logged
	// rather than reading a wrong offset.
	// -------------------------------------------------------------------------------------

	/** An int Blueprint variable. Returns false if absent. */
	bool GetIntVar(FName VarName, int32& OutValue) const;
	bool SetIntVar(FName VarName, int32 Value);

	/** Length of an array Blueprint variable; 0 if absent. */
	int32 GetArrayLength(FName VarName) const;

	/** Element of a byte/enum array Blueprint variable. Returns false if absent or out of range. */
	bool GetByteArrayElement(FName VarName, int32 Index, uint8& OutValue) const;

	/** An object-typed Blueprint variable, cast to T. */
	template <typename T>
	T* GetObjectVar(FName VarName) const
	{
		return Cast<T>(GetObjectVarRaw(VarName));
	}

	UObject* GetObjectVarRaw(FName VarName) const;
};
