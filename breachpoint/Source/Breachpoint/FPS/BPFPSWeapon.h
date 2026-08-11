#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "BPFPSWeapon.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class USkeletalMeshComponent;

/**
 * BP_FPST_BaseWeapon's eight functions, in C++ — as a STANDALONE class that nothing
 * derives from yet.
 *
 * WHY THIS FILE EXISTS INSTEAD OF THESE FUNCTIONS LIVING ON ABPFPSWeapon.
 *
 * They were put on `ABPFPSWeapon` twice, and both times every weapon asset stopped
 * compiling: `BP_FPST_BaseWeapon` owns graphs with these exact names, and a Blueprint
 * function whose name matches a parent UFUNCTION is a COMPILE ERROR, not an override. The
 * symptom is `weapons=0/4` on the READY line and no weapon in hand. Landing that half
 * without deleting the Blueprint's graphs in the same sitting broke the game twice.
 *
 * `ABPFPSWeapon` is therefore back to declaring NOTHING, the weapons keep working, and the
 * port lives here where it can be compiled, reviewed and diffed without touching the
 * inheritance chain. Nothing derives from `ABPFPSWeapon`, so no Blueprint can collide with
 * it.
 *
 * HOW TO ACTIVATE IT, when someone has a clear session for the two-part change:
 *   1. In the editor, delete these seven graphs from BP_FPST_BaseWeapon: GetCurrentFireMode,
 *      NextFireMode, GetFireAnimMontage, GetReloadBoltAnimMontage, GetScopeType, PlayAnim,
 *      ResetAttachments. Leave UserConstructionScript — engine-owned and empty. There is no
 *      remove-graph tool over MCP; this is hand work.
 *   2. Move these members onto ABPFPSWeapon (or reparent BP_FPST_BaseWeapon onto this
 *      class), rebuild, re-run mcp-bp/reparent_weapon_base.py, and confirm weapons=4/4.
 * Between those steps the weapons are broken, so they belong in one sitting.
 *
 * The bodies are a 1:1 transfer of an exec-order read of the live asset
 * (mcp-bp/weapon_graphs.py -> mcp-bp/weapon_graphs.json; the DSL for each is in the ticket
 * Log), not a reconstruction from the function names. Six of the eight are one expression.
 *
 * The DATA stays on the assets. AvailableFireModes, CurFireModeIndex, the montage sets,
 * Scope, DefaultScope and SkeletalMesh remain Blueprint variables, read here BY REFLECTION
 * off the instance's own generated class — per-weapon tuning is a DT_Weapons row, not a
 * class member, and declaring UPROPERTYs for them is the very collision described above.
 */
UCLASS()
class BREACHPOINT_API ABPFPSWeapon : public AActor
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
