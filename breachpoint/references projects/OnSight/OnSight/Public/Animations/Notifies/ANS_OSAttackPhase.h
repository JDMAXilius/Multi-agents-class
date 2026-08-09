#pragma once

#include "CoreMinimal.h"
#include "OSAnimNotifyState.h"
#include "ANS_OSAttackPhase.generated.h"

/**
 * Attack Phase notify state — defines the Active (hitting) window of an attack.
 *
 * Place ONE instance per attack section on the montage timeline.
 * The notify's width IS the Active window. No properties, no configuration.
 *
 *   Windup...       [====== ACTIVE ======]       ...Recovery...
 *   ────────────────╠══════════════════════╣──────────────────
 *                   ▲                      ▲
 *              NotifyBegin            NotifyEnd
 *              → Active starts        → Recovery starts
 *
 * NotifyBegin: broadcasts GameplayEvent.Attack.Phase.Active to the owning ASC
 * NotifyEnd:   broadcasts GameplayEvent.Attack.Phase.Recovery to the owning ASC
 * NotifyTick:  no-op (GA owns all ticking behavior)
 *
 * The GA listens for these events via WaitGameplayEvent and drives phase transitions.
 * On montage interruption/cancellation, UE automatically calls NotifyEnd — ensuring
 * clean Recovery transition without leaked Active tags.
 */
UCLASS(meta = (DisplayName = "OS Attack Phase"))
class ONSIGHT_API UANS_OSAttackPhase : public UOSAnimNotifyState
{
	GENERATED_BODY()

public:
	UANS_OSAttackPhase();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
