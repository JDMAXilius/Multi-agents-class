// Breachpoint. Match telemetry collection: events, not opinions. Authority-only, no PII, no tick.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "Online/BRServerLifecycle.h"

#include "BRTelemetrySubsystem.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class APlayerState;
enum class EBRMatchPhase : uint8;
struct FBRKillFeedEntry;

/**
 * BRTelemetrySubsystem — ARCHITECTURE §3.10 unit 1, ticket BP11 step 3 (collection half).
 *
 * THE JOB, in one line: **fold the delegates the game already broadcasts into one match record,
 * on the server, for free.** It measures; it never decides. Nothing in this file may change a
 * gameplay outcome, and deleting the whole folder must leave the match bit-identical.
 *
 * THREE CONSTRAINTS, and they are the whole design:
 *
 *  1. **Authority only.** Every entry point opens with an authority gate. A client that folded
 *     its own telemetry would be reporting a prediction, and a client that could WRITE telemetry
 *     would be a leaderboard exploit the moment GL-5 lands (`BREACHPOINT-GAMELIFT-PLAN.md` §6:
 *     leaderboards read server-written results ONLY). There is no client path in.
 *
 *  2. **No PII, ever.** Players appear as `PlayerKey` — a small integer assigned per match, in
 *     join order, meaningless outside this record. Steam ids, persona names, IP addresses and
 *     join credentials are never copied here, never logged here, and never reach a consumer.
 *     This is the services trust law read forwards: the platform proved identity to the login
 *     handshake, and that is where identity stops. `FBRJoinTarget::DisplayLabel` in particular is
 *     PII-adjacent and is deliberately absent from every struct below.
 *
 *  3. **Cheap enough to leave on.** No tick, no allocation per event beyond one array slot, a
 *     BOUNDED ring (`MaxRetainedEvents`, oldest dropped with a counted loss so the record is never
 *     silently partial), and not one poll: every source is a delegate the game already fires.
 *
 * WHERE THE DATA GOES: nowhere, yet. This subsystem's output is a delegate
 * (`OnMatchTelemetryFinalized`) plus a getter. The Spotter (`BRSpotterSubsystem`, ai-builder)
 * subscribes for coach lines; the tuning-curator and QA read the same record; GL-5's DynamoDB
 * writer will subscribe later. Writing a file or opening a socket from HERE would make telemetry
 * a dependency of the match, which is exactly what constraint 3 forbids.
 *
 * THE ONE FIELD THAT IS LOAD-BEARING RIGHT NOW: `FBRMatchTelemetryRecord::Outcome`. Ruling R16
 * gates every dollar of GameLift spend on demand telemetry — "host-quit match-abandonment rate,
 * NAT/join failure rate" — so `AbandonedByHost` vs `Completed` is not a nice-to-have statistic,
 * it is the trigger the Phase-2 decision reads. It is stamped from `IBRServerLifecycle::
 * OnHostingEnding`, synchronously, while the world still exists.
 */

// =============================================================================================
// Record shapes
// =============================================================================================

/** How a match ended, from the server's point of view. The R16 trigger reads this. */
UENUM()
enum class EBRMatchTelemetryOutcome : uint8
{
	/** Still running (or the record was read early). */
	InProgress,
	/** Ran to a decided end. */
	Completed,
	/** The host quit while the match was still live. THE abandonment datum. */
	AbandonedByHost,
	/** The host or its network died mid-match. Abandonment's involuntary twin. */
	HostLost,
	/** The hosting platform ended us (Phase 2: scale-down, spot interruption). */
	PlatformEnded,
	/** Ended by a server fault. */
	Fault
};

/**
 * One telemetry event. Deliberately fixed-width and flat — a `TMap<FName,double>` per event
 * would be tidier to read and would cost an allocation per kill (constraint 3).
 *
 * `EventId` is a plain FName, NOT a GameplayTag: telemetry ids are not gameplay state, and
 * routing them through the tag registry would make adding a metric a Core/ edit.
 */
USTRUCT()
struct FBRTelemetryEvent
{
	GENERATED_BODY()

	/** e.g. "Kill", "Death", "Damage", "Join", "Leave", "PhaseChanged", "HostingEnding". */
	UPROPERTY()
	FName EventId;

	/** Server world time. The only clock in this file. */
	UPROPERTY()
	float ServerTime = 0.f;

	/** Pseudonymous per-match key of the acting player; 0 = nobody/the world. NEVER a platform id. */
	UPROPERTY()
	int32 SubjectKey = 0;

