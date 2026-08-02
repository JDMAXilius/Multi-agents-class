// BREACHPOINT — BP05 step 2. The melee path.
#include "AbilitySystem/Abilities/BRGA_Melee.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "CollisionShape.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	const FName MeleeBaseDamageCurve(TEXT("Melee.BaseDamage"));
	const FName MeleeRangeCurve(TEXT("Melee.RangeMetres"));
	const FName MeleeSweepRadiusCurve(TEXT("Melee.SweepRadiusMetres"));
	const FName MeleeRearArcCurve(TEXT("Melee.RearArcDegrees"));

	constexpr float SwingWindowFallbackSeconds = 0.25f;

	constexpr float ClaimTimeoutSeconds = 1.5f;

	constexpr float ReachToleranceUU = 75.f;

	bool ResolveMeleeTuning(float& OutBaseDamage, float& OutRangeUU, float& OutSweepRadiusUU, float& OutRearArcDegrees, FString& OutMissing)
	{
		float RangeMetres = 0.f;
		float SweepRadiusMetres = 0.f;

		if (!BRCombatCurves::Evaluate(MeleeBaseDamageCurve, OutBaseDamage))
		{
			OutMissing = MeleeBaseDamageCurve.ToString();
			return false;
		}
		if (!BRCombatCurves::Evaluate(MeleeRangeCurve, RangeMetres))
		{
			OutMissing = MeleeRangeCurve.ToString();
			return false;
		}
		if (!BRCombatCurves::Evaluate(MeleeSweepRadiusCurve, SweepRadiusMetres))
		{
			OutMissing = MeleeSweepRadiusCurve.ToString();
			return false;
		}
		if (!BRCombatCurves::Evaluate(MeleeRearArcCurve, OutRearArcDegrees))
		{
			OutMissing = MeleeRearArcCurve.ToString();
			return false;
		}

		OutRangeUU = RangeMetres * BRUnits::MetresToUU;
		OutSweepRadiusUU = SweepRadiusMetres * BRUnits::MetresToUU;

		if (OutRangeUU <= 0.f)
		{
			OutMissing = FString::Printf(TEXT("%s is %.3f (must be > 0)"), *MeleeRangeCurve.ToString(), RangeMetres);
			return false;
		}
		if (OutSweepRadiusUU <= 0.f)
		{
			OutMissing = FString::Printf(TEXT("%s is %.3f (must be > 0)"), *MeleeSweepRadiusCurve.ToString(), SweepRadiusMetres);
			return false;
		}

		return true;
	}
}

UBRGA_Melee::UBRGA_Melee(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Melee);
	SetAssetTags(AssetTags);

	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	bCommitOnActivate = true;
}

bool UBRGA_Melee::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!BRGas::IsStageEnabled(EBRGasStage::FullSandbox))
	{
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	float BaseDamage = 0.f, RangeUU = 0.f, SweepRadiusUU = 0.f, RearArcDegrees = 0.f;
	FString Missing;
	if (!ResolveMeleeTuning(BaseDamage, RangeUU, SweepRadiusUU, RearArcDegrees, Missing))
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGA_Melee refused to activate: CT_Combat is missing '%s'. Melee's four coefficients "
				 "(Melee.BaseDamage, Melee.RangeMetres, Melee.SweepRadiusMetres, Melee.RearArcDegrees) "
				 "are DATA — typing one into this ability would be a law-3 violation. Filed as a "
				 "contract_gap in TICKET_BP05_TRIANGLE."),
			*Missing);
		return false;
	}

	return true;
}

void UBRGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	SwingOriginLocation = FVector::ZeroVector;
	bWindowOpen = false;
	bSwingResolved = false;
	bClaimHandled = false;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (IsLocallyControlled())
	{
		WindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, BRGameplayTags::Event_Melee_WindowBegin, nullptr, true);
		WindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, BRGameplayTags::Event_Melee_WindowEnd, nullptr, true);

		if (!WindowBeginTask || !WindowEndTask)
		{
			UE_LOG(LogBRCombat, Error, TEXT("BRGA_Melee: could not create the notify-window tasks; ending without swinging."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		WindowBeginTask->EventReceived.AddDynamic(this, &UBRGA_Melee::OnWindowBegin);
		WindowEndTask->EventReceived.AddDynamic(this, &UBRGA_Melee::OnWindowEnd);
		WindowBeginTask->ReadyForActivation();
		WindowEndTask->ReadyForActivation();

		WatchdogTask = UAbilityTask_WaitDelay::WaitDelay(this, SwingWindowFallbackSeconds);
		if (WatchdogTask)
		{
			WatchdogTask->OnFinish.AddDynamic(this, &UBRGA_Melee::OnSwingWatchdogElapsed);
			WatchdogTask->ReadyForActivation();
		}
		else
		{
			UE_LOG(LogBRCombat, Error, TEXT("BRGA_Melee: could not create the swing watchdog; ending without swinging."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		}
		return;
	}

	WatchdogTask = UAbilityTask_WaitDelay::WaitDelay(this, ClaimTimeoutSeconds);
	if (WatchdogTask)
	{
		WatchdogTask->OnFinish.AddDynamic(this, &UBRGA_Melee::OnClaimTimeoutElapsed);
		WatchdogTask->ReadyForActivation();
	}

	if (!IsActive())
	{
		return;
	}

	TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
		.AddUObject(this, &UBRGA_Melee::OnTargetDataReady);
	ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
}

void UBRGA_Melee::OnWindowBegin(FGameplayEventData Payload)
{
	if (bWindowOpen || bSwingResolved)
	{
		return;
	}

	FVector ViewLocation, ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRGA_Melee: Event.Melee.WindowBegin with no resolvable view point; ending."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	bWindowOpen = true;
	SwingOriginLocation = ViewLocation;
}

void UBRGA_Melee::OnWindowEnd(FGameplayEventData Payload)
{
	if (!bWindowOpen)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGA_Melee: Event.Melee.WindowEnd arrived with no open window — the montage's notify "
				 "pair is mis-ordered or WindowBegin is missing. No swing was resolved."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	bWindowOpen = false;
	ResolveSwing();
}

void UBRGA_Melee::OnSwingWatchdogElapsed()
{
	if (bSwingResolved)
	{
		return;
	}

	if (!bWindowOpen)
	{
		static bool bLoggedNoAnimationOnce = false;
		if (!bLoggedNoAnimationOnce)
		{
			bLoggedNoAnimationOnce = true;
			UE_LOG(LogBRCombat, Warning,
				TEXT("BRGA_Melee IS RUNNING WITHOUT ANIMATION. No swing montage exists, so "
					 "Event.Melee.WindowBegin/WindowEnd never fired; the swing is being resolved from the "
					 "structural %.2f s fallback window instead. This is BP05 gap 3 — once the montage "
					 "lands, its notifies are authoritative and this fallback only ever guards against a "
					 "missing one. Logged once per process."),
				SwingWindowFallbackSeconds);
		}

		FVector ViewLocation, ViewDirection;
		if (!GetViewPoint(ViewLocation, ViewDirection))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
		SwingOriginLocation = ViewLocation;
	}
	else
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGA_Melee: Event.Melee.WindowBegin fired but WindowEnd did not within %.2f s. "
				 "Closing the window from the watchdog so the ability cannot leak."),
			SwingWindowFallbackSeconds);
	}

	bWindowOpen = false;
	ResolveSwing();
}

void UBRGA_Melee::OnClaimTimeoutElapsed()
{
	if (bClaimHandled)
	{
		return;
	}

	bClaimHandled = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UBRGA_Melee::ResolveSwing()
{
	if (bSwingResolved)
	{
		return;
	}
	bSwingResolved = true;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	float BaseDamage = 0.f, RangeUU = 0.f, SweepRadiusUU = 0.f, RearArcDegrees = 0.f;
	FString Missing;
	if (!ResolveMeleeTuning(BaseDamage, RangeUU, SweepRadiusUU, RearArcDegrees, Missing))
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRGA_Melee: CT_Combat lost '%s' between activation and the swing; nothing was resolved."), *Missing);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	FVector ViewLocation, ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FVector SweepEnd = ViewLocation + ViewDirection * RangeUU;
	const FHitResult Hit = SweepSwing(SwingOriginLocation, SweepEnd, SweepRadiusUU);

	FGameplayAbilityTargetData_SingleTargetHit* HitData = new FGameplayAbilityTargetData_SingleTargetHit(Hit);
	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(HitData);

	{
		FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());

		ASC->ServerSetReplicatedTargetData(
			CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey(),
			DataHandle,
			FGameplayTag(),
			ASC->ScopedPredictionKey);
	}

	OnTargetDataReady(DataHandle, FGameplayTag());
}

FHitResult UBRGA_Melee::SweepSwing(const FVector& From, const FVector& To, float SweepRadiusUU) const
{
	FHitResult Hit;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Hit;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRMeleeSwing), false);
	Params.AddIgnoredActor(GetAvatarActorFromActorInfo());

	World->SweepSingleByChannel(Hit, From, To, FQuat::Identity, BRCollision::MeleeTrace, FCollisionShape::MakeSphere(SweepRadiusUU), Params);

	if (!Hit.bBlockingHit)
	{
		Hit.TraceStart = From;
		Hit.TraceEnd = To;
		Hit.ImpactPoint = To;
	}
	return Hit;
}

