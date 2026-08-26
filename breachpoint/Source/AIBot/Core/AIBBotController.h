#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Core/AIBTypes.h"
#include "GameplayTagContainer.h"
#include "Perception/AIBSensorium.h"
#include "AIBBotController.generated.h"

class IAIBAvatarInterface;
class IAIBExecutor;
class UAIBAmbitionEngine;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UStateTree;
class UStateTreeAIComponent;
struct FAIStimulus;

/**
 * The HAND. Owns the engine perception (eyes/ears), feeds the sensorium, hosts the brain
 * (Phase 2), runs the executor (Phase 3), presses verbs — and decides nothing itself.
 *
 * Server-only by construction, tickless by law: a think timer pumps the sensorium and,
 * later, the brain. Because there is no tick, the engine's focus-based aim never runs —
 * aim will be stepped explicitly by executor tasks (the seam audit's lesson).
 *
 * Perception is FFA-open (detect everyone; hostility is decided above perception, so
 * teams can land later without touching the senses) — the pattern transcribed from the
 * host's compiled controller, not designed fresh.
 */
UCLASS(Config=Game)
class AIBOT_API AAIBBotController : public AAIController
{
	GENERATED_BODY()

public:
	AAIBBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The avatar door, resolved at possession. Null when the pawn carries no adapter —
	 *  loud (one Error) and the bot stands, never crashes. */
	IAIBAvatarInterface* GetAvatar() const { return Avatar; }

	/** The matured world — the ONLY awareness anything downstream may read. */
	const FAIBSensorium& GetSensorium() const { return Sensorium; }

	/** The arbitration layer; valid while possessing on the authority. */
	UAIBAmbitionEngine* GetAmbitionEngine() const { return AmbitionEngine; }

	/** The execution surface (Phase 3). The executor drives it; nothing else touches it. */
	UStateTreeAIComponent* GetStateTreeComponent() const { return StateTreeComponent; }

	/** Where the compiled behaviour asset lives — ini-set, resolved by the executor at
	 *  possession. Soft by law 3's sibling rule: the module hard-references no asset. */
	const TSoftObjectPtr<UStateTree>& GetBotStateTreePath() const { return BotStateTree; }

	/** The last matured, arbitrated world — what executor tasks read mid-frame. Facts are
	 *  built ONCE per Think (F3: one belief sample per pump); tasks reading this cache
	 *  cannot accidentally re-sample the live world between pumps. */
	const FAIBFacts& GetLastFacts() const { return LastFacts; }

	/** The game's projectile warning seam calls this (via the adapter wiring). It NOTES —
	 *  the dodge happens only after the stimulus matures (FAIRPLAY F2). */
	void NoteIncomingBlast(const FVector& Center, float Radius, double DetonateAtSeconds);

	// FFA seam, verbatim from the host's proven pattern: one shared "no team",
	// hostility decided per-pawn. A team system replaces these two overrides.
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(255); }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);

private:
	void Think();

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAIPerceptionComponent> BotPerception;

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	/** Born in the constructor (components must be); logic start is the executor's call,
	 *  never automatic — the host controller's proven, respawn-idempotent shape. */
	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	/** Soft path to the compiled tree, from [/Script/AIBot.AIBBotController] in ini.
	 *  TICKET_AIB2's authoring commandlet builds the asset it names. */
	UPROPERTY(Config)
	TSoftObjectPtr<UStateTree> BotStateTree;

	/** Seconds between thinks. Config so the terminal can tune cadence without a
	 *  recompile; the floor law does not live here (the clock owns it). */
	UPROPERTY(Config)
	float ThinkIntervalSeconds = 0.1f;

	FAIBSensorium Sensorium;
	FTimerHandle ThinkTimer;

	UPROPERTY()
	TObjectPtr<UAIBAmbitionEngine> AmbitionEngine;

	/** For the one ambition-switch log line per change (the verifier's instrument). */
	FGameplayTag LastLoggedAmbition;

	/** For the one fairness log line per acquisition (aib-verifier's sample). */
	TWeakObjectPtr<AActor> LastLoggedTarget;

	/** One fact snapshot per Think; see GetLastFacts. */
	FAIBFacts LastFacts;

	IAIBAvatarInterface* Avatar = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> AvatarObject;

	// The executor door, the avatar door's twin: the interface pointer is what runs, the
	// UObject pointer is what keeps it alive. Today the concrete type is the StateTree
	// executor; a Behavior Tree impl replaces one NewObject line, nothing else.
	IAIBExecutor* Executor = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> ExecutorObject;
};
