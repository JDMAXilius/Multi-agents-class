// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "GC_ShieldBubble.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UAbilitySystemComponent;
class UNiagaraSystem;

/**
 GameplayCue actor for a simple shield bubble visual.
 Recommended usage: create a BP child `GC_ShieldBubble` and set Mesh/Material defaults there.
 */
UCLASS()
class ONSIGHT_API AGC_ShieldBubble : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AGC_ShieldBubble();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ShieldBubble")
	TObjectPtr<UStaticMeshComponent> BubbleMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShieldBubble")
	TObjectPtr<UStaticMesh> BubbleStaticMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShieldBubble")
	TObjectPtr<UMaterialInterface> BubbleMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShieldBubble")
	FName AttachSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShieldBubble")
	FVector RelativeLocation = FVector::ZeroVector;

	// Relative scale of the bubble at full stamina. The bubble will scale between this and MinScale based on current stamina percentage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShieldBubble")
	FVector RelativeScale = FVector(1.f);

	// Scale of the bubble at minimum stamina. The bubble will scale between this and RelativeScale based on current stamina percentage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShieldBubble")
	float MinScale = 0.5f;

	// When true, the bubble inherits the attached actor's rotation instead of staying world-aligned.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShieldBubble")
	bool bInheritRotation;

	// When true, the bubble scales with current stamina percentage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShieldBubble")
	bool bAdjustScale;

	// Niagara system spawned at the impact point when the bubble is hit (see OnShieldHit).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShieldBubble|FX")
	TObjectPtr<UNiagaraSystem> HitVFX;

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	/* Routed here whenever the Cue_ShieldBubble tag is Executed (fire-and-forget) while the bubble
	   is already active. Reads Parameters.Normal and forwards to OnShieldHit. */
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	/* Spawn HitVFX attached to the bubble, oriented along the inverted hit normal.
	   Call from BP (cheat/test) or from C++ block-hit pipeline. */
	UFUNCTION(BlueprintCallable, Category = "ShieldBubble")
	void OnShieldHit(const FVector& HitNormal);

private:
	void OnStaminaChanged(const FOnAttributeChangeData& Data);
	void ApplyShieldScale(float Current, float Max);

	UPROPERTY()
	UAbilitySystemComponent* CachedASC = nullptr;

	float CachedMaxStamina = 100.f;
	FDelegateHandle StaminaDelegateHandle;
};

