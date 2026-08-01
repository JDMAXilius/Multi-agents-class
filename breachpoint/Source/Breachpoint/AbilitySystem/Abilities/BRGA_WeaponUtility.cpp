// BREACHPOINT — BP03 step 2. Reload and swap: the other half of the fire path.

#include "AbilitySystem/Abilities/BRGA_WeaponUtility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Data/BRDataRows.h"
#include "GameFramework/Actor.h"
#include "Weapons/BREquipmentComponent.h"
#include "Weapons/BRWeaponInstance.h"

namespace
{
	/** The equipped weapon, or null. Shared by both abilities in this pair. */
	UBREquipmentComponent* FindEquipment(const AActor* Avatar)
	{
		return Avatar ? Avatar->FindComponentByClass<UBREquipmentComponent>() : nullptr;
	}

	UBRWeaponInstance* FindActiveWeapon(const AActor* Avatar)
	{
		const UBREquipmentComponent* Equipment = FindEquipment(Avatar);
		return Equipment ? Equipment->GetActiveWeapon() : nullptr;
	}

	/**
	 * The fallback duration, and the ONLY place either ability decides one.
	 *
	 * Returns the row-authored time. A zero/absent row time yields 0, which the caller reads as
	 * "commit immediately" rather than "wait forever" — a reload that never completes because a
	 * CSV cell was blank is a worse failure than one that completes instantly and says so.
	 */
	float FallbackSeconds(const FBRWeaponRow* Row, bool bIsReload)
	{
		if (!Row)
		{
			return 0.f;
		}
		return bIsReload ? Row->ReloadTime_s : Row->EquipTime_s;
	}
}

// ================================================================================================
// UBRGA_Reload
// ================================================================================================

UBRGA_Reload::UBRGA_Reload(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Weapon_Reload);
	SetAssetTags(AssetTags);

	// FIRING CANCELS A RELOAD. This is the whole reason the commit moment exists: cancel here and
	// no ammo has moved, because the only code that moves ammo is behind the event.
	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	// No cooldown. A reload is bounded by its own duration, not by a cooldown gate — stated
	// rather than omitted, because "no cooldown" and "someone forgot the cooldown" look identical.
	bCommitOnActivate = true;
}

bool UBRGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// THE STAGE GATE — Stage 5 `Weapons` (docs/GAS-INTEGRATION-ROADMAP.md). FIRST statement, and in
	// CanActivateAbility because past here the ASC takes a prediction key and the base commits. A
	// reload that activates and then bails has opened its window and armed its commit timer.
	if (!BRGas::IsStageEnabled(EBRGasStage::Weapons))
	{
		UE_LOG(LogBRCombat, Verbose, TEXT("UBRGA_Reload: activation refused — GAS stage gate is '%s'; reloading needs at least 'Weapons'. Set GasStage in Config/DefaultGame.ini."),
			BRGas::ToString(BRGas::GetStage()));
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UBRWeaponInstance* Weapon = FindActiveWeapon(GetAvatarActorFromActorInfo());
	const FBRWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	if (!Weapon || !Row)
	{
		return false;
	}

	// Refuse a reload that would move zero rounds. CalcReloadTransfer owns the arithmetic AND its
	// invariants (never creates ammunition, never overfills); asking it is how this check stays
	// consistent with the commit that follows, instead of being a second opinion about the same
	// numbers.
	return UBRWeaponInstance::CalcReloadTransfer(Row->MagSize, Weapon->GetAmmoInMag(), Weapon->GetAmmoReserve()) > 0;
}

void UBRGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bCommitted = false;

	// The authoritative path: the montage's notify raises this. Once anim lands it always wins,
	// because it fires before any sane ReloadTime_s elapses.
	if (UAbilityTask_WaitGameplayEvent* WaitCommit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, BRGameplayTags::Event_Weapon_ReloadCommit))
	{
		WaitCommit->EventReceived.AddDynamic(this, &UBRGA_Reload::OnReloadCommit);
		WaitCommit->ReadyForActivation();
	}

	const UBRWeaponInstance* Weapon = FindActiveWeapon(GetAvatarActorFromActorInfo());
	const float Seconds = FallbackSeconds(Weapon ? Weapon->GetRow() : nullptr, /*bIsReload=*/true);

	if (Seconds <= 0.f)
	{
		// No authored duration and no montage: commit now rather than hang. Loud, because a
		// zero-length reload is a data gap wearing the costume of a working feature.
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Reload: ReloadTime_s is 0 and no montage raised %s — committing immediately."),
			*BRGameplayTags::Event_Weapon_ReloadCommit.GetTag().GetTagName().ToString());
		Commit();
		return;
	}

	if (UAbilityTask_WaitDelay* Fallback = UAbilityTask_WaitDelay::WaitDelay(this, Seconds))
	{
		Fallback->OnFinish.AddDynamic(this, &UBRGA_Reload::OnFallbackElapsed);
		Fallback->ReadyForActivation();
	}
}

void UBRGA_Reload::OnReloadCommit(FGameplayEventData Payload)
{
	Commit();
}

void UBRGA_Reload::OnFallbackElapsed()
{
	if (!bCommitted)
	{
		// Scaffolding fired, which means no montage raised the event. Say so — this line is how
		// the next session learns the anim gap is still open without reading a ticket.
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Reload committed on the ReloadTime_s timer, not on %s. No reload montage "
				 "exists yet (BP18 owns Content); the notify is authoritative once it does."),
			*BRGameplayTags::Event_Weapon_ReloadCommit.GetTag().GetTagName().ToString());
	}
	Commit();
}

