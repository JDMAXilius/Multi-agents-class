#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIBSensorium.h"
#include "AIBBotController.generated.h"

class IAIBAvatarInterface;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
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

	/** Seconds between thinks. Config so the terminal can tune cadence without a
	 *  recompile; the floor law does not live here (the clock owns it). */
	UPROPERTY(Config)
	float ThinkIntervalSeconds = 0.1f;

	FAIBSensorium Sensorium;
	FTimerHandle ThinkTimer;

	/** For the one fairness log line per acquisition (aib-verifier's sample). */
	TWeakObjectPtr<AActor> LastLoggedTarget;

	IAIBAvatarInterface* Avatar = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> AvatarObject;
};