	/** Pseudonymous key of the acted-upon player; 0 = none. */
	UPROPERTY()
	int32 ObjectKey = 0;

	/** Magnitude: damage, count, seconds — whatever the EventId documents. */
	UPROPERTY()
	float Value = 0.f;

	/** Qualifier: a `Damage.*` tag name, a weapon row name, a phase name. Never free text. */
	UPROPERTY()
	FName Detail;
};

/**
 * Per-player, per-match folded totals — GDD Appendix C's schema, restricted to what this packet
 * can source honestly today. The rest of Appendix C (accuracy per weapon, TTK distribution,
 * shield-break conversion, fights lost below 40% shields, grapple/rocket counters) arrives
 * through `RecordEvent` from the packets that own those pipelines: this subsystem will not
 * invent a number it cannot observe.
 */
USTRUCT()
struct FBRPlayerMatchTelemetry
{
	GENERATED_BODY()

	/** Pseudonymous per-match key. Stable within the record, meaningless outside it. */
	UPROPERTY()
	int32 PlayerKey = 0;

	UPROPERTY()
	uint8 TeamId = 255;

	/** True for a bot. Bots are folded so human-vs-bot rows are separable, never merged. */
	UPROPERTY()
	bool bIsBot = false;

	/** True when this player arrived after the match went Live — the join-in-progress cohort. */
	UPROPERTY()
	bool bJoinedInProgress = false;

	UPROPERTY()
	int32 Kills = 0;

	UPROPERTY()
	int32 Deaths = 0;

	UPROPERTY()
	int32 Assists = 0;

	/** Kills charged against the player (self/world deaths and friendly fire), per BP04's rules. */
	UPROPERTY()
	int32 SelfInflictedDeaths = 0;

	UPROPERTY()
	int32 FriendlyFireKills = 0;

	/** Seconds this player was connected while the match was scoring. */
	UPROPERTY()
	float TimeInMatchSeconds = 0.f;

	/** True when the player's participation ended before the match did. */
	UPROPERTY()
	bool bLeftEarly = false;
};

/** The whole match, in one struct. This is what a consumer receives. */
USTRUCT()
struct FBRMatchTelemetryRecord
{
	GENERATED_BODY()

	/** Random per match. Correlates events across systems without correlating PLAYERS across matches. */
	UPROPERTY()
	FGuid MatchId;

	UPROPERTY()
	FName MapName;

	UPROPERTY()
	EBRMatchTelemetryOutcome Outcome = EBRMatchTelemetryOutcome::InProgress;

	/** Seconds from the first Live phase to the end. 0 when the match never went Live. */
	UPROPERTY()
	float MatchDurationSeconds = 0.f;

	/** Seconds of Warmup actually spent. Feeds the "how long until a match starts" question. */
	UPROPERTY()
	float WarmupDurationSeconds = 0.f;

	UPROPERTY()
	uint8 WinningTeamId = 255;

	/** Humans at the end. Bots are counted separately so backfill never inflates a player count. */
	UPROPERTY()
	int32 HumanPlayerCount = 0;

	UPROPERTY()
	int32 BotPlayerCount = 0;

	/** Joins that landed after the match went Live. The join-in-progress rate. */
	UPROPERTY()
	int32 JoinInProgressCount = 0;

	/** Players who left before the end. With `Outcome`, this is the abandonment picture (R16). */
	UPROPERTY()
	int32 EarlyDepartureCount = 0;

	UPROPERTY()
	TArray<FBRPlayerMatchTelemetry> Players;

	/** The bounded event ring, oldest first. */
	UPROPERTY()
	TArray<FBRTelemetryEvent> Events;

	/** Events the ring could not keep. Non-zero means this record is partial — say so, never hide it. */
	UPROPERTY()
	int32 DroppedEventCount = 0;
};

/** The record is closed and will not change again. The ONLY output of this subsystem. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRMatchTelemetryFinalizedSignature, const FBRMatchTelemetryRecord& /*Record*/);

/** One event was folded. For live consumers (a debug overlay, a future streaming writer). */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRTelemetryEventRecordedSignature, const FBRTelemetryEvent& /*Event*/);

