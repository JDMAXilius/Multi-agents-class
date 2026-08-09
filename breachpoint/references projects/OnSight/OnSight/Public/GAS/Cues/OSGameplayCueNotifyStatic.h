// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "OSGameplayCueNotifyStatic.generated.h"

class UChooserTable;

/**
 * Base class for OnSight static gameplay cues.
 *
 * Performance knobs (MP-safe): you can cull presentation without touching gameplay.
 *
 * Recommended editor setup:
 * - **Wwise**: set Concurrency/MaxInstances (and virtual voice) for hit/death events to prevent spikes.
 * - **Niagara**: use an EffectType with Significance + Pooling + Scalability so impact/death VFX don't explode on bursts.
 */
UCLASS()
class ONSIGHT_API UOSGameplayCueNotifyStatic : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	/** Optional chooser table for cue-authored montage selection (presentation-only). */
	UPROPERTY(EditDefaultsOnly, Category="Chooser")
	TObjectPtr<UChooserTable> ChooserTable = nullptr;

	// Cull the entire cue if farther than this from the nearest local player viewpoint. 0 = no culling.
	UPROPERTY(EditDefaultsOnly, Category="Performance", meta=(ClampMin="0.0"))
	float MaxCueDistance = 0.0f;

	// Per-target rate limit. 0 = no rate limit. (Keyed by MyTarget + this cue tag.)
	UPROPERTY(EditDefaultsOnly, Category="Performance", meta=(ClampMin="0.0"))
	float MinIntervalPerTarget = 0.0f;

	// Skip cue execution on dedicated servers (recommended; they have no presentation).
	UPROPERTY(EditDefaultsOnly, Category="Performance")
	bool bSkipOnDedicatedServer = true;

	virtual void HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;
	
};
