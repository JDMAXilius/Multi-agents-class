// Breachpoint. The server-only match spine: phase machine, kill attribution, scored respawn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameplayTagContainer.h"
#include "Match/BRGameState.h"

#include "BRGameMode.generated.h"

class AActor;
class AController;
class APlayerState;
class UAbilitySystemComponent;
struct FGameplayEventData;

/**
 * BRGameMode — ARCHITECTURE §3.6, ticket BP04 step 2.
 *
 * TRUST BOUNDARY: the GameMode exists ONLY on the server. It is never replicated and no
 * client ever holds a pointer to it, which is exactly why the match rules live here and
 * not on the PlayerController or the character. Every public entry point below still
 * opens with an authority gate — the gate is the documentation and the guard, and it is
 * what makes a future "just call it from the pawn" refactor fail loudly instead of
 * quietly handing a client the scoreboard.
 *
 * NO GAMEPLAY TICK (law 4). The phase machine is four timers and two gameplay events:
 *
 *   None --HandleMatchHasStarted (engine event)--> WarmUp
 *   WarmUp --WarmupTimerHandle (timer)--> Live
 *   Live --score limit reached (Event.Death -> deferred win check)--> PostMatch
 *   Live --MatchClockTimerHandle (timer)--> PostMatch if a team leads, else SuddenDeath
 *   SuddenDeath --any lead (Event.Death -> deferred win check)--> PostMatch
 *   SuddenDeath --SuddenDeathTimerHandle (timer, 60 s cap)--> PostMatch by damage tiebreak
 *   PostMatch --PostMatchTimerHandle (timer)--> RequestMatchTeardown()
 *
 * `PrimaryActorTick.bCanEverTick` is false and there is no Tick override. If a future
 * packet needs "every N seconds", it needs a timer, not a tick.
 *
 * THE DECIDED EDGE CASES (BP04's whole point — these are rulings, not discoveries):
 *
 *   Double-KO trade:  BOTH kills count. Attribution NEVER asks whether the killer is
 *                     still alive, and the win check is deferred to the next tick via
 *                     SetTimerForNextTick, so two deaths resolved inside one server frame
 *                     are both credited BEFORE the match can end on either of them.
 *   No instigator:    fall damage, world kill, out-of-bounds, self-detonation, or a
 *                     killer whose last hit fell outside KillCreditWindowSeconds =>
 *                     -1 to the VICTIM's own team (clamped at 0) and an Event.Kill with
 *                     magnitude -1 sent to the victim. Nobody gets a free kill.
 *   Friendly fire:    no credit to the killer; -1 to the KILLER's team (clamped at 0).
 *                     Same shape as the self rule, charged to whoever pulled the trigger.
 *   Kill steal:       the most recent damaging ENEMY inside the credit window takes it.
 *                     "Most damage" does not win an argument with "last hit".
 *   Warmup kills:     no score, no killfeed, no Event.Kill. Respawn still happens.
 *   PostMatch deaths: same as warmup — the scoreboard is frozen the instant a winner exists.
 *   Sudden death:     respawns CONTINUE (it is a 60 s overtime, not last-man-standing).
 *                     Configurable via bAllowRespawnInSuddenDeath, defaulted to true.
 *
 * HANDSHAKES THIS CLASS DEPENDS ON (owned by other packets — see the BP04 Log):
 *   - BP02 sends `Event.Death` through the victim's PlayerState ASC when GE_Death applies.
 *     This class binds that ASC's GenericGameplayEventCallbacks; it never polls for death.
 *   - The combat pipeline calls NotifyDamageDealt() server-side on each applied damage
 *     execution. Without it, attribution falls back to the death event's Instigator and
 *     the sudden-death damage tiebreak reads zero.
 *   - K/D bookkeeping lives on the PlayerState: this class AWARDS by sending `Event.Kill`
 *     (magnitude +1 credit, -1 penalty) to the credited player's ASC. It deliberately
 *     does not reach into PlayerState fields.
 */
