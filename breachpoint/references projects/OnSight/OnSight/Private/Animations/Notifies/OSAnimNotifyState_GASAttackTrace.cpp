// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/Notifies/OSAnimNotifyState_GASAttackTrace.h"
#include "DrawDebugHelpers.h"
#include "Characters/OSCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Data/OSGameplayTags.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Effects/GE_OSApplyDamage.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "OSLogCategories.h"

#include "Core/OSDebugGlobals.h"

UOSAnimNotifyState_GASAttackTrace::UOSAnimNotifyState_GASAttackTrace()
{
	// Default to the single canonical damage GE pipeline (ExecCalc_OSDamage -> Damage meta -> Health).
	DamageEffectClass = UGE_OSApplyDamage::StaticClass();
}

void UOSAnimNotifyState_GASAttackTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp) return;
	if (Sockets.Num() == 0)
	{
		return;
	}
	FPerMeshState& StateData = State.FindOrAdd(MeshComp);
	StateData.PrevPositions.SetNum(Sockets.Num());
	StateData.HitActorsThisNotify.Empty();
	StateData.CachedDamageEffectClass = DamageEffectClass;
	if (!IsValid(StateData.CachedDamageEffectClass))
	{
		StateData.CachedDamageEffectClass = UGE_OSApplyDamage::StaticClass();
	}
	for (int32 i = 0; i < Sockets.Num(); ++i)
		StateData.PrevPositions[i] = MeshComp->GetSocketLocation(Sockets[i].SocketName);
	StateData.LastHitTime = -FLT_MAX;
}