void UBRGA_Melee::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag)
{
	if (bClaimHandled)
	{
		return;
	}
	bClaimHandled = true;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (ASC->GetOwnerRole() == ROLE_Authority)
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* Claim = Data ? Data->GetHitResult() : nullptr;

	if (!Claim || !Claim->GetActor())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		float BaseDamage = 0.f, RangeUU = 0.f, SweepRadiusUU = 0.f, RearArcDegrees = 0.f;
		FString Missing;
		FString Reason;

		if (!ResolveMeleeTuning(BaseDamage, RangeUU, SweepRadiusUU, RearArcDegrees, Missing))
		{
			UE_LOG(LogBRCombat, Error, TEXT("BRGA_Melee: CT_Combat is missing '%s' on the server; no damage was applied."), *Missing);
		}
		else if (!ValidateClaim(*Claim, RangeUU, Reason))
		{
			UE_LOG(LogBRCombat, Warning, TEXT("BRGA_Melee REJECTED a client claim: %s"), *Reason);
		}
		else
		{
			const bool bRearHit = IsRearHit(Claim->GetActor(), RearArcDegrees);
			ApplyMeleeDamage(*Claim, BaseDamage, bRearHit);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UBRGA_Melee::ValidateClaim(const FHitResult& Claim, float RangeUU, FString& OutReason) const
{
	const AActor* Attacker = GetAvatarActorFromActorInfo();
	AActor* Victim = Claim.GetActor();
	if (!Attacker || !Victim)
	{
		OutReason = TEXT("server has no avatar for the attacker or the victim");
		return false;
	}

	if (Victim == Attacker)
	{
		OutReason = TEXT("the claim names the attacker as its own victim");
		return false;
	}

	FVector ServerLocation, ServerDirection;
	if (!GetViewPoint(ServerLocation, ServerDirection))
	{
		OutReason = TEXT("server could not resolve the attacker's view point");
		return false;
	}

	const FVector VictimLocation = Victim->GetActorLocation();

	const float MaxReachUU = RangeUU + ReachToleranceUU;
	const float ClaimedDistance = FVector::Dist(ServerLocation, VictimLocation);
	if (ClaimedDistance > MaxReachUU)
	{
		OutReason = FString::Printf(
			TEXT("victim '%s' is %.0f uu from the server's attacker; melee reach (+tolerance) is %.0f uu"),
			*GetNameSafe(Victim), ClaimedDistance, MaxReachUU);
		return false;
	}

	const FVector ToVictim = (VictimLocation - ServerLocation).GetSafeNormal();
	if (!ToVictim.IsNearlyZero() && FVector::DotProduct(ServerDirection, ToVictim) <= 0.f)
	{
		OutReason = FString::Printf(
			TEXT("victim '%s' is behind the server's view direction for the attacker"), *GetNameSafe(Victim));
		return false;
	}

	if (const UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BRMeleeLineOfSight), false);
		Params.AddIgnoredActor(Attacker);

		FHitResult Blocker;
		if (World->LineTraceSingleByChannel(Blocker, ServerLocation, VictimLocation, BRCollision::MeleeTrace, Params)
			&& Blocker.GetActor() != Victim)
		{
			OutReason = FString::Printf(
				TEXT("'%s' blocks the line from the attacker to victim '%s' — melee through geometry"),
				*GetNameSafe(Blocker.GetActor()), *GetNameSafe(Victim));
			return false;
		}
	}

	return true;
}

bool UBRGA_Melee::IsRearHit(const AActor* Victim, float RearArcDegrees) const
{
	const AActor* Attacker = GetAvatarActorFromActorInfo();
	if (!Attacker || !Victim)
	{
		return false;
	}

	FVector VictimForward = Victim->GetActorForwardVector();
	VictimForward.Z = 0.f;

	FVector VictimToAttacker = Attacker->GetActorLocation() - Victim->GetActorLocation();
	VictimToAttacker.Z = 0.f;

	if (!VictimForward.Normalize() || !VictimToAttacker.Normalize())
	{
		return false;
	}

	const float HalfArcDegrees = FMath::Clamp(RearArcDegrees, 0.f, 360.f) * 0.5f;
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(HalfArcDegrees));

	return FVector::DotProduct(-VictimForward, VictimToAttacker) >= MinCos;
}

void UBRGA_Melee::ApplyMeleeDamage(const FHitResult& Hit, float BaseDamage, bool bRearHit) const
{
	UBRAbilitySystemComponent* SourceASC = GetBRAbilitySystemComponent();
	AActor* HitActor = Hit.GetActor();
	if (!SourceASC || !HitActor)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC || TargetASC == SourceASC)
	{
		return;
	}

	FGameplayTagContainer DamageTags;
	DamageTags.AddTag(BRGameplayTags::Damage_Melee);
	if (bRearHit)
	{
		DamageTags.AddTag(BRGameplayTags::Damage_Rear);
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	Context.AddHitResult(Hit);

	const FGameplayEffectSpecHandle Spec = UBRGE_Damage::MakeSpec(SourceASC, BaseDamage, DamageTags, Context);
	UBRGE_Damage::ApplyToTarget(Spec, SourceASC, TargetASC);
}

void UBRGA_Melee::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TargetDataDelegateHandle.IsValid())
	{
		if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
				.Remove(TargetDataDelegateHandle);
		}
		TargetDataDelegateHandle.Reset();
	}

	ClearTasks();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBRGA_Melee::ClearTasks()
{
	if (WindowBeginTask)
	{
		WindowBeginTask->EndTask();
		WindowBeginTask = nullptr;
	}
	if (WindowEndTask)
	{
		WindowEndTask->EndTask();
		WindowEndTask = nullptr;
	}
	if (WatchdogTask)
	{
		WatchdogTask->EndTask();
		WatchdogTask = nullptr;
	}
}

bool UBRGA_Melee::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return false;
	}

	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(OutLocation, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}
