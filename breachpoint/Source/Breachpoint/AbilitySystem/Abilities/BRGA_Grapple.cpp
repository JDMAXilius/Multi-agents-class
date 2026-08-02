// BREACHPOINT — BP06. The Grappleshot: the netcode packet.
#include "AbilitySystem/Abilities/BRGA_Grapple.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "Character/BRCharacterMovementComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Weapons/BRWeaponPickup.h"

namespace
{
	const FName GrappleRangeMetresCurve(TEXT("Grapple.RangeMetres"));
	const FName GrappleCooldownSecondsCurve(TEXT("Grapple.CooldownSeconds"));
}

UBRGA_Grapple::UBRGA_Grapple(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Grapple);
	SetAssetTags(AssetTags);

	CooldownTag = BRGameplayTags::Ability_Grapple;

	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	bCommitOnActivate = true;
}

float UBRGA_Grapple::GetCooldownDurationSeconds() const
{
	float Seconds = 0.f;
	if (BRCombatCurves::Evaluate(GrappleCooldownSecondsCurve, Seconds) && Seconds > 0.f)
	{
		return Seconds;
	}

	UE_LOG(LogBRCombat, Warning,
		TEXT("UBRGA_Grapple: CT_Combat has no '%s' row. The cooldown cannot be authored, and the "
			 "base will refuse rather than apply a zero-length one. Row owed by BP13/curator."),
		*GrappleCooldownSecondsCurve.ToString());
	return 0.f;
}

float UBRGA_Grapple::GetMaxRangeMetres() const
{
	float Metres = 0.f;
	if (BRCombatCurves::Evaluate(GrappleRangeMetresCurve, Metres) && Metres > 0.f)
	{
		return Metres;
	}
	return 0.f;
}

bool UBRGA_Grapple::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!BRGas::IsStageEnabled(EBRGasStage::FullSandbox))
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UBRGA_Grapple::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bResolved = false;
	ActiveMode = EBRGrappleMode::None;

	const float RangeMetres = GetMaxRangeMetres();
	if (RangeMetres <= 0.f)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grapple refused: CT_Combat has no '%s' row, so there is no range to trace. "
				 "A hardcoded 20 m here would be a law-3 violation."),
			*GrappleRangeMetresCurve.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FHitResult Hit;
	if (!TraceForTarget(Hit))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const EBRGrappleMode Mode = ClassifyHit(Hit);
	if (Mode == EBRGrappleMode::None)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		FString Reason;
		if (!ValidateTarget(Hit, Mode, Reason))
		{
			UE_LOG(LogBRCombat, Warning, TEXT("UBRGA_Grapple REJECTED: %s"), *Reason);
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	BeginPull(Hit, Mode);
}

bool UBRGA_Grapple::TraceForTarget(FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!World || !Avatar)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	const FVector End = ViewLocation + ViewRotation.Vector() * (GetMaxRangeMetres() * BRUnits::MetresToUU);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRGrapple), false);
	Params.AddIgnoredActor(Avatar);

	return World->LineTraceSingleByChannel(OutHit, ViewLocation, End, BRCollision::GrappleTrace, Params)
		&& OutHit.bBlockingHit;
}

EBRGrappleMode UBRGA_Grapple::ClassifyHit(const FHitResult& Hit) const
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return EBRGrappleMode::SelfPull;
	}

	if (HitActor->IsA<ABRWeaponPickup>())
	{
		return EBRGrappleMode::WeaponAttract;
	}

	if (HitActor->IsA<APawn>())
	{
		return EBRGrappleMode::PawnReel;
	}

	return EBRGrappleMode::SelfPull;
}

bool UBRGA_Grapple::ValidateTarget(const FHitResult& Claim, EBRGrappleMode ClaimedMode, FString& OutReason) const
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

	const float MaxUU = GetMaxRangeMetres() * BRUnits::MetresToUU;
	const float Distance = FVector::Dist(ServerLocation, Claim.ImpactPoint);
	if (Distance > MaxUU)
	{
		OutReason = FString::Printf(TEXT("target at %.0f uu exceeds grapple range %.0f uu"), Distance, MaxUU);
		return false;
	}

	FHitResult LosHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRGrappleLOS), false);
	Params.AddIgnoredActor(Avatar);
	if (Claim.GetActor())
	{
		Params.AddIgnoredActor(Claim.GetActor());
	}
	if (World->LineTraceSingleByChannel(LosHit, ServerLocation, Claim.ImpactPoint, BRCollision::GrappleTrace, Params)
		&& LosHit.bBlockingHit)
	{
		OutReason = FString::Printf(TEXT("no line of sight — '%s' blocks the path to the target"),
			*GetNameSafe(LosHit.GetActor()));
		return false;
	}

	if (ClassifyHit(Claim) != ClaimedMode)
	{
		OutReason = TEXT("server classified the target differently from the client");
		return false;
	}

	return true;
}

void UBRGA_Grapple::BeginPull(const FHitResult& Hit, EBRGrappleMode Mode)
{
	if (bResolved)
	{
		return;
	}
	bResolved = true;
	ActiveMode = Mode;

	switch (Mode)
	{
	case EBRGrappleMode::SelfPull:
	case EBRGrappleMode::PawnReel:
	{
		if (UBRCharacterMovementComponent* Movement = GetBRMovement())
		{
			Movement->StartGrapplePull(Hit.ImpactPoint);
		}
		else
		{
			UE_LOG(LogBRCombat, Error,
				TEXT("UBRGA_Grapple: no UBRCharacterMovementComponent — the pull cannot be "
					 "predicted and will not be faked with a position write."));
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		break;
	}

	case EBRGrappleMode::WeaponAttract:
	{
		if (HasAuthority(&CurrentActivationInfo))
		{
			if (ABRWeaponPickup* Pickup = Cast<ABRWeaponPickup>(Hit.GetActor()))
			{
				UE_LOG(LogBRCombat, Warning,
					TEXT("UBRGA_Grapple: WeaponAttract has no seam on '%s' yet. "
						 "ABRWeaponPickup owes an authority-side attract entry point; this "
						 "ability must not move it from here."),
					*GetNameSafe(Pickup));
			}
		}
		break;
	}

	default:
		break;
	}
}

void UBRGA_Grapple::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActiveMode == EBRGrappleMode::SelfPull || ActiveMode == EBRGrappleMode::PawnReel)
	{
		if (UBRCharacterMovementComponent* Movement = GetBRMovement())
		{
			Movement->StopGrapplePull();
		}
	}
	ActiveMode = EBRGrappleMode::None;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UBRCharacterMovementComponent* UBRGA_Grapple::GetBRMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	return Character ? Cast<UBRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}