UCLASS()
class BREACHPOINT_API ABRGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ABRGameMode();

	//~ AGameModeBase / AGameMode
	virtual void InitGameState() override;
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleMatchHasStarted() override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	//~ End AGameModeBase / AGameMode

	// ---------------------------------------------------------------------
	// SERVER-SIDE ENTRY POINTS. All authority-gated. None of these is an RPC and none
	// may ever become one: a client that wants something asks its PlayerController,
	// whose Server RPC validates and then calls in here.
	// ---------------------------------------------------------------------

	/**
	 * The combat pipeline's report of applied damage. Server only.
	 * Feeds two things: the per-victim credit ledger (who killed whom) and the per-team
	 * damage total (the sudden-death tiebreak). Rejects non-positive, absurd, and
	 * self-damage amounts, and only counts enemy damage toward the tiebreak.
	 */
	void NotifyDamageDealt(APlayerState* Attacker, APlayerState* Victim, float Amount, FGameplayTag DamageTag);

	/**
	 * The `Event.Death` handler. Server only, and public so the Sim.Match spec can drive
	 * the attribution table directly without a live ASC.
	 */
	void HandleDeathEvent(APlayerState* Victim, APlayerState* EventInstigator, FGameplayTag CauseTag);

	/**
	 * Registers a combatant's ASC for death events. Called automatically for human
	 * players in OnPostLogin; bots (whose AIControllers never OnPostLogin) must be
	 * registered by the bot manager. Idempotent.
	 */
	void RegisterCombatant(AController* Combatant);

	/** Unbinds and drops all per-player server state. Idempotent. */
	void UnregisterCombatant(AController* Combatant);

	/**
	 * The server-side half of a respawn request. Server only. Returns true if a respawn
	 * happened. This is what a PlayerController's ServerRequestRespawn_Validate/Implementation
	 * must call — it re-checks EVERYTHING (dead, phase, earliest-allowed time), so a
	 * forged or spammed request buys nothing: an early request is refused, not queued,
	 * and a repeated one cannot shorten the timer.
	 */
	bool RequestRespawn(AController* Requester);

	/**
	 * Loads the tuning numbers. Server only, refused once scoring has opened.
	 * Every value is clamped to a sane band — a corrupt CSV row may not produce a
	 * zero-second match or a negative respawn delay.
	 */
	void ApplyMatchRules(int32 InScoreLimit, float InMatchSeconds, float InSuddenDeathSeconds,
		float InWarmupSeconds, float InRespawnSeconds, float InKillCreditWindowSeconds);

	/** Server-side telemetry/spec hook. Killer is null for a self/world kill. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FBRPlayerKilledSignature, APlayerState* /*Killer*/, APlayerState* /*Victim*/, const TArray<APlayerState*>& /*Assists*/);
	FBRPlayerKilledSignature OnPlayerKilled;

	/** Reads through to the GameState so callers do not need both pointers. */
	EBRMatchPhase GetMatchPhase() const;