void UOSAnimNotifyState_GASAttackTrace::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (!MeshComp) return;
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor)) return;

	// Guard: skip trace during montage blend-out if the owning attack ability has ended.
	// ActivationOwnedTags remove IsAttacking on EndAbility, so its absence means the
	// ability is gone and any hits during blend-out would be illegitimate.
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (SourceASC)
	{
		if (!SourceASC->HasMatchingGameplayTag(FOSGameplayTags::Get().IsAttacking))
			return;
	}

	if (!OwnerActor->HasAuthority())
	{
		FPerMeshState* StateData = State.Find(MeshComp);
		if (!StateData) return;

		// Always keep positions updated client-side (so predicted trace is stable).
		APawn* OwnerPawn = Cast<APawn>(OwnerActor);
		const bool bCanPredictCosmetics =
			bEnablePredictedHitCosmetics &&
			(PredictedHitCameraShake || PredictedHitVfx) &&
			IsValid(OwnerPawn) &&
			OwnerPawn->IsLocallyControlled() &&
			IsValid(SourceASC) &&
			StateData->HitActorsThisNotify.IsEmpty(); // only once per notify window

		UWorld* World = bCanPredictCosmetics ? OwnerActor->GetWorld() : nullptr;

		FCollisionObjectQueryParams ObjParams;
		if (bCanPredictCosmetics)
		{
			ObjParams.AddObjectTypesToQuery(TraceChannel);
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OSGASAttackTrace_Predicted), false, OwnerActor);
		QueryParams.bReturnPhysicalMaterial = false;

		for (int32 i = 0; i < Sockets.Num(); ++i)
		{
			const FOSGASAttackTraceSocket& Socket = Sockets[i];
			const FVector Prev = StateData->PrevPositions.IsValidIndex(i) ? StateData->PrevPositions[i] : MeshComp->GetSocketLocation(Socket.SocketName);
			const FVector Curr = MeshComp->GetSocketLocation(Socket.SocketName);
			StateData->PrevPositions[i] = Curr;
			if (Prev.Equals(Curr, KINDA_SMALL_NUMBER)) continue;

			if (!bCanPredictCosmetics || !IsValid(World)) continue;

			TArray<FHitResult> Hits;
			const bool bHit = World->SweepMultiByObjectType(
				Hits, Prev, Curr, FQuat::Identity, ObjParams,
				FCollisionShape::MakeSphere(Socket.Radius),
				QueryParams);

			if (!bHit) continue;

			for (const FHitResult& HR : Hits)
			{
				AActor* HitActor = HR.GetActor();
				if (!IsValid(HitActor) || HitActor == OwnerActor) continue;
				// Team filter (client predicted cosmetics): skip teammates so the attacker doesn't
				// see hit sparks / camera shake when their sweep overlaps a friendly.
				if (UOSCombatBlueprintLibrary::AreActorsFriendly(OwnerActor, HitActor)) continue;
				StateData->HitActorsThisNotify.Add(HitActor);

				// Optional predicted spark (attacker-only)
				if (PredictedHitVfx)
				{
					// #106: Route predicted hit VFX through the GameplayCue system
					// so we don't bypass cue logic (socket resolution, cue culling, etc.).
					const FGameplayTag HitCueTag =
						FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Sound.Hit"), false);

					if (HitCueTag.IsValid() && IsValid(SourceASC))
					{
						UAbilitySystemComponent* TargetASC =
							UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);
						if (IsValid(TargetASC))
						{
							FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
							EffectContext.AddSourceObject(OwnerActor);
							EffectContext.AddHitResult(HR);

							// Helps GC_OSHitImpact_Sound resolve to an authored socket/bone when possible.
							FOSGameplayEffectContext* OSCtx = nullptr;
							if (TryGetOSGameplayEffectContext(EffectContext, OSCtx) && OSCtx)
							{
								if (!ImpactCueSocketName.IsNone())
								{
									OSCtx->CueSocketName = ImpactCueSocketName;
								}
							}

							FGameplayCueParameters CueParams;
							CueParams.EffectContext = EffectContext;
							CueParams.SourceObject = OwnerActor;
							CueParams.Instigator = OwnerActor;

							TargetASC->ExecuteGameplayCue(HitCueTag, CueParams);
						}
					}
				}

				// Optional predicted camera shake (attacker-only)
				if (PredictedHitCameraShake)
				{
					if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
					{
						PC->ClientStartCameraShake(PredictedHitCameraShake, PredictedHitShakeScale);
					}
				}

				// Only once per notify window (HitActorsThisNotify now non-empty).
				return;
			}
		}
		return;
	}
	AOSCharacter* Character = Cast<AOSCharacter>(OwnerActor);
	if (!IsValid(Character)) return;
	if (!IsValid(SourceASC)) return;

	FPerMeshState* StateData = State.Find(MeshComp);
	if (!StateData) return;

	TSubclassOf<UGameplayEffect> EffectClassToUse = StateData->CachedDamageEffectClass;
	if (!IsValid(EffectClassToUse)) return;
	UWorld* World = Character->GetWorld();
	if (!IsValid(World)) return;

	const float Now = World->GetTimeSeconds();
	if (MinHitInterval > 0.f && (Now - StateData->LastHitTime) < MinHitInterval)
	{
		for (int32 i = 0; i < Sockets.Num(); ++i)
			StateData->PrevPositions[i] = MeshComp->GetSocketLocation(Sockets[i].SocketName);
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OSGASAttackTrace), false, Character);
	QueryParams.bReturnPhysicalMaterial = false;
	TArray<AActor*> HitActorsThisTick;

	for (int32 i = 0; i < Sockets.Num(); ++i)
	{
		const FOSGASAttackTraceSocket& Socket = Sockets[i];
		const FVector Prev = StateData->PrevPositions.IsValidIndex(i) ? StateData->PrevPositions[i] : MeshComp->GetSocketLocation(Socket.SocketName);
		const FVector Curr = MeshComp->GetSocketLocation(Socket.SocketName);
		StateData->PrevPositions[i] = Curr;
		if (Prev.Equals(Curr, KINDA_SMALL_NUMBER)) continue;

		TArray<FHitResult> Hits;
		FCollisionObjectQueryParams ObjParams;
		ObjParams.AddObjectTypesToQuery(TraceChannel);
		const bool bHit = World->SweepMultiByObjectType(Hits, Prev, Curr, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(Socket.Radius), QueryParams);

		const bool bShouldDraw = bDrawDebug
#if !UE_BUILD_SHIPPING
			|| GDebugAttackTraceViz
#endif
			;
		if (bShouldDraw && World)
		{
			const float DrawLife = bDrawDebug ? DebugDrawLifetime : 0.08f;
			DrawDebugLine(World, Prev, Curr, FColor::Cyan, false, DrawLife, 0, 1.0f);
			DrawDebugSphere(World, Prev, Socket.Radius, 12, FColor::Cyan, false, DrawLife, 0, 0.5f);
			DrawDebugSphere(World, Curr, Socket.Radius, 12, FColor::Cyan, false, DrawLife, 0, 0.5f);
			if (bHit) DrawDebugSphere(World, Curr, Socket.Radius, 12, FColor::Green, false, DrawLife, 0, 1.0f);
		}
		if (!bHit) continue;

		for (const FHitResult& HitResult : Hits)
		{
			AActor* HitActor = HitResult.GetActor();
			if (!IsValid(HitActor) || HitActor == Character) continue;
			if (StateData->HitActorsThisNotify.Contains(HitActor)) continue;

			// Damage pipeline (team, invuln, FOS context, GE, Event_AttackHit) lives in UOSCombatBlueprintLibrary.
			if (!UOSCombatBlueprintLibrary::ApplyDamageFromHitResult(
				Character,
				HitActor,
				HitResult,
				EffectClassToUse,
				Damage,
				AttackType,
				ImpactCueSocketName,
				true))
			{
				continue;
			}

			UE_LOG(LogOSGASAttackTrace, Verbose,
				TEXT("[GASAttackTrace] Applied %s Damage=%.2f Instigator=%s Target=%s"),
				*GetNameSafe(EffectClassToUse),
				Damage,
				*GetNameSafe(Character),
				*GetNameSafe(HitActor));

			StateData->HitActorsThisNotify.Add(HitActor);
			HitActorsThisTick.Add(HitActor);
			if (bShouldDraw && World)
			{
				const float HitDrawLife = bDrawDebug ? DebugDrawLifetime : 0.08f;
				DrawDebugPoint(World, HitResult.ImpactPoint, 10.f, FColor::Yellow, false, HitDrawLife);
				DrawDebugString(World, HitResult.ImpactPoint, TEXT("HIT"), nullptr, FColor::Yellow, HitDrawLife);
			}
		}
	}
	if (HitActorsThisTick.Num() > 0) StateData->LastHitTime = Now;
}

void UOSAnimNotifyState_GASAttackTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (!MeshComp) return;
	State.Remove(MeshComp);
}
