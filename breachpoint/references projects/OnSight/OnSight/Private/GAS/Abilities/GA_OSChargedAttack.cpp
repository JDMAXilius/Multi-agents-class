#include "GAS/Abilities/GA_OSChargedAttack.h"
#include "Engine/Engine.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Data/OSGameplayTags.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Characters/OSCharacter.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

UGA_OSChargedAttack::UGA_OSChargedAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Avoid FOSGameplayTags::Get() here: it constructs the static singleton and can run before the
	// GameplayTag manager is ready (or hit missing tags), causing breakpoint/crash. Request tags
	// by name with bErrorIfNotFound=false so CDO creation never asserts.
	constexpr bool bErrorIfNotFound = false;
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Attack"), bErrorIfNotFound));
	AssetTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.Attack.Heavy"), bErrorIfNotFound));
	AssetTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.Ability.ChargedAttack"), bErrorIfNotFound));
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsDead"), bErrorIfNotFound));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsAttacking"), bErrorIfNotFound));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Guard.IsActive"), bErrorIfNotFound));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Sprinting"), bErrorIfNotFound));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.InAir"), bErrorIfNotFound));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.IsGrabbed"), bErrorIfNotFound));
}

void UGA_OSChargedAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Safety net: block attacks while sprinting or airborne (C++ enforced, bypasses Blueprint CDO serialization).
	// Primary blocking is via ActivationBlockedTags in the Blueprint CDO. This gate catches misconfigured BPs.
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FOSGameplayTags& GateTags = FOSGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(GateTags.IsBlocking)
			|| ASC->HasMatchingGameplayTag(GateTags.IsSprinting)
			|| ASC->HasMatchingGameplayTag(GateTags.IsInAir))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	// CommitAbility is handled by Super (UOSGameplayAbility::ActivateAbility).
	// Do NOT call it here — double commit causes double cost deduction.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Base class commits costs and calls EndAbility on failure — bail if that happened.
	if (!IsActive()) return;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		UE_LOG(LogTemp, Error, TEXT("UGA_OSChargedAttack: AvatarActor is invalid"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		UE_LOG(LogTemp, Error, TEXT("UGA_OSChargedAttack: AbilitySystemComponent is invalid"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bIsCharging = true;
	bHasReleasedInput = false;
	ChargeStartTime = GetWorld()->GetTimeSeconds();
	StoredChargeDamage = 0.0f;

	const FOSGameplayTags& Tags = FOSGameplayTags::Get();
	if (Tags.IsCharging.IsValid())
	{
		ASC->AddLooseGameplayTag(Tags.IsCharging);
	}

	// Play charge loop montage if specified
	if (IsValid(ChargedAttackMontage) && !ChargeLoopSection.IsNone())
	{
		ACharacter* Character = Cast<ACharacter>(AvatarActor);
		if (IsValid(Character))
		{
			UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
			if (IsValid(AnimInstance))
			{
				UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
					this,
					FName("ChargedAttackLoop"),
					ChargedAttackMontage,
					1.0f,
					ChargeLoopSection,
					false,
					1.0f
				);

				if (IsValid(PlayMontageTask))
				{
					PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_OSChargedAttack::OnChargeLoopCompleted);
					PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_OSChargedAttack::OnChargedAttackCancelled);
					PlayMontageTask->ReadyForActivation();
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("UGA_OSChargedAttack: Failed to create charge loop montage task"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UGA_OSChargedAttack: AnimInstance is invalid"));
			}
		}
	}

	// Start stamina drain timer
	if (StaminaCostPerSecond > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ChargeCostTimerHandle,
			[this]()
			{
				if (!bIsCharging) return;

				UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
				if (!IsValid(ASC)) return;

				const UOSAttributeSet* AS = OSAttrs();
				if (!IsValid(AS)) return;

				float CurrentStamina = AS->GetStamina();
				if (CurrentStamina < StaminaCostPerSecond)
				{
					// Not enough stamina, release attack
					if (bHasReleasedInput)
					{
						// Already released, just end
						return;
					}
					
					// Force release
					InputReleased(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
					return;
				}

				// Apply stamina cost on server only so it replicates correctly
				AActor* OwnerActor = GetOwningActorFromActorInfo();
				if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
				{
					if (!IsValid(StaminaCostEffectClass))
					{
						UE_LOG(LogTemp, Error, TEXT("GA_OSChargedAttack: StaminaCostEffectClass is invalid — assign a valid GE in Blueprint."));
						return;
					}

					FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
					EffectContext.AddSourceObject(this);

					FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StaminaCostEffectClass, 1.0f, EffectContext);
					if (SpecHandle.IsValid())
					{
						const FOSGameplayTags& Tags = FOSGameplayTags::Get();
						// Negative magnitude = drain. Matches base class convention (UOSGameplayAbility::ApplyCosts).
						if (Tags.Data_Cost_Stamina.IsValid())
							SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Stamina, -StaminaCostPerSecond);
						if (Tags.Data_Cost_Health.IsValid())
							SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Health, 0.0f);
						if (Tags.Data_Cost_Aura.IsValid())
							SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Cost_Aura, 0.0f);
						ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
					}
				}
			},
			1.0f, // Cost per second
			true  // Loop
		);
	}
}

void UGA_OSChargedAttack::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Input is already pressed (we're charging), do nothing
}

