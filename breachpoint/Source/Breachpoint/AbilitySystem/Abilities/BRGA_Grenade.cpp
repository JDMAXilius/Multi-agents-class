// BREACHPOINT — BP05 step 1. The grenade: cook, throw, and the server-only projectile spawn.
#include "AbilitySystem/Abilities/BRGA_Grenade.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Weapons/BRProjectile.h"

namespace
{
	const FName GrenadeCookSecondsCurve(TEXT("Grenade.CookSeconds"));
	const FName GrenadeFuseSecondsCurve(TEXT("Grenade.FuseSeconds"));
	const FName GrenadeThrowSpeedCurve(TEXT("Grenade.ThrowSpeedMetresPerSecond"));
	const FName GrenadeBlastRadiusCurve(TEXT("Grenade.BlastRadiusMetres"));
	const FName GrenadeBlastCentreDamageCurve(TEXT("Grenade.BlastCentreDamage"));

	const FName GrenadeBlastFalloffCurve(TEXT("Grenade.BlastFalloff"));

	const FName GrenadeBouncinessCurve(TEXT("Grenade.Bounciness"));
	const FName GrenadeBounceFrictionCurve(TEXT("Grenade.BounceFriction"));

	const TCHAR* GrenadeThrowCueTagString = TEXT("GameplayCue.Grenade.Throw");
	const TCHAR* GrenadeExplodeCueTagString = TEXT("GameplayCue.Grenade.Explode");
}

FGameplayTag UBRGA_Grenade::RequestOwedTag(const TCHAR* TagString)
{
	return FGameplayTag::RequestGameplayTag(FName(TagString), false);
}

UBRGA_Grenade::UBRGA_Grenade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	const FGameplayTag GrenadeTag = BRGameplayTags::Ability_Grenade;
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(GrenadeTag);
	SetAssetTags(AssetTags);

	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	CostGameplayEffectClass = UBRGE_GrenadeCost::StaticClass();

	bCommitOnActivate = true;
}

namespace
{
	const FName GrenadeCostPerThrowCurve(TEXT("Grenade.CostPerThrow"));
}

FGameplayEffectSpecHandle UBRGA_Grenade::MakeCostSpec() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !UBRGE_GrenadeCost::IsOperational())
	{
		return FGameplayEffectSpecHandle();
	}

	float CostPerThrow = 0.f;
	if (!BRCombatCurves::Evaluate(GrenadeCostPerThrowCurve, CostPerThrow) || CostPerThrow <= 0.f)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: CT_Combat has no '%s' row, so the throw cost is unknown. Refusing "
				 "rather than defaulting — a free grenade is not a safe fallback."),
			*GrenadeCostPerThrowCurve.ToString());
		return FGameplayEffectSpecHandle();
	}

	return UBRGE_GrenadeCost::MakeSpec(ASC, CostPerThrow, ASC->MakeEffectContext());
}

bool UBRGA_Grenade::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	const FGameplayEffectSpecHandle Spec = MakeCostSpec();
	if (!Spec.IsValid())
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return false;
	}

	const FGameplayAttribute GrenadeAttribute = UBRGE_GrenadeCost::ResolveGrenadeCountAttribute();
	if (!GrenadeAttribute.IsValid())
	{
		return false;
	}

	bool bFound = false;
	const float Held = ASC->GetGameplayAttributeValue(GrenadeAttribute, bFound);
	if (!bFound)
	{
		return false;
	}

	float CostPerThrow = 0.f;
	BRCombatCurves::Evaluate(GrenadeCostPerThrowCurve, CostPerThrow);
	return Held >= CostPerThrow;
}

void UBRGA_Grenade::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const FGameplayEffectSpecHandle Spec = MakeCostSpec();
	if (!Spec.IsValid())
	{
		return;
	}

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

bool UBRGA_Grenade::ResolveTuning(FBRGrenadeTuning& OutTuning, FString& OutReason) const
{
	auto ReadPositive = [&OutReason](FName CurveName, float& OutValue) -> bool
	{
		float Value = 0.f;
		if (!BRCombatCurves::Evaluate(CurveName, Value))
		{
			OutReason = FString::Printf(TEXT("CT_Combat has no '%s' row"), *CurveName.ToString());
			return false;
		}
		if (Value <= 0.f)
		{
			OutReason = FString::Printf(TEXT("CT_Combat row '%s' is %.3f; every grenade number must be positive"), *CurveName.ToString(), Value);
			return false;
		}
		OutValue = Value;
		return true;
	};

	FBRGrenadeTuning Resolved;
	if (!ReadPositive(GrenadeCookSecondsCurve, Resolved.CookSeconds)
		|| !ReadPositive(GrenadeFuseSecondsCurve, Resolved.FuseSeconds)
		|| !ReadPositive(GrenadeThrowSpeedCurve, Resolved.ThrowSpeedMetresPerSecond)
		|| !ReadPositive(GrenadeBlastRadiusCurve, Resolved.BlastRadiusMetres)
		|| !ReadPositive(GrenadeBlastCentreDamageCurve, Resolved.BlastCentreDamage))
	{
		return false;
	}

	float FalloffProbe = 0.f;
	if (!BRCombatCurves::Evaluate(GrenadeBlastFalloffCurve, 0.f, FalloffProbe))
	{
		OutReason = FString::Printf(TEXT("CT_Combat has no '%s' curve, so the blast has no falloff shape"), *GrenadeBlastFalloffCurve.ToString());
		return false;
	}

	BRCombatCurves::Evaluate(GrenadeBouncinessCurve, Resolved.Bounciness);
	BRCombatCurves::Evaluate(GrenadeBounceFrictionCurve, Resolved.BounceFriction);

	OutTuning = Resolved;
	return true;
}

