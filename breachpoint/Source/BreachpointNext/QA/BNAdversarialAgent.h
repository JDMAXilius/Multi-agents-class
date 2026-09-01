#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "BNAdversarialAgent.generated.h"

class UBNAbilitySystemComponent;
class UGameplayAbility;

/** What the probe is currently doing to the game. Cycled on a timer — the loop the
 *  whole tool exists for. FreezeProbe and DeadInput are NOT in this cycle: they are
 *  opportunistic modes the act tick enters whenever the pawn is frozen or dead,
 *  because those windows open on the match's schedule, not ours. */
enum class EBNAQABehavior : uint8
{
	Roam,           // baseline: pathed wander — the control the anomaly detectors calibrate against
	BoundaryProbe,  // direct (un-pathed) walks into walls and map edges, jumping at them, 8 headings
	LedgeDive,      // sprint straight off the farthest edge — trying to LEAVE the playable space
	GrappleAbuse,   // grapple at the sky, the floor, and nothing, jumping mid-flight — speed exploits
	AbilityMash,    // every input tag, rapid, in illegal combinations — ADS+sprint+fire, crouch strobe
	COUNT
};

/** One deduplicated defect record. Everything the report schema needs; the JSON writer
 *  in WriteReport is the only consumer. Plain struct — nothing here replicates. */
struct FBNAQAFinding
{
	FString ErrorType;      // machine-readable class, e.g. "speed_violation"
	FString Severity;       // "high" | "medium"
	FString Behavior;       // which probe behavior was running
	FString MatchState;     // engine MatchState FName at detection
	FVector Location = FVector::ZeroVector;
	FString NearestAnchor;  // closest PlayerStart (name + distance) — the human-readable "where"
	FString Evidence;       // one sentence with the numbers that convicted it
	float SimTimeS = 0.f;
	FDateTime FirstUtc;
	int32 Occurrences = 1;

	// game context at first detection — the report's per-finding game_context block
	bool bAlive = true;
	float HealthNorm = 1.f;
	float ShieldNorm = 1.f;
	float SpeedUU = 0.f;
};

/**
 * ASSIGNMENT #9 — the adversarial QA probe. An AIController that joins the match through
 * the SAME doors a bot does (GenericPlayerInitialization -> RestartPlayer, presses on the
 * PlayerState ASC via AbilityInputTagPressed) and then, instead of trying to win, tries
 * to BREAK: it cycles the behaviors above continuously and runs detectors against every
 * sample, writing a structured JSON report to Saved/AdversarialQA/.
 *
 * What "broken" means here, exactly — the seven detector classes:
 *   fell_out_of_world_alive   below WorldSettings KillZ and still alive past a grace window
 *   escaped_playable_space    standing on ground OUTSIDE the arena's PlayerStart hull + margin
 *   stuck_state               an active move request, alive, unfrozen, yet speed ~0 for 3s
 *   speed_violation           ground speed far past CMC MaxWalkSpeed with no grapple/fall excuse
 *   attribute_anomaly         health/shield NaN, negative, or above its own max
 *   teleport_discontinuity    more displacement in one 100ms sample than any legal mover can make
 *   acted_while_dead / input_during_freeze   an ability ACTIVATED while State.Dead or
 *                             State.Match.Frozen was on the ASC — the gates exist; this
 *                             probe leans on them all run long and records any give
 *
 * Server-authoritative on purpose: the probe runs on the authority and presses the same
 * gated path a client's intent reaches, so anything it breaks a player could break.
 * Law 4: no Tick — the loop is three timers (behavior switch / act / probe sample).
 * QA-only: dormant unless `bn.aqa.start` is issued; never spawned by the mode.
 *
 * Console:  bn.aqa.start [seconds=180]   spawn the probe into the current authority world
 *           bn.aqa.stop                  end early; the report writes on any stop
 */
UCLASS()
class BREACHPOINTNEXT_API ABNAQAController : public AAIController
{
	GENERATED_BODY()

public:
	ABNAQAController();

	/** Begin a run: computes the arena hull, arms the three timers, stamps the clock. */
	void StartRun(float DurationSeconds);

	/** End, write the report, log the path, despawn pawn + self. Idempotent. */
	/** BY VALUE, not const&: this is bound as a TIMER PAYLOAD
	 *  (FTimerDelegate::CreateUObject(this, &StopRun, FString(...))), and the payload
	 *  overload cannot bind an FString payload through a const-reference parameter — it
	 *  broke the build for every session on the branch (BN27). Copying one FString once
	 *  per run is not a cost worth a cleverer binding.
	 *
	 *  Changed by the team-colours packet under an explicit law-5 exception (founder,
	 *  28 Aug) purely to unblock the branch; QA/ is not that packet's folder and nothing
	 *  else in this file was touched. */
	void StopRun(FString Reason);

	virtual void OnPossess(APawn* InPawn) override;

	/** A PIE stop, a level travel, or any world teardown is a STOP: the run's evidence
	 *  must not die with the world. StopRun is only ever reached from the end timer or
	 *  bn.aqa.stop, so before this override a session ended early wrote no report at all. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	// -- the loop -------------------------------------------------------------------
	void AdvanceBehavior();          // BehaviorTimer: next EBNAQABehavior, reset per-behavior state
	void ActTick();                  // ActTimer (4 Hz): issue moves / presses for the current behavior
	void ProbeTick();                // ProbeTimer (10 Hz): run every detector against a fresh sample

	// -- acting ---------------------------------------------------------------------
	void Press(FGameplayTag InputTag);
	void PressAndRelease(FGameplayTag InputTag);
	void MashAllVerbs();             // the illegal-combo pressure, also used while dead/frozen
	UBNAbilitySystemComponent* GetASC() const;

	// -- detecting ------------------------------------------------------------------
	void OnAbilityActivated(UGameplayAbility* Ability);
	void RecordFinding(const FString& ErrorType, const FString& Severity, const FString& Evidence);
	FString NearestAnchor(const FVector& At) const;
	void WriteReport();

	// -- run state ------------------------------------------------------------------
	FTimerHandle BehaviorTimer, ActTimer, ProbeTimer, EndTimer;
	EBNAQABehavior Behavior = EBNAQABehavior::Roam;
	int32 BehaviorCycles = 0;        // total switches — proves the loop cycled
	int32 HeadingIndex = 0;          // BoundaryProbe's 8-point compass sweep
	bool bASCBound = false;          // AbilityActivatedCallbacks bound once per run

	FBox ArenaHull = FBox(ForceInit);            // PlayerStart hull; the "intended space"
	TArray<TWeakObjectPtr<AActor>> Anchors;       // PlayerStarts, for NearestAnchor naming
	FDateTime StartedUtc;
	double StartedSeconds = 0.0;
	float PlannedDuration = 180.f;

	// probe sample memory
	FVector LastSampleLocation = FVector::ZeroVector;
	bool bHasLastSample = false;
	double BelowKillZSince = -1.0;   // <0 = not currently below
	double StillSince = -1.0;        // <0 = not currently still-while-commanded

	// stats — the report proves the agent actually exercised the game
	int32 Presses = 0, MoveRequests = 0, Deaths = 0;
	bool bWasAlive = true;
	double TravelledUU = 0.0;
	TSet<FString> MatchStatesSeen;

	TArray<FBNAQAFinding> Findings;
	TMap<FString, int32> FindingIndexByKey;      // dedup: error_type + 2000uu grid cell
};
