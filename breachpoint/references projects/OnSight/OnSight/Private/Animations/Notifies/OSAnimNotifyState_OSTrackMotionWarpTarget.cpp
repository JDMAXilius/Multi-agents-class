#include "Animations/Notifies/OSAnimNotifyState_OSTrackMotionWarpTarget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/Abilities/GA_OSAttack.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "OSLogCategories.h"

UOSAnimNotifyState_OSTrackMotionWarpTarget::UOSAnimNotifyState_OSTrackMotionWarpTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FName UOSAnimNotifyState_OSTrackMotionWarpTarget::ResolveWarpName(const AActor* Owner) const
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		if (const UGA_OSAttack* AttackAbility = Cast<UGA_OSAttack>(ASC->GetAnimatingAbility()))
		{
			return AttackAbility->GetWarpTargetName();
		}
	}
	return WarpTargetName;
}

bool UOSAnimNotifyState_OSTrackMotionWarpTarget::TryInjectFallbackTarget(UMotionWarpingComponent* MWComp, FName ResolvedName) const
{
	AActor* Owner = MWComp->GetOwner();
	AActor* SoftTarget = UOSCombatBlueprintLibrary::FindBestSoftTarget(Owner, MaxTargetRange);
	if (SoftTarget)
	{
		const FVector Dir = (SoftTarget->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		const FRotator Rot(0.f, Dir.Rotation().Yaw, 0.f);
		FVector WarpLoc = SoftTarget->GetActorLocation() - Dir * 90.f;
		WarpLoc.Z = Owner->GetActorLocation().Z;
		MWComp->AddOrUpdateWarpTargetFromTransform(ResolvedName, FTransform(Rot, WarpLoc));
		return true;
	}

	if (DefaultWarpDistanceWhenNoTarget > 0.f)
	{
		const FVector Dir = Owner->GetActorForwardVector().GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			const FVector OwnerLoc = Owner->GetActorLocation();
			const FRotator Rot(0.f, Dir.Rotation().Yaw, 0.f);
			FVector WarpLoc = OwnerLoc + Dir * DefaultWarpDistanceWhenNoTarget;
			WarpLoc.Z = OwnerLoc.Z;
			MWComp->AddOrUpdateWarpTargetFromTransform(ResolvedName, FTransform(Rot, WarpLoc));
			return true;
		}
	}

	return false;
}

URootMotionModifier* UOSAnimNotifyState_OSTrackMotionWarpTarget::AddRootMotionModifier_Implementation(
	UMotionWarpingComponent* MWComp,
	const UAnimSequenceBase* Animation,
	float StartTime,
	float EndTime) const
{
	if (!MWComp || !RootMotionModifier)
	{
		return nullptr;
	}

	URootMotionModifier_Warp* Template = Cast<URootMotionModifier_Warp>(RootMotionModifier);
	if (!Template)
	{
		UE_LOG(LogOSTrackMotionWarpTarget, Warning, TEXT("RootMotionModifier is not a URootMotionModifier_Warp subclass"));
		return Super::AddRootMotionModifier_Implementation(MWComp, Animation, StartTime, EndTime);
	}

	const FName ResolvedName = ResolveWarpName(MWComp->GetOwner());

	// If no warp target exists, check for non-GA fallback path.
	if (!MWComp->FindWarpTarget(ResolvedName))
	{
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MWComp->GetOwner());
		const bool bIsGADriven = ASC && Cast<UGA_OSAttack>(ASC->GetAnimatingAbility()) != nullptr;

		if (bIsGADriven)
		{
			// GA is animating but didn't inject targets or name mismatch — skip modifier
			// to avoid feedback loops from fallback targeting.
			UE_LOG(LogOSTrackMotionWarpTarget, Warning, TEXT("GA is active but warp target '%s' not found. Skipping warp to avoid feedback."), *ResolvedName.ToString());
			return nullptr;
		}

		if (!TryInjectFallbackTarget(MWComp, ResolvedName))
		{
			return nullptr;
		}
	}

	// Set-restore: configure template with resolved name, then delegate to Super.
	// Safe because AddRootMotionModifier is synchronous on the game thread.
	const FName OriginalName = Template->WarpTargetName;
	Template->WarpTargetName = ResolvedName;

	URootMotionModifier* Result = Super::AddRootMotionModifier_Implementation(MWComp, Animation, StartTime, EndTime);

	Template->WarpTargetName = OriginalName;

	return Result;
}

void UOSAnimNotifyState_OSTrackMotionWarpTarget::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp) return;

	ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	if (!Character) return;

	// Only refresh on the locally controlling client or the server.
	// Simulated proxies get warp data from replicated CMC state, not live input.
	if (!Character->IsLocallyControlled() && !Character->HasAuthority()) return;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
	if (!ASC) return;

	if (UGA_OSAttack* AttackAbility = Cast<UGA_OSAttack>(ASC->GetAnimatingAbility()))
	{
		if (AttackAbility->WantsPerTickWarpRefresh())
		{
			AttackAbility->RefreshWarpTarget(Character, FrameDeltaTime);
		}
	}
}

void UOSAnimNotifyState_OSTrackMotionWarpTarget::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (!ASC) return;

	// Combo-chain abilities opt out — Hit N's warp target must survive Hit (N-1)'s blend-out.
	// Non-opt-out abilities (grab, any future non-combo MW-using ability) get clean cleanup
	// so MW stops correcting once the authored notify window closes.
	bool bPersistAcrossEnd = false;
	if (const UOSGameplayAbility* Ability = Cast<UOSGameplayAbility>(ASC->GetAnimatingAbility()))
	{
		bPersistAcrossEnd = Ability->PersistsWarpTargetsAcrossNotifyEnd();
	}

	const FName ResolvedName = ResolveWarpName(Owner);
	bool bCleared = false;
	if (!bPersistAcrossEnd)
	{
		if (UMotionWarpingComponent* MWC = Owner->FindComponentByClass<UMotionWarpingComponent>())
		{
			MWC->RemoveWarpTarget(ResolvedName);
			bCleared = true;
		}
	}

	UE_LOG(LogOSTrackMotionWarpTarget, Verbose,
		TEXT("[NotifyEnd] Owner=%s resolvedName=%s persistAcrossEnd=%d cleared=%d"),
		*GetNameSafe(Owner), *ResolvedName.ToString(), bPersistAcrossEnd ? 1 : 0, bCleared ? 1 : 0);
}

FString UOSAnimNotifyState_OSTrackMotionWarpTarget::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("OS Track Motion Warp [%s]"), *WarpTargetName.ToString());
}

#if WITH_EDITOR
void UOSAnimNotifyState_OSTrackMotionWarpTarget::ValidateAssociatedAssets()
{
	if (WarpTargetName.IsNone())
	{
		UE_LOG(LogOSTrackMotionWarpTarget, Warning,
			TEXT("WarpTargetName is None. A target name must be set."));
	}
}
#endif
