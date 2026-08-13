#include "AbilitySystem/Abilities/BNGA_Reload.h"

#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Weapons/BNWeapon.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	ABNWeapon* BNReloadGetWeapon(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const ABNCharacter* Character = ActorInfo ? Cast<ABNCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		const UBNEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
		return Equipment ? Equipment->GetCurrentWeapon() : nullptr;
	}
}

bool UBNGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || ASC->HasMatchingGameplayTag(BNTags::State_Weapon_Reloading))
	{
		return false;
	}

	const ABNWeapon* Weapon = BNReloadGetWeapon(ActorInfo);
	return Weapon && Weapon->GetCurrentAmmo() < Weapon->GetMagazineSize();
}

void UBNGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const ABNWeapon* Weapon = BNReloadGetWeapon(ActorInfo);
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !Row)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The state tag, through a GE like every other state in this module — so it reaches simulated
	// proxies and so UBNGA_Fire::CanActivateAbility can read it off the ASC on either machine.
	ReloadingHandle = ApplyStateTag(BNTags::State_Weapon_Reloading);

	// The montage is COSMETIC on every machine and nothing waits on it. Played through the ASC so
	// it replicates to simulated proxies; not through PlayMontageAndWait, because the commit must
	// not hang off a task the client can tear down (below).
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		if (UAnimMontage* Montage = Row->ReloadMontage.IsNull() ? nullptr : Row->ReloadMontage.LoadSynchronous())
		{
			ASC->PlayMontage(this, ActivationInfo, Montage, 1.f);
		}
	}

	if (!ActorInfo->IsNetAuthority())
	{
		// A remote shooter's own instance drives NOTHING. It shows the montage and waits for the
		// authority's EndAbility to arrive — which is the whole point of the next comment.
		return;
	}

	// THE COMMIT IS A TIMER ON THE AUTHORITY, and it has to be, for a reason that is invisible
	// until you play it on a real connection. If the commit hung off the montage's OnCompleted,
	// the CLIENT's montage would finish first (its leg started ~half an RTT earlier), its
	// EndAbility would replicate ServerEndAbility, and that RPC would tear down the AUTHORITY's
	// montage task before the authority's own OnCompleted ever fired — RPC dispatch precedes the
	// anim tick in a frame. Weapon->Reload() would never run, CurrentAmmo would never refill, and
	// the next trigger pull would dry-fire. Deterministic whenever the return leg beats the
	// activation leg, and worse on a 30 Hz server with URO. UBNGA_Fire pays the same lesson by
	// letting the client own its lifetime; reload pays it by letting the AUTHORITY own both the
	// commit and the end. A timer cannot be torn down by a client's RPC, needs no montage notify —
	// the template's reload montages carry none, only AN_FPST_Melee (MyCharacter.cpp:1267-1276) —
	// and keeps working when the ReloadMontage soft ref is unset.
	//
	// ReloadTime is therefore the CONTRACT: the montage should be authored to match it.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CommitTimer, this, &UBNGA_Reload::OnReloadCommitted, FMath::Max(0.1f, Row->ReloadTime), /*bLoop=*/false);
		return;
	}

	OnReloadCommitted();
}

void UBNGA_Reload::OnReloadCommitted()
{
	if (ABNWeapon* Weapon = BNReloadGetWeapon(CurrentActorInfo))
	{
		// Authority-only inside Reload(); every other machine sees CurrentAmmo replicate.
		Weapon->Reload();
	}

	// The authority ends it, and the end replicates down to the shooter's predicted instance.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBNGA_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// A cancelled reload — swap, death, the spec cleared out from under it — must NOT refill, and
	// this is the whole of that guarantee: the timer is gone before it can fire.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CommitTimer);
	}

	// Nothing waits on the montage, so a cancel has to stop it explicitly or the arms keep
	// reloading a magazine that was never loaded.
	if (bWasCancelled)
	{
		MontageStop();
	}

	RemoveStateTag(ReloadingHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