bool UBRGA_Grenade::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!BRGas::IsStageEnabled(EBRGasStage::FullSandbox))
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UBRGA_Grenade::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bThrowResolved = false;
	Tuning = FBRGrenadeTuning();

	FString Reason;
	if (!ResolveTuning(Tuning, Reason))
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grenade refused: %s. The grenade's numbers are owed by BP13/curator; a "
				 "literal fuse or radius here would be a law-3 violation."),
			*Reason);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CookReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (!CookReleaseTask)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: could not create its WaitInputRelease task; refusing rather than "
				 "cooking a grenade that only a timer can throw."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CookReleaseTask->OnRelease.AddDynamic(this, &UBRGA_Grenade::HandleCookReleased);
	CookReleaseTask->ReadyForActivation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CookTimerHandle, this, &UBRGA_Grenade::HandleCookExpired, Tuning.CookSeconds, false);
	}
	else
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRGA_Grenade: no world, so the cook has no ceiling; refusing."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UBRGA_Grenade::HandleCookReleased(float TimeHeld)
{
	ThrowGrenade(TimeHeld);
}

void UBRGA_Grenade::HandleCookExpired()
{
	ThrowGrenade(Tuning.CookSeconds);
}

void UBRGA_Grenade::ThrowGrenade(float SecondsCooked)
{
	if (bThrowResolved)
	{
		return;
	}
	bThrowResolved = true;

	ClearCook();

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	FVector ViewLocation, ViewDirection;
	if (!ASC || !GetViewPoint(ViewLocation, ViewDirection))
	{
		UE_LOG(LogBRCombat, Warning, TEXT("UBRGA_Grenade: no ASC or no view point at the throw; nothing left the hand."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const float RemainingFuseSeconds = FMath::Max(Tuning.FuseSeconds - SecondsCooked, 0.f);

	const FTransform ReleaseTransform(ViewDirection.Rotation(), ViewLocation);
	const FVector LaunchVelocity = ViewDirection * (Tuning.ThrowSpeedMetresPerSecond * BRUnits::MetresToUU);

	const FGameplayTag ThrowCueTag = RequestOwedTag(GrenadeThrowCueTagString);
	if (ThrowCueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = ViewLocation;
		CueParams.Normal = ViewDirection;
		CueParams.Instigator = GetAvatarActorFromActorInfo();
		ASC->ExecuteGameplayCue(ThrowCueTag, CueParams);
	}
	else
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grenade: '%s' is not declared and has no C++ handler, so the client ghost "
				 "does not exist. The throw is invisible until the tag and its cue class land "
				 "together."),
			GrenadeThrowCueTagString);
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		RequestProjectileSpawn(ReleaseTransform, LaunchVelocity, RemainingFuseSeconds);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBRGA_Grenade::RequestProjectileSpawn(const FTransform& ReleaseTransform, const FVector& LaunchVelocity, float RemainingFuseSeconds) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: no world at the spawn; the throw was authorised and nothing left the hand."));
		return;
	}

	AActor* Thrower = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* ThrowerASC = GetAbilitySystemComponentFromActorInfo();
	if (!Thrower || !ThrowerASC)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: no avatar actor or no ASC at the spawn (avatar '%s'); nothing was "
				 "spawned, because an unattributable grenade is a scoring bug three systems away."),
			*GetNameSafe(Thrower));
		return;
	}

	const FGameplayTag ExplodeCueTag = RequestOwedTag(GrenadeExplodeCueTagString);
	if (!ExplodeCueTag.IsValid())
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("UBRGA_Grenade: '%s' is not declared and has no C++ handler, so this grenade's "
				 "detonation will be silent and invisible. Damage is unaffected."),
			GrenadeExplodeCueTagString);
	}

	FBRProjectileSpawnParams Params;
	Params.InstigatorActor = Thrower;
	Params.InstigatorASC = ThrowerASC;
	Params.LaunchVelocity = LaunchVelocity;
	Params.FuseSeconds = RemainingFuseSeconds;
	Params.Bounciness = Tuning.Bounciness;
	Params.BounceFriction = Tuning.BounceFriction;
	Params.BlastRadiusMetres = Tuning.BlastRadiusMetres;
	Params.BlastCentreDamage = Tuning.BlastCentreDamage;
	Params.BlastFalloffCurveName = GrenadeBlastFalloffCurve;
	Params.ExplodeCueTag = ExplodeCueTag;

	const ABRProjectile* Spawned = ABRProjectile::SpawnProjectile(
		World, ABRProjectile::StaticClass(), ReleaseTransform, Params);

	if (!Spawned)
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("UBRGA_Grenade: the projectile spawn FAILED (release at %s, launch %.0f uu/s, %.2f s "
				 "of fuse left). The throw was authorised on the authority and nothing is in the air; "
				 "the refusal above names the reason."),
			*ReleaseTransform.GetLocation().ToCompactString(), LaunchVelocity.Size(), RemainingFuseSeconds);
		return;
	}
}

void UBRGA_Grenade::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearCook();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBRGA_Grenade::ClearCook()
{
	if (CookReleaseTask)
	{
		CookReleaseTask->EndTask();
		CookReleaseTask = nullptr;
	}

	if (CookTimerHandle.IsValid())
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CookTimerHandle);
		}
		CookTimerHandle.Invalidate();
	}
}

bool UBRGA_Grenade::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
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