protected:
	// ---------------------------------------------------------------------
	// Tuning. Defaults mirror the ticket (25 / 8:00 / 60 s SD / 5 s respawn); the real
	// values arrive from DT_MatchRules through ApplyMatchRules() once BRDataRows.h has
	// FBRMatchRulesRow. These are the FALLBACK, not the source of truth (law 3).
	// ---------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	int32 ScoreLimit = 25;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float MatchDurationSeconds = 480.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float SuddenDeathSeconds = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float WarmupSeconds = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float PostMatchSeconds = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float RespawnDelaySeconds = 5.f;

	/** A hit older than this no longer earns the kill. The "last damaging instigator <= 5 s" rule. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float KillCreditWindowSeconds = 5.f;

	/** Sudden death is overtime, not elimination. See the header's decided-edge-case block. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	bool bAllowRespawnInSuddenDeath = true;

	/** A team kill costs the killer's team a point. Set false for a no-penalty playtest. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	bool bFriendlyFirePenalizesKillersTeam = true;

	/** Anything above this in one damage report is a bug or an exploit; it is dropped and logged. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float MaxPlausibleSingleDamage = 10000.f;

	// ---------------------------------------------------------------------
	// Phase machine
	// ---------------------------------------------------------------------

	/** The ONE way the phase changes. Authority-gated; clears the outgoing phase's timer. */
	void EnterPhase(EBRMatchPhase NewPhase);

	void HandleWarmupElapsed();
	void HandleMatchClockExpired();
	void HandleSuddenDeathExpired();
	void HandlePostMatchElapsed();

	/**
	 * Queues the win check for the NEXT tick instead of running it inline. This single
	 * line is what makes a double-KO at match point credit both players: every death
	 * resolved in the current frame is scored first, then the check runs once.
	 */
	void ScheduleWinCheck();
	void EvaluateWinConditions();

	/** Ends the match on `WinningTeamId` (`BRMatch::InvalidTeamId` == draw). */
	void EnterPostMatch(uint8 WinningTeamId);

	/**
	 * Post-match teardown seam. Default logs only — session travel belongs to Online/
	 * behind IBRServerLifecycle, not to the match frame.
	 */
	virtual void RequestMatchTeardown();

	// ---------------------------------------------------------------------
	// Scoring / attribution
	// ---------------------------------------------------------------------

	/** The most recent enemy to damage `Victim` inside the credit window, else null. */
	APlayerState* ResolveKiller(APlayerState* Victim, APlayerState* EventInstigator) const;

	/** Everyone except the killer who damaged the victim inside the window, killer's team first. */
	void CollectAssists(APlayerState* Victim, APlayerState* Killer, TArray<APlayerState*>& OutAssists) const;

	/** Sends `Event.Kill` (magnitude +1 credit / -1 penalty) to `Recipient`'s ASC. */
	void SendKillEvent(APlayerState* Recipient, APlayerState* Victim, float Magnitude, FGameplayTag CauseTag) const;

	/**
	 * The team lookup seam. Default resolves through IGenericTeamAgentInterface on the
	 * PlayerState and then its controller. BRPlayerState (BP02) owns the real TeamID; the
	 * one-line override lands with the BP04 follow-up rather than forcing this file to
	 * include a header another packet is writing right now.
	 */
	virtual uint8 ResolveTeamId(const APlayerState* Player) const;

	// ---------------------------------------------------------------------
	// Respawn
	// ---------------------------------------------------------------------

	void StartRespawnTimer(AController* Victim);
	void HandleRespawnTimerElapsed(TWeakObjectPtr<AController> WeakVictim);
	bool CanRespawnNow(AController* Candidate) const;

	/**
	 * Spawn-point candidates. Default = every APlayerStart in the level. Override when
	 * the arena manifest lands so scored respawn reads the authored vocabulary instead.
	 */
	virtual void GatherSpawnCandidates(TArray<AActor*>& OutCandidates) const;

	/** Farthest-from-threat: distance to the nearest living enemy, penalised for line of sight and reuse. */
	float ScoreSpawnCandidate(const AActor* Candidate, AController* ForPlayer) const;

	// ---------------------------------------------------------------------
	// Internals
	// ---------------------------------------------------------------------

	/** One damage report, kept only as long as the credit window needs it. */
	struct FBRDamageRecord
	{
		TWeakObjectPtr<APlayerState> Attacker;
		float Amount = 0.f;
		float ServerTime = 0.f;
		FGameplayTag DamageTag;
	};

	/** ASC callback shim; forwards to HandleDeathEvent after resolving the PlayerStates. */
	void OnDeathGameplayEvent(const FGameplayEventData* Payload);

	/** APawn / AController / APlayerState -> APlayerState. Null-safe. */
	static APlayerState* ResolvePlayerState(const AActor* Actor);

	void PruneDamageLedger(APlayerState* Victim);

	ABRGameState* GetBRGameState() const;

	float GetServerTime() const;

	/** Per-victim recent damage, newest last. Cleared when that victim dies. */
	TMap<TWeakObjectPtr<APlayerState>, TArray<FBRDamageRecord>> DamageLedger;

	/** Per-team damage dealt to ENEMIES while scoring was open — the sudden-death tiebreak. */
	TArray<float> TeamDamageDealt;

	/** Players whose death has been processed and who are awaiting respawn. The dedupe set. */
	TSet<TWeakObjectPtr<APlayerState>> PendingRespawnPlayers;

	/** Earliest server time each controller may respawn. A request before it is refused. */
	TMap<TWeakObjectPtr<AController>, float> EarliestRespawnServerTime;

	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;

	/** ASCs we hold an Event.Death binding on, so unbinding is exact and idempotent. */
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle> DeathListenerHandles;

	/** Recently used spawn points -> server time, so two players do not spawn on top of each other. */
	TMap<TWeakObjectPtr<AActor>, float> SpawnPointLastUsed;

	FTimerHandle WarmupTimerHandle;
	FTimerHandle MatchClockTimerHandle;
	FTimerHandle SuddenDeathTimerHandle;
	FTimerHandle PostMatchTimerHandle;

	/**
	 * Guards the deferred win check. SetTimerForNextTick returns no handle, so this bool
	 * is the "already queued" latch — many deaths in one frame queue exactly one check.
	 */
	bool bWinCheckPending = false;

	/** Latches the moment a winner exists, so a trailing death cannot re-decide the match. */
	bool bMatchDecided = false;
};
