#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AIBBotController.generated.h"

class IAIBAvatarInterface;

/**
 * The HAND. Owns the sensorium, hosts the brain, runs the executor, presses verbs —
 * and decides nothing itself: deciding is the brain's, acting is the avatar's.
 *
 * Server-only by construction (bots are spawned only by the authority's game mode) and
 * tickless by law: thinking runs on a timer, reactions run on matured stimuli. The seam
 * audit's lesson is baked in: because there is no tick, the engine's focus-based
 * UpdateControlRotation never runs — aim is stepped explicitly by the executor's tasks.
 *
 * Phase 0: possession finds the avatar door and proves the wiring with one log line.
 * Phase 1 adds the sensorium; Phase 2 the brain; Phase 3 the executor.
 */
UCLASS()
class AIBOT_API AAIBBotController : public AAIController
{
	GENERATED_BODY()

public:
	AAIBBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The avatar door, resolved at possession. Null when the pawn carries no adapter —
	 *  which is loud (one Error) and leaves the bot standing, never crashing. */
	IAIBAvatarInterface* GetAvatar() const { return Avatar; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	/** Raw interface pointer, valid only while the possessed pawn lives; cleared on
	 *  unpossess. Backed by AvatarObject so GC keeps the component alive. */
	IAIBAvatarInterface* Avatar = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> AvatarObject;
};
