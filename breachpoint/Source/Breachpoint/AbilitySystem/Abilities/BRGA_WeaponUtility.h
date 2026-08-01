// BREACHPOINT — BP03 step 2. Reload and swap: the other half of the fire path.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "BRGA_WeaponUtility.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UBREquipmentComponent;
class UBRWeaponInstance;
struct FBRWeaponRow;

/**
 * ================================================================================================
 * TWO ABILITIES, ONE FILE PAIR — and that is ARCHITECTURE §3.3's instruction, not a shortcut.
 * ================================================================================================
 *
 * §3.3 calls `BRGA_WeaponUtility` *"(v2: merged) UBRGA_Reload + UBRGA_WeaponSwap — two tiny
 * sibling abilities, one pair."* Same shape as the generic GE library: several `UCLASS`es in one
 * header because the shared authoring pattern is the point, and split across files the second
 * copy drifts from the first. (It is also why BP15's scanner counts CLASS declarations rather
 * than files — see that ticket's Log.)
 *
 * They are two ABILITIES rather than one with a branch because an ability-set row maps ONE input
 * tag to ONE ability class, and these answer `InputTag.Reload` and `InputTag.Swap`.
 *
 * ================================================================================================
 * THE COMMIT MOMENT — the one idea both abilities are built around
 * ================================================================================================
 *
 * Neither ability does its work when it *starts*. Each waits for a **commit event** and does the
 * work then:
 *
 *   press Reload -> ability activates -> [animation plays] -> `Event.Weapon.ReloadCommit`
 *                -> SERVER moves the rounds -> EndAbility
 *
 * That seam is ruling **R17** and `animation.md` law 4: **a notify announces a MOMENT; the sim
 * decides the CONSEQUENCE, on the authority.** `Event.Weapon.ReloadCommit` says "the hands
 * reached the magazine". It does not say how many rounds — that is a `DT_Weapons` row — and it
 * does not move them, which is the server's job.
 *
 * **The consequence that makes it worth the machinery: a cancel before the commit event costs
 * nothing and refunds nothing.** Interrupt a reload halfway and no ammo has moved, because the
 * only code that moves ammo is behind the event. There is no "partial reload" state to unwind,
 * and therefore no partial-reload bug.
 *
 * ================================================================================================
 * THE ANIMATION GAP, stated rather than hidden
 * ================================================================================================
 *
 * **No montage asset exists yet** (BP18 owns Content; anim is Tier 4). So the commit events can
 * never fire today, and an ability that only waited for them would hang until cancelled — a
 * reload that silently never completes.
 *
 * Both abilities therefore wait on the event AND on a row-authored duration
 * (`ReloadTime_s` / `EquipTime_s`), whichever arrives first, and LOG loudly when the timer is
 * what resolved them. **The timer is scaffolding with an expiry date**: once the montage lands,
 * the notify fires first and the timer never wins. It is not a second source of truth for the
 * timing — it is the same number, from the same row, that the montage will be authored to.
 */

/**
 * RELOAD. Moves rounds from reserve into the magazine at the commit moment, on the server.
 *
 * The arithmetic is NOT here: `UBRWeaponInstance::CalcReloadTransfer` is a static pure function
 * with the invariants on it (never creates ammunition, never overfills the magazine), which is
 * what makes it testable without an ASC, an avatar or a world.
 */
UCLASS()
class BREACHPOINT_API UBRGA_Reload : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Reload(const FObjectInitializer& ObjectInitializer);

	/** A full magazine or an empty reserve is a refusal, not a failed reload. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** The commit moment. Server moves the rounds; every other machine learns by replication. */
	UFUNCTION()
	void OnReloadCommit(FGameplayEventData Payload);

	/** Scaffolding: fires only while no montage exists to raise the commit event. */
	UFUNCTION()
	void OnFallbackElapsed();

	void Commit();

	bool bCommitted = false;
};

/**
 * SWAP. Flips the active slot at the commit moment.
 *
 * The ability does not move the weapon itself — `UBREquipmentComponent` owns the slots and the
 * ability grants that travel with them. This ability owns the *timing* and the *cancel window*;
 * the component owns the *state change*. Two owners, one direction of flow.
 */
UCLASS()
class BREACHPOINT_API UBRGA_WeaponSwap : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_WeaponSwap(const FObjectInitializer& ObjectInitializer);

	/** Nothing to swap to is a refusal — a one-weapon loadout must not play a swap animation. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void OnSwapCommit(FGameplayEventData Payload);

	UFUNCTION()
	void OnFallbackElapsed();

	void Commit();

	bool bCommitted = false;
};