void UBRGA_Reload::Commit()
{
	if (bCommitted)
	{
		// The event and the timer can both arrive. Rounds move once.
		return;
	}
	bCommitted = true;

	// SERVER ONLY. Ammo is the named GAS exception (`gas-purity.md` §8): weapon state, server
	// -mutated, replicated down. A client that moved its own rounds would be corrected on the
	// next RepNotify, which looks exactly like a bug and is one.
	if (HasAuthority(&CurrentActivationInfo))
	{
		if (UBRWeaponInstance* Weapon = FindActiveWeapon(GetAvatarActorFromActorInfo()))
		{
			const int32 Moved = Weapon->CommitReload();
			UE_LOG(LogBRCombat, Verbose, TEXT("UBRGA_Reload moved %d round(s)."), Moved);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ================================================================================================
// UBRGA_WeaponSwap
// ================================================================================================

UBRGA_WeaponSwap::UBRGA_WeaponSwap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Weapon_Swap);
	SetAssetTags(AssetTags);

	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	bCommitOnActivate = true;
}

bool UBRGA_WeaponSwap::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// THE STAGE GATE — Stage 5 `Weapons` (docs/GAS-INTEGRATION-ROADMAP.md). FIRST statement, same
	// reasoning as the reload above: refuse before the prediction key, not after.
	if (!BRGas::IsStageEnabled(EBRGasStage::Weapons))
	{
		UE_LOG(LogBRCombat, Verbose, TEXT("UBRGA_WeaponSwap: activation refused — GAS stage gate is '%s'; swapping needs at least 'Weapons'. Set GasStage in Config/DefaultGame.ini."),
			BRGas::ToString(BRGas::GetStage()));
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UBREquipmentComponent* Equipment = FindEquipment(GetAvatarActorFromActorInfo());
	if (!Equipment)
	{
		return false;
	}

	// Something to swap TO. A one-weapon loadout must not play a swap animation and then flip to
	// the slot it is already in.
	const int32 ActiveSlot = Equipment->GetActiveSlotIndex();
	const EBRWeaponSlot OtherSlot = (ActiveSlot == 0) ? EBRWeaponSlot::Secondary : EBRWeaponSlot::Primary;
	return Equipment->IsSlotOccupied(OtherSlot);
}

void UBRGA_WeaponSwap::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bCommitted = false;

	if (UAbilityTask_WaitGameplayEvent* WaitCommit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, BRGameplayTags::Event_Weapon_SwapCommit))
	{
		WaitCommit->EventReceived.AddDynamic(this, &UBRGA_WeaponSwap::OnSwapCommit);
		WaitCommit->ReadyForActivation();
	}

	// EquipTime_s is R3's "0.4 s swap" — the number the AR-strip -> swap -> Magnum-finish line is
	// balanced around. It is read from the row of the weapon being PUT AWAY, which is the one
	// currently active.
	const UBRWeaponInstance* Weapon = FindActiveWeapon(GetAvatarActorFromActorInfo());
	const float Seconds = FallbackSeconds(Weapon ? Weapon->GetRow() : nullptr, /*bIsReload=*/false);

	if (Seconds <= 0.f)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_WeaponSwap: EquipTime_s is 0 and no montage raised %s — committing "
				 "immediately. R3 balances the sandbox around a 0.4 s swap, so a 0 here is a data "
				 "gap, not a fast weapon."),
			*BRGameplayTags::Event_Weapon_SwapCommit.GetTag().GetTagName().ToString());
		Commit();
		return;
	}

	if (UAbilityTask_WaitDelay* Fallback = UAbilityTask_WaitDelay::WaitDelay(this, Seconds))
	{
		Fallback->OnFinish.AddDynamic(this, &UBRGA_WeaponSwap::OnFallbackElapsed);
		Fallback->ReadyForActivation();
	}
}

void UBRGA_WeaponSwap::OnSwapCommit(FGameplayEventData Payload)
{
	Commit();
}

void UBRGA_WeaponSwap::OnFallbackElapsed()
{
	if (!bCommitted)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_WeaponSwap committed on the EquipTime_s timer, not on %s. No swap montage "
				 "exists yet (BP18 owns Content)."),
			*BRGameplayTags::Event_Weapon_SwapCommit.GetTag().GetTagName().ToString());
	}
	Commit();
}

void UBRGA_WeaponSwap::Commit()
{
	if (bCommitted)
	{
		return;
	}
	bCommitted = true;

	// The ability owns the TIMING and the cancel window; the component owns the STATE CHANGE.
	// Going through the component's existing client-intent RPC rather than writing the slot here
	// keeps one owner for the slots — and that RPC already splits wire validation (_Validate:
	// is this a real slot index) from state validation (is it occupied, is the pawn alive).
	//
	// LOCALLY CONTROLLED ONLY, and this guard is load-bearing rather than defensive. A
	// LocalPredicted ability runs its body on BOTH the owning client and the server, so an
	// unguarded call swaps twice on a dedicated server: once from the server's own Commit, once
	// from the client's RPC — which lands the player back on the weapon they started with, and
	// only for remote clients, never for the listen-server host who tests it.
	if (IsLocallyControlled())
	{
		if (UBREquipmentComponent* Equipment = FindEquipment(GetAvatarActorFromActorInfo()))
		{
			const int32 ActiveSlot = Equipment->GetActiveSlotIndex();
			const uint8 TargetSlot = (ActiveSlot == 0) ? 1 : 0;
			Equipment->ServerRequestSwapSlot(TargetSlot);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
