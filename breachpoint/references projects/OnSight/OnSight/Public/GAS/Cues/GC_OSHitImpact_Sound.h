// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Cues/OSGameplayCueNotifyStatic.h"
#include "GC_OSHitImpact_Sound.generated.h"

class UAkAudioEvent;
class UNiagaraSystem;

/**
 * GameplayCue for tag GameplayCue.Sound.Hit.
 * Plays Wwise audio (AkEvent) at hit impact location (from EffectContext HitResult),
 * with optional Niagara VFX.
 *
 * Editor performance checklist:
 * - **Wwise**: assign Concurrency/MaxInstances (and virtual voice) to the hit event to prevent voice spam on burst hits.\n
 * - **Niagara**: use an EffectType with pooling + scalability; avoid expensive collision/lights for impact spam.\n
 * - **Cue culling**: set `MaxCueDistance` and/or `MinIntervalPerTarget` (inherited from `UOSGameplayCueNotifyStatic`) on the cue asset to reduce spikes.
 */
UCLASS()
class ONSIGHT_API UGC_OSHitImpact_Sound : public UOSGameplayCueNotifyStatic
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category="Wwise")
	TObjectPtr<UAkAudioEvent> HitAkEvent = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="VFX")
	TObjectPtr<UNiagaraSystem> ImpactVfx = nullptr;

	/** Fallback socket/bone on the victim mesh (used when hit bone/socket can't be resolved). */
	UPROPERTY(EditDefaultsOnly, Category="Socket")
	FName DefaultSocketName = NAME_None;

	// Instant effects (e.g., ApplyDamage) will call OnExecute.
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

	// Duration/infinite effects (e.g., DeathState) will call OnActive.
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};

