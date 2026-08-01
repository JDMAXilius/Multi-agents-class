// Breachpoint. The replicated match view: phase, clock deadline, team scores, killfeed.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "GameplayTagContainer.h"

#include "BRGameState.generated.h"

class APlayerState;

/**
 * BRGameState — ARCHITECTURE §3.6, ticket BP04 step 1.
 *
 * THE TRUST BOUNDARY THIS FILE MOVES: this is the entire client-visible surface of the
 * match. Everything here is written by ABRGameMode on the server and READ by clients.
 * There is deliberately NO Server RPC on this class and no client-settable state: a
 * client cannot ask the GameState for anything, so there is nothing here to forge.
 * If a later packet wants a client to influence match state, the RPC belongs on the
 * PlayerController (owning connection, per-player rate limiting) and must call a
 * server-side, authority-gated entry point on ABRGameMode — never a setter here.
 *
 * THE CLOCK (netcode.md law 4, and the reason this ticket exists):
 *   ONE replicated float, `MatchEndServerTime`, is the deadline of the CURRENT phase in
 *   server world time. Clients render the countdown locally from
 *   `MatchEndServerTime - GetServerWorldTimeSeconds()`. A ticking "SecondsRemaining" int
 *   would be a property change every second on an always-relevant actor for every
 *   connection, and it would still be wrong between updates. Do not add one. Ever.
 *
 * NO TICK (law 4): nothing on this class ticks. Phase, scores and killfeed change only
 * when the server pushes them; the HUD reads the clock in its own paint pass.
 *
 * COSMETIC ONLY IN OnRep_ (netcode.md law 3): every OnRep_ body below does exactly two
 * things — refresh a local cache and broadcast a native delegate for UI. Deleting all of
 * them would change what the player SEES and nothing about what the match IS.
 */

/** Team identifiers. 4v4 arena => two teams; `InvalidTeamId` is "no team / spectator". */
namespace BRMatch
{
	inline constexpr uint8 NumTeams = 2;
	inline constexpr uint8 InvalidTeamId = 255;

	/** Killfeed depth. A ring, not a log — bandwidth is a budget (netcode.md law 4). */
	inline constexpr int32 KillFeedCapacity = 8;
}

/**
 * The match phase machine's states (BP04 step 1). Server-owned; advanced ONLY by
 * ABRGameMode::EnterPhase, which runs on timers and gameplay events (never Tick).
 */
UENUM()
enum class EBRMatchPhase : uint8
{
	/** Pre-match: the GameMode has not started the match yet (engine MatchState WaitingToStart). */
	None		UMETA(DisplayName = "None"),
	/** Players in, weapons hot, NOTHING scores. Advanced by the warmup timer. */
	WarmUp		UMETA(DisplayName = "WarmUp"),
	/** Scoring. Advanced by the score limit (event) or the match clock (timer). */
	Live		UMETA(DisplayName = "Live"),
	/** Overtime, entered only on a tied clock expiry. Advanced by any score change or the 60 s cap. */
	SuddenDeath	UMETA(DisplayName = "SuddenDeath"),
	/** Decided. No scoring, no respawns. Advanced by the post-match timer to teardown. */
	PostMatch	UMETA(DisplayName = "PostMatch")
};

/**
 * One killfeed row. Deliberately tiny: two NetGUID actor refs, two bytes of team, one
 * cause tag, a sequence number and a timestamp — ~16 bytes on the wire per row.
 *
 * Names are NOT replicated: the client reads them off the replicated APlayerState, which
 * is always relevant. Law 7 (join/travel honesty): a PlayerState reference can arrive
 * null on a late joiner or after a leaver's PlayerState is destroyed — consumers MUST
 * null-check and fall back to a neutral label. It is never a gameplay outcome, only a row
 * of text, so a null there is a cosmetic miss and not a divergence.
 */
USTRUCT()
struct FBRKillFeedEntry
{
	GENERATED_BODY()

	/** Credited killer. Null for a self / world / environment kill. */
	UPROPERTY()
	TObjectPtr<APlayerState> Killer = nullptr;

