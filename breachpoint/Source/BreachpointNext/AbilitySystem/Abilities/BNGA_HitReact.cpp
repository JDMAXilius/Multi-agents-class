#include "AbilitySystem/Abilities/BNGA_HitReact.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "BreachpointNext.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

namespace
{
	/** The incoming vector, unrotated into the victim's frame; the dominant axis names the montage.
	 *  Incoming travelling BACKWARD relative to the victim (-X) means the shot came from the front
	 *  -> Front reaction; +X -> Back. Travelling toward +Y (the victim's right) means the attacker
	 *  stood to the LEFT -> Left; -Y -> Right. */
	EBNHitDirection BNDirectionForHit(const AActor* Victim, const FHitResult& Hit)
	{
		// Traced hits (fire, melee) carry a real TraceStart; the grenade's hand-built hit does not,
		// and its ImpactNormal already IS the blast's travel direction, so it is used as-is.
		const FVector Incoming = !Hit.TraceStart.IsNearlyZero()
			? (Hit.ImpactPoint - Hit.TraceStart).GetSafeNormal()
			: FVector(Hit.ImpactNormal);
		const FVector Local = Victim->GetActorRotation().UnrotateVector(Incoming);

		if (FMath::Abs(Local.X) >= FMath::Abs(Local.Y))
		{
			return Local.X >= 0.f ? EBNHitDirection::Back : EBNHitDirection::Front;
		}
		return Local.Y >= 0.f ? EBNHitDirection::Left : EBNHitDirection::Right;
	}

	const TCHAR* BNDirectionName(EBNHitDirection Direction)
	{
		switch (Direction)
		{
		case EBNHitDirection::Back:  return TEXT("Back");
		case EBNHitDirection::Left:  return TEXT("Left");
		case EBNHitDirection::Right: return TEXT("Right");
		default:                     return TEXT("Front");
		}
	}
}

UBNGA_HitReact::UBNGA_HitReact()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// A second hit mid-flinch restarts the reaction rather than being swallowed — sustained fire
	// keeps the victim visibly rocking instead of playing one montage and going stiff.
	bRetriggerInstancedAbility = true;
}

void UBNGA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = GetWorld();
	if (!ASC || !Avatar || !World || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Retrigger path: a previous flinch may still own the timer.
	World->GetTimerManager().ClearTimer(ReactTimer);

	// The capture the damage packet built, used as designed: the attribute set recorded the hit at
	// the one reaction point; this is the read. Amount picks severity, the hit picks direction,
	// and the bone is logged — it is phase 2's input (per-limb physics) and a live check that
	// traces return bones at all.
	const UBNAttributeSet* Attributes = ASC->GetSet<UBNAttributeSet>();
	UBNHitReactionSet* Set = ReactionSet.IsNull() ? nullptr : ReactionSet.LoadSynchronous();
	if (!Attributes || !Set)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FBNLastDamage& LastDamage = Attributes->GetLastDamage();
	const EBNHitDirection Direction = BNDirectionForHit(Avatar, LastDamage.Hit);
	const EBNHitSeverity Severity = Set->SeverityForDamage(LastDamage.Amount);
	const FBNHitReactionRow* Row = Set->FindRow(Direction, Severity);
	UAnimMontage* Montage = nullptr;
	if (Row)
	{
		// The SERVER picks the variant and the pick replicates inside the montage channel, so every
		// observer sees the same one — per-machine randomness would show each client a different flinch.
		const TSoftObjectPtr<UAnimMontage>& Soft = Row->Montages[FMath::RandRange(0, Row->Montages.Num() - 1)];
		Montage = Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
	}

	UE_LOG(LogBN, Verbose, TEXT("BNGA_HitReact: %s hit from %s for %.0f (bone '%s') -> %s"),
		*GetNameSafe(Avatar), BNDirectionName(Direction), LastDamage.Amount,
		*LastDamage.Hit.BoneName.ToString(), *GetNameSafe(Montage));

	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const float Length = ASC->PlayMontage(this, ActivationInfo, Montage, 1.f);
	if (Length <= 0.f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// The flinch ends with its animation. A timer rather than the montage-ended delegate for the
	// same reason melee's swing uses one: the ability's lifetime must not depend on a callback the
	// montage might never deliver.
	World->GetTimerManager().SetTimer(ReactTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}), Length, /*bLoop=*/false);
}

void UBNGA_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReactTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
