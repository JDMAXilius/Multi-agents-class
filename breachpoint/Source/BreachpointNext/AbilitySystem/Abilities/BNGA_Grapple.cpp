#include "AbilitySystem/Abilities/BNGA_Grapple.h"

#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacterMovementComponent.h"
#include "Core/BNGameplayTags.h"
#include "GameFramework/Character.h"

UBNGA_Grapple::UBNGA_Grapple()
{
	// Base defaults stand: InstancedPerActor, LocalPredicted — the press must feel
	// instant on the machine that pressed it, and the flags path reconciles the rest.
}

const FGameplayTagContainer* UBNGA_Grapple::GetCooldownTags() const
{
	// Resolved here, not the constructor — the construction-order rule (native tags are
	// not guaranteed registered while CDOs are built; the grenade's own comment).
	if (CooldownTags.IsEmpty())
	{
		CooldownTags.AddTag(BNTags::Cooldown_Grapple);
	}
	return &CooldownTags;
}

void UBNGA_Grapple::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CooldownDuration <= 0.f)
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UBNGE_GrappleCooldown::StaticClass(), GetAbilityLevel());
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(BNSetByCaller::GrappleCooldown, CooldownDuration);
	Spec.Data->DynamicGrantedTags.AddTag(BNTags::Cooldown_Grapple);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UBNGA_Grapple::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	// TRACE FIRST, COMMIT LAST. A press at the sky must cost nothing — the cooldown is
	// only committed once a pull is genuinely about to start, which is what makes a
	// whiff free and a server rejection roll the cooldown back with the window.
	FHitResult Hit;
	if (!TraceForTarget(Hit))
	{
		UE_LOG(LogBN, Verbose, TEXT("BNGrapple: %s found nothing to hook."), *GetNameSafe(GetAvatarActorFromActorInfo()));
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// The authority's own eyes re-check the claim: the client traced its predicted view,
	// and the server accepts nothing it cannot re-derive (clients send intent, law 1).
	if (HasAuthority(&CurrentActivationInfo))
	{
		FString Reason;
		if (!ValidateTarget(Hit, Reason))
		{
			UE_LOG(LogBN, Log, TEXT("BNGrapple: %s REFUSED — %s."), *GetNameSafe(GetAvatarActorFromActorInfo()), *Reason);
			EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/true);
			return;
		}
	}

	UBNCharacterMovementComponent* Movement = GetBNMovement();
	if (!Movement || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}

	if (!Movement->StartGrapplePull(Hit.ImpactPoint))
	{
		// Committed but the component refused (already at the target, bad ini): the
		// cancel is what rolls the just-committed cooldown's predicted copy back.
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/true);
		return;
	}

	UE_LOG(LogBN, Log, TEXT("BNGrapple: %s hooked %.0fuu away."),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		FVector::Dist(GetAvatarActorFromActorInfo()->GetActorLocation(), Hit.ImpactPoint));

	// Fire-and-forget, the dated delta from the BR original: the component owns the
	// pull's whole lifecycle from here. A clean end must NOT stop the pull it started.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled=*/false);
}

void UBNGA_Grapple::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Only a CANCELLED end stops a running pull — death and the respawn sweep cancel,
	// and a corpse must not keep flying; the fire-and-forget handoff above ends clean.
	if (bWasCancelled)
	{
		if (UBNCharacterMovementComponent* Movement = GetBNMovement())
		{
			Movement->StopGrapplePull();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UBNGA_Grapple::TraceForTarget(FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar || MaxRangeUU <= 0.f)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	const FVector End = ViewLocation + ViewRotation.Vector() * MaxRangeUU;

	// Visibility, the projectile-LOS precedent — the BR original's bespoke grapple
	// channel is deliberately not ported: a second trace channel is config surface the
	// first cut does not need, and "can I see it" is what a hook shot means.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BNGrapple), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Avatar);

	if (!World->LineTraceSingleByChannel(OutHit, ViewLocation, End, ECC_Visibility, Params)
		|| !OutHit.bBlockingHit)
	{
		return false;
	}

	// SELF-PULL ONLY (the first cut's scope): a pawn under the reticle is not a hook
	// point — WeaponAttract and PawnReel are later packets with their own rulings.
	return !OutHit.GetActor() || !OutHit.GetActor()->IsA<APawn>();
}

bool UBNGA_Grapple::ValidateTarget(const FHitResult& Hit, FString& OutReason) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = GetWorld();
	if (!Avatar || !World)
	{
		OutReason = TEXT("server could not resolve the avatar or world");
		return false;
	}

	FVector ServerLocation;
	FRotator ServerRotation;
	Avatar->GetActorEyesViewPoint(ServerLocation, ServerRotation);

	const float Distance = FVector::Dist(ServerLocation, Hit.ImpactPoint);
	if (Distance > MaxRangeUU)
	{
		OutReason = FString::Printf(TEXT("target at %.0fuu exceeds grapple range %.0fuu"), Distance, MaxRangeUU);
		return false;
	}

	FHitResult LosHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BNGrappleLOS), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Avatar);
	if (Hit.GetActor())
	{
		Params.AddIgnoredActor(Hit.GetActor());
	}
	if (World->LineTraceSingleByChannel(LosHit, ServerLocation, Hit.ImpactPoint, ECC_Visibility, Params)
		&& LosHit.bBlockingHit)
	{
		OutReason = FString::Printf(TEXT("no line of sight — '%s' blocks the path"), *GetNameSafe(LosHit.GetActor()));
		return false;
	}

	return true;
}

UBNCharacterMovementComponent* UBNGA_Grapple::GetBNMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	return Character ? Cast<UBNCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}