void UGA_OSChargedAttack::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!bIsCharging)
	{
		// Not charging, cancel ability
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	float ChargeTime = CurrentTime - ChargeStartTime;

	// Check minimum charge time
	if (ChargeTime < MinChargeTime)
	{
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	bHasReleasedInput = true;
	bIsCharging = false;

	// Calculate charge amount (0.0 to 1.0)
	float ChargeAmount = FMath::Clamp(ChargeTime / MaxChargeTime, 0.0f, 1.0f);

	// Calculate damage based on charge
	float FinalDamage = FMath::Lerp(BaseDamage, MaxDamage, ChargeAmount);

	// Stop stamina drain
	GetWorld()->GetTimerManager().ClearTimer(ChargeCostTimerHandle);

	// Remove charging tag and add attacking tag
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(ASC))
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (Tags.IsCharging.IsValid())
		{
			ASC->RemoveLooseGameplayTag(Tags.IsCharging);
		}
		ASC->AddLooseGameplayTag(Tags.IsAttacking);
	}

	// Perform charged attack
	PerformChargedAttack(ChargeAmount);
}

void UGA_OSChargedAttack::PerformChargedAttack(float ChargeAmount)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	if (!IsValid(Character))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	// Calculate damage
	float FinalDamage = FMath::Lerp(BaseDamage, MaxDamage, ChargeAmount);

	// Align to input direction via BPFL — MW if montage has warp window, else CMC fallback.
	if (AOSCharacter* Char = Avatar())
	{
		const FVector Dir2D = UOSCombatBlueprintLibrary::GetInputDirection2D(Char);
		if (!Dir2D.IsNearlyZero())
		{
			UOSCombatBlueprintLibrary::AlignCharacterToDirection(Char, Dir2D, ChargedAttackMontage);
		}
	}
	OnChargedAttackPerformed(ChargeAmount);

	// Trace will be performed via anim notify (Do Attack Trace) - PerformAttackTrace() is BlueprintCallable
	// Store damage for trace
	StoredChargeDamage = FinalDamage;

	// Play attack montage if specified
	if (IsValid(ChargedAttackMontage) && !ChargeAttackSection.IsNone())
	{
		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		if (IsValid(AnimInstance))
		{
			UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				FName("ChargedAttackRelease"),
				ChargedAttackMontage,
				1.0f,
				ChargeAttackSection,
				false,
				1.0f
			);

			if (IsValid(PlayMontageTask))
			{
				PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_OSChargedAttack::OnChargedAttackCompleted);
				PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_OSChargedAttack::OnChargedAttackCompleted);
				PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_OSChargedAttack::OnChargedAttackCancelled);
				PlayMontageTask->ReadyForActivation();
			}
			else
			{
				// No montage, end immediately
				EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
			}
		}
		else
		{
			// No anim instance, end immediately
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
		}
	}
	else
	{
		// No montage specified, end immediately
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
	}
}

void UGA_OSChargedAttack::OnChargeLoopCompleted()
{
	// Charge loop completed, but we might still be charging
	// Wait for input release
}

void UGA_OSChargedAttack::OnChargedAttackCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
}

void UGA_OSChargedAttack::OnChargedAttackCancelled()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(ASC))
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (Tags.IsCharging.IsValid())
		{
			ASC->RemoveLooseGameplayTag(Tags.IsCharging);
		}
	}
	
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UGA_OSChargedAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Stop stamina drain timer
	GetWorld()->GetTimerManager().ClearTimer(ChargeCostTimerHandle);

	// Ensure state tags are removed
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const FOSGameplayTags& Tags = FOSGameplayTags::Get();
		if (Tags.IsCharging.IsValid())
		{
			ASC->RemoveLooseGameplayTag(Tags.IsCharging);
		}
		ASC->RemoveLooseGameplayTag(Tags.IsAttacking);
	}

	bIsCharging = false;
	bHasReleasedInput = false;

	// Clear any active alignment state (MW warp target + CMC target rotation)
	if (AOSCharacter* Char = Avatar())
	{
		UOSCombatBlueprintLibrary::ClearAlignmentState(Char, WarpTargetName);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_OSChargedAttack::PerformAttackTrace()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)) return;

	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	if (!IsValid(Character)) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC)) return;

	// Use stored charge damage or calculate from current charge
	float FinalDamage = StoredChargeDamage;
	if (FinalDamage <= 0.0f)
	{
		// Fallback: calculate from charge time if not set
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float ChargeTime = CurrentTime - ChargeStartTime;
		float ChargeAmount = FMath::Clamp(ChargeTime / MaxChargeTime, 0.0f, 1.0f);
		FinalDamage = FMath::Lerp(BaseDamage, MaxDamage, ChargeAmount);
	}

	// Perform trace for damage (can be called from anim notify)
	FVector TraceStart = Character->GetActorLocation();
	FVector TraceEnd = TraceStart + (Character->GetActorForwardVector() * 100.0f); // Longer range for charged attack
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	
	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Pawn, QueryParams))
	{
		if (AActor* HitActor = HitResult.GetActor())
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
			{
				// NOTE: Authoritative damage is applied exclusively by `OSAnimNotifyState_GASAttackTrace` on the server.
				// Keep this trace for optional target acquisition / event dispatch only.
				
				if (FOSGameplayTags::Get().Event_AttackHit.IsValid())
				{
					FGameplayEventData EventData;
					EventData.Instigator = AvatarActor;
					EventData.Target = HitActor;
					// MP GAS best-practice: we already have the target ASC, so route via ASC directly.
					TargetASC->HandleGameplayEvent(FOSGameplayTags::Get().Event_AttackHit, &EventData);
				}
			}
		}
	}
}