	/** Who died. Always valid when the server pushed the row. */
	UPROPERTY()
	TObjectPtr<APlayerState> Victim = nullptr;

	/** Team of the killer at the moment of the kill (teams can change between matches). */
	UPROPERTY()
	uint8 KillerTeamId = BRMatch::InvalidTeamId;

	/** Team of the victim at the moment of the kill. */
	UPROPERTY()
	uint8 VictimTeamId = BRMatch::InvalidTeamId;

	/** `Damage.*` tag that finished them, for the icon. Empty for a world kill. */
	UPROPERTY()
	FGameplayTag CauseTag;

	/** Server world time of the kill. Lets the HUD expire rows locally with no extra traffic. */
	UPROPERTY()
	float ServerTime = 0.f;

	/** Monotonic, server-assigned. The client's ONLY dedupe key across array RepNotifies. */
	UPROPERTY()
	int32 Sequence = 0;

	/** True when the victim killed themselves or the world did it (the -1 self rule). */
	UPROPERTY()
	uint8 bSelfInflicted : 1;

	/** True when killer and victim shared a team. */
	UPROPERTY()
	uint8 bFriendlyFire : 1;

	FBRKillFeedEntry()
		: bSelfInflicted(0)
		, bFriendlyFire(0)
	{
	}
};

/** Phase changed (cosmetic/UI). Fires on server AND clients. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRMatchPhaseChangedSignature, EBRMatchPhase /*OldPhase*/, EBRMatchPhase /*NewPhase*/);
/** A team's score changed (cosmetic/UI). Fires on server AND clients. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRTeamScoreChangedSignature, uint8 /*TeamId*/, int32 /*NewScore*/);
/** A killfeed row arrived (cosmetic/UI). Fires once per row, in order, on server AND clients. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRKillFeedEntryAddedSignature, const FBRKillFeedEntry& /*Entry*/);
/** The phase deadline moved (cosmetic/UI — the HUD may want to re-anchor an animation). */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRMatchClockChangedSignature, float /*NewMatchEndServerTime*/);

UCLASS()
class BREACHPOINT_API ABRGameState : public AGameState
{
	GENERATED_BODY()

public:
	ABRGameState();

	//~ AActor
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor

	// ---------------------------------------------------------------------
	// READ SIDE — safe on every machine, on server and client alike.
	// ---------------------------------------------------------------------

	EBRMatchPhase GetMatchPhase() const { return MatchPhase; }

	/** True while kills move the scoreboard. The single authority on "does this count?". */
	bool IsScoringOpen() const { return MatchPhase == EBRMatchPhase::Live || MatchPhase == EBRMatchPhase::SuddenDeath; }

	/** Absolute server-world-time deadline of the CURRENT phase. 0 == this phase has no clock. */
	float GetMatchEndServerTime() const { return MatchEndServerTime; }

	bool IsClockRunning() const { return MatchEndServerTime > 0.f; }

	/**
	 * The whole point of the one-float clock: every machine derives the same countdown
	 * locally from replicated server time. Never replicate the result of this function.
	 */
	float GetSecondsRemaining() const;

	int32 GetTeamScore(uint8 TeamId) const;

	int32 GetScoreLimit() const { return ScoreLimit; }

	/** `BRMatch::InvalidTeamId` until the match is decided; a draw also stays invalid. */
	uint8 GetWinningTeamId() const { return WinningTeamId; }

	const TArray<FBRKillFeedEntry>& GetKillFeed() const { return KillFeed; }

	// ---------------------------------------------------------------------
	// COSMETIC DELEGATES — UI/VM subscribe here. No gameplay logic may live on these.
	// ---------------------------------------------------------------------

	FBRMatchPhaseChangedSignature OnMatchPhaseChanged;
	FBRTeamScoreChangedSignature OnTeamScoreChanged;
	FBRKillFeedEntryAddedSignature OnKillFeedEntryAdded;
	FBRMatchClockChangedSignature OnMatchClockChanged;

