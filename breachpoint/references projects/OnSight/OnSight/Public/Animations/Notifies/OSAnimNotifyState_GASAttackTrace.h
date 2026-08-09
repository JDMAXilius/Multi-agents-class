// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSAnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Data/OSCombatTypes.h"
#include "Data/OSHitDamageContext.h"
#include "OSAnimNotifyState_GASAttackTrace.generated.h"

class UCameraShakeBase;
class UNiagaraSystem;
class UGameplayEffect;

/** Socket + radius for sweep trace. */
USTRUCT(BlueprintType)
struct FOSGASAttackTraceSocket
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Attack Trace")
	FName SocketName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Attack Trace", meta = (ClampMin = "1.0"))
	float Radius = 30.f;
};

/** Notify state: sweep between socket positions, apply damage via GAS (Data.Damage, server only). */
UCLASS()
class ONSIGHT_API UOSAnimNotifyState_GASAttackTrace : public UOSAnimNotifyState
{
	GENERATED_BODY()

public:
	UOSAnimNotifyState_GASAttackTrace();
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Attack Trace")
	TArray<FOSGASAttackTraceSocket> Sockets;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Attack Trace", meta = (ClampMin = "0.0"))
	float Damage = 5.0f;

	/** Authored per notify window; copied into the gameplay effect context for hit reaction and downstream VFX/SFX/UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS Attack Trace")
	EOSAttackType AttackType = EOSAttackType::Light;

	/** Optional override: if set, the authoritative impact cue will spawn on this socket/bone on the victim mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS Attack Trace")
	FName ImpactCueSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Attack Trace")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Attack Trace", meta = (ClampMin = "0.0"))
	float MinHitInterval = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Attack Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebug = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.0"))
	float DebugDrawLifetime = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawOnlyOnHit = true;

	// --- Predicted cosmetics (client-only, attacker only) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Predicted Cosmetics")
	bool bEnablePredictedHitCosmetics = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Predicted Cosmetics")
	TSubclassOf<UCameraShakeBase> PredictedHitCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Predicted Cosmetics", meta=(ClampMin="0.0"))
	float PredictedHitShakeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Predicted Cosmetics")
	TObjectPtr<UNiagaraSystem> PredictedHitVfx = nullptr;

private:
	struct FPerMeshState
	{
		TArray<FVector> PrevPositions;
		float LastHitTime = -FLT_MAX;
		TSet<TWeakObjectPtr<AActor>> HitActorsThisNotify;

		// Cache the effect class for this notify window (performance + strictness).
		TSubclassOf<UGameplayEffect> CachedDamageEffectClass;
	};
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FPerMeshState> State;
};