UCLASS(Config = Game)
class BREACHPOINT_API UBRTelemetrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem / UWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~ End USubsystem / UWorldSubsystem

	// =========================================================================================
	// Ingestion. Server only — every one of these is a no-op on a client.
	// =========================================================================================

	/**
	 * Fold one event. The general entry point for packets that own a pipeline this subsystem
	 * cannot observe (accuracy, TTK, shield-break conversion — GDD Appendix C's remainder).
	 * Cheap: one array slot, no allocation, no string work.
	 */
	void RecordEvent(const FBRTelemetryEvent& Event);

	/**
	 * Convenience over RecordEvent that resolves PlayerStates to pseudonymous keys for you, so a
	 * caller never has to hold — or be tempted to log — an identity.
	 */
	void RecordPlayerEvent(FName EventId, const APlayerState* Subject, const APlayerState* Object,
		float Value, FName Detail);

	/**
	 * The per-match key for a player. Assigned on first sight, in join order, starting at 1.
	 * Returns 0 for null. This function is the ONLY place a PlayerState becomes a telemetry
	 * identity, and it is why no platform id appears anywhere else in this file.
	 */
	int32 GetPlayerKey(const APlayerState* Player);

	// =========================================================================================
	// Output
	// =========================================================================================

	/** The live record. `Outcome` is `InProgress` until the match ends. */
	const FBRMatchTelemetryRecord& GetMatchRecord() const { return Record; }

	/** True once the record is closed. A consumer that arrives late reads GetMatchRecord(). */
	bool IsFinalized() const { return bFinalized; }

	FBRMatchTelemetryFinalizedSignature OnMatchTelemetryFinalized;
	FBRTelemetryEventRecordedSignature OnTelemetryEventRecorded;

protected:
	/**
	 * Ring capacity. A 4v4 arena match produces a few hundred events; the default leaves room for
	 * the packets that will push accuracy and TTK into `RecordEvent` later. When it fills, the
	 * OLDEST event is dropped and `DroppedEventCount` increments — a partial record that says so
	 * beats a complete record that costs a reallocation storm mid-fight.
	 */
	UPROPERTY(Config)
	int32 MaxRetainedEvents = 2048;

	/** Master switch. False makes every entry point an immediate return. */
	UPROPERTY(Config)
	bool bTelemetryEnabled = true;

private:
	/** True only where telemetry may run: a game world with authority. */
	bool HasTelemetryAuthority() const;

	/** Idempotent. Binds the match sources once the GameMode/GameState actually exist. */
	void TryBindMatchSources();

	/** Idempotent. Binds the lifecycle seam's ending event — the R16 abandonment stamp. */
	void TryBindServerLifecycle();

	void HandlePlayerKilled(APlayerState* Killer, APlayerState* Victim, const TArray<APlayerState*>& Assists);
	void HandleMatchPhaseChanged(EBRMatchPhase OldPhase, EBRMatchPhase NewPhase);
	void HandleKillFeedEntryAdded(const FBRKillFeedEntry& Entry);
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	void HandleHostingEnding(const FBRHostingEndNotice& Notice);

	/** Finds or creates the per-player row. Never returns null for a valid PlayerState. */
	FBRPlayerMatchTelemetry* FindOrAddPlayerRow(const APlayerState* Player);

	FBRPlayerMatchTelemetry* FindPlayerRowByKey(int32 PlayerKey);

	/** Closes the record and broadcasts it. Idempotent — the first close wins. */
	void FinalizeRecord(EBRMatchTelemetryOutcome Outcome);

	float GetServerTime() const;

	FBRMatchTelemetryRecord Record;

	/**
	 * PlayerState -> pseudonymous key. FObjectKey, not a pointer: it neither keeps the PlayerState
	 * alive nor goes stale, so a player who leaves mid-match keeps their key and their row instead
	 * of silently becoming a second player when someone reuses the address.
	 */
	TMap<FObjectKey, int32> PlayerKeys;

	int32 NextPlayerKey = 1;

	bool bFinalized = false;
	bool bMatchSourcesBound = false;
	bool bLifecycleBound = false;

	/** Server time the match entered Live / left WarmUp. 0 when it has not yet. */
	float LiveStartServerTime = 0.f;
	float WarmupStartServerTime = 0.f;

	/** True while the match is scoring — the window in which a host quit counts as abandonment. */
	bool bMatchIsLive = false;

	FDelegateHandle PlayerKilledHandle;
	FDelegateHandle PhaseChangedHandle;
	FDelegateHandle KillFeedHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	FDelegateHandle HostingEndingHandle;

	/** The lifecycle we bound to, so the unbind in Deinitialize is exact. */
	UPROPERTY()
	TScriptInterface<IBRServerLifecycle> BoundLifecycle;
};