	// ---------------------------------------------------------------------
	// WRITE SIDE — SERVER ONLY. Every one of these early-outs on !HasAuthority().
	// ABRGameMode is the only intended caller. There is no client path in.
	// ---------------------------------------------------------------------

	/** Server only. Sets the phase, mirrors the notify locally, forces an immediate net update. */
	void ServerSetMatchPhase(EBRMatchPhase NewPhase);

	/** Server only. `NewEndServerTime` is absolute server world time; 0 clears the clock. */
	void ServerSetMatchEndServerTime(float NewEndServerTime);

	/** Server only. Adds `Delta` to a team, clamped at >= 0. Returns the resulting score. */
	int32 ServerAddTeamScore(uint8 TeamId, int32 Delta);

	/** Server only. One-time match parameter; refused once scoring has opened. */
	void ServerSetScoreLimit(int32 NewScoreLimit);

	/** Server only. Records the decided winner (or `InvalidTeamId` for a draw). */
	void ServerSetWinningTeamId(uint8 TeamId);

	/**
	 * Server only. Pushes one row, dropping the oldest past `BRMatch::KillFeedCapacity`.
	 * Stamps `Sequence` and `ServerTime` itself — callers do not get to choose either,
	 * which is what keeps client-side dedupe honest.
	 */
	void ServerPushKillFeedEntry(FBRKillFeedEntry Entry);

protected:
	// ---------------------------------------------------------------------
	// REPLICATED STATE. Condition rationale, property by property:
	//
	//  MatchPhase          COND_None  every client draws the phase banner and gates its
	//                                 own HUD on it; no client-private component.
	//  MatchEndServerTime  COND_None  the shared countdown. One float, changes ~4x/match.
	//  TeamScores          COND_None  the scoreboard is public by design in a team arena.
	//  ScoreLimit          COND_None  needed for "17 / 25"; set once, before Live.
	//  WinningTeamId       COND_None  the end card; set once.
	//  KillFeed            COND_None  a public feed — every client shows every kill.
	//
	// Nothing here is per-player private, so nothing here earns COND_OwnerOnly; the
	// private surface (loadout, ammo, K/D internals) lives on PlayerState and is that
	// owner's condition problem, not this class's. What keeps the GameState cheap is not
	// a condition but the SHAPE: no ticking value, a bounded ring, and event-driven
	// ForceNetUpdate instead of a raised NetUpdateFrequency.
	// ---------------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase)
	EBRMatchPhase MatchPhase = EBRMatchPhase::None;

	UPROPERTY(ReplicatedUsing = OnRep_MatchEndServerTime)
	float MatchEndServerTime = 0.f;

	/** Sized to `BRMatch::NumTeams` in the constructor and never resized. */
	UPROPERTY(ReplicatedUsing = OnRep_TeamScores)
	TArray<int32> TeamScores;

	UPROPERTY(Replicated)
	int32 ScoreLimit = 0;

	UPROPERTY(Replicated)
	uint8 WinningTeamId = BRMatch::InvalidTeamId;

	UPROPERTY(ReplicatedUsing = OnRep_KillFeed)
	TArray<FBRKillFeedEntry> KillFeed;

	// ---------------------------------------------------------------------
	// RepNotifies — COSMETIC ONLY. Remove every body here and the match still plays
	// out identically on the server; only the HUD goes quiet. That is the test.
	// ---------------------------------------------------------------------

	UFUNCTION()
	void OnRep_MatchPhase(EBRMatchPhase OldPhase);

	UFUNCTION()
	void OnRep_MatchEndServerTime();

	UFUNCTION()
	void OnRep_TeamScores(const TArray<int32>& OldScores);

	UFUNCTION()
	void OnRep_KillFeed();

private:
	/** Server-side row counter. Not replicated — it travels inside each entry. */
	int32 NextKillFeedSequence = 1;

	/** Client-side high-water mark, so an array RepNotify never re-announces old rows. */
	int32 LastAnnouncedKillFeedSequence = 0;
};
