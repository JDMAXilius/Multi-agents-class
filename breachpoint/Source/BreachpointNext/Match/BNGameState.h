#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "BNGameState.generated.h"

class ABNPlayerState;

/** One kill, as the SERVER decided it. Names ride the entry as strings written at push time —
 *  the PlayerState refs are for self-identification and may legitimately null (a leaver's
 *  PlayerState dies under the ref; the OnRep_Winner lesson), but the LINE must survive the
 *  leaver, so the render truth is the strings. Killer null = world damage; killer == victim =
 *  their own grenade — the same three cases the kill log prints. */
USTRUCT()
struct FBNKillfeedRingEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ABNPlayerState> Victim;

	UPROPERTY()
	TObjectPtr<ABNPlayerState> Killer;

	UPROPERTY()
	FString VictimName;

	UPROPERTY()
	FString KillerName;

	/** R7.3 — WHAT did it: a weapon ROW NAME the UI resolves for a display name and a glyph, or
	 *  one of BNDamageSource's rowless causes (Melee, Grenade), or None. A name, not a weapon
	 *  reference: the weapon that fired is routinely destroyed before this line ages out. */
	UPROPERTY()
	FName SourceName;

	/** Server-assigned, monotonic, never reused — the reader's dedupe key: a ring that
	 *  re-replicates hands the client the same entries again, and index positions shift. */
	UPROPERTY()
	int32 Sequence = INDEX_NONE;

	UPROPERTY()
	double ServerTime = 0.0;
};

/** No payload: the ring itself is the data and readers walk it — one broadcast per change beats
 *  a per-entry delegate that a batched replication update would fire out of order. */
DECLARE_MULTICAST_DELEGATE(FBNKillfeedChangedSignature);

/** Fires on EVERY machine: the server from AGameState::SetMatchState (which runs the OnRep by
 *  hand on authority), remote clients from replication. The states are the ENGINE'S OWN
 *  MatchState FNames — WaitingToStart, InProgress, WaitingPostMatch — not a BN enum: the machine
 *  that changes them lives in AGameMode, and the old Breachpoint module's session subsystem
 *  already speaks these exact names through FGameModeEvents.
 *
 *  SUBSCRIBERS MUST ALSO READ GetMatchState() WHEN THEY SUBSCRIBE. On a client joining mid-match
 *  this fires from the GameState channel's INITIAL bunch — before that client's controller or HUD
 *  exists to have subscribed — so a reader that waits only for the delegate never learns the state
 *  it joined into. Subscribe, then read once; every transition after that is the delegate's. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBNMatchStateSignature, FName /*NewState*/);

/** AGameState, not GameStateBase: the parent carries the replicated MatchState FName, its OnRep,
 *  and HasMatchStarted/HasMatchEnded — the native match machine's client half. BN adds only what
 *  the engine doesn't have: the clock stamp, the score limit mirror, and the winner. */
UCLASS()
class BREACHPOINTNEXT_API ABNGameState : public AGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	FBNMatchStateSignature OnMatchStateChanged;

	/** R7 — fires on every machine when the killfeed ring changes: the authority by hand from
	 *  PushKillfeedEntry (OnReps do not run there), clients from OnRep_Killfeed. Subscribers walk
	 *  GetKillfeed() and dedupe by Sequence. */
	FBNKillfeedChangedSignature OnKillfeedChanged;

	double GetMatchEndServerTime() const { return MatchEndServerTime; }
	int32 GetScoreLimit() const { return ScoreLimit; }
	ABNPlayerState* GetWinner() const { return Winner; }
	const TArray<FBNKillfeedRingEntry>& GetKillfeed() const { return Killfeed; }

	/** Authority: append one decided kill. Names resolved HERE, while both parties are alive to
	 *  ask; the ring holds the newest KillfeedCapacity entries and the oldest falls off — the
	 *  drop is by design, a feed is not a match log. */
	void PushKillfeedEntry(ABNPlayerState* Victim, ABNPlayerState* Killer, FName SourceName = NAME_None);

	/** Authority, the restart's: last round's kills are the previous record and must not
	 *  replicate into the new one (critic, R7 W1). The Sequence counter is NOT reset — readers
	 *  dedupe against it, and a counter that restarts re-shows history. */
	void ResetKillfeed();

	/** Seconds until MatchEndServerTime, computed LOCALLY against server time on whatever machine
	 *  asks. Zero when no clock is stamped and never negative. */
	float GetRemainingSeconds() const;

	/** Everyone tied at the top of the kill count. Empty when nobody is playing. */
	void GetLeaders(TArray<ABNPlayerState*>& OutLeaders) const;

	/** Authority only. Neither has an OnRep body of its own that announces — the state machine's
	 *  announcement (OnRep_MatchState below) is the one broadcast readers subscribe to. */
	void SetMatchEndServerTime(double InEndServerTime);
	void SetWinner(ABNPlayerState* InWinner);

	// -- TEAMS (BN15): the replicated record. Summing PlayerArray client-side is
	// tempting-but-wrong — a leaver's PlayerState takes its points out of the sum;
	// these ints are the honest book. ------------------------------------------------
	/** 0 for an out-of-range team — honest-unknown, never a crash. */
	int32 GetTeamScore(uint8 Team) const;
	/** FGenericTeamId::NoTeam::GetId() (255) = no team has won / not a team match. */
	uint8 GetWinningTeamId() const { return WinningTeamId; }

	/** Authority only; fires the OnRep by hand (the SetWinner idiom). AddTeamScore
	 *  broadcasts OnTeamScoreChanged with the team that moved. */
	void AddTeamScore(uint8 Team, int32 Points);
	void SetWinningTeamId(uint8 Team);
	void ResetTeamScores();

	/** Fires on every machine where a team's score changes (authority by hand, clients
	 *  from the OnRep) — the HUD's one subscription; it reads GetTeamScore back. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FBNTeamScoreChangedSignature, uint8 /*Team*/);
	FBNTeamScoreChangedSignature OnTeamScoreChanged;

protected:
	UFUNCTION()
	void OnRep_TeamScores();

	/** Sized NumTeams on the authority's first write; empty in FFA so the surface
	 *  costs nothing while bTeamsEnabled is false. */
	UPROPERTY(ReplicatedUsing = OnRep_TeamScores)
	TArray<int32> TeamScores;

	/** 255 (NoTeam) = undecided / not a team match. */
	UPROPERTY(Replicated)
	uint8 WinningTeamId = 255;
	UFUNCTION()
	void OnRep_Killfeed();

	/** The engine's own notify, on every machine (the server calls it by hand from SetMatchState).
	 *  Super runs the native client handlers (NotifyMatchStarted and friends); BN adds the LogBN
	 *  line and the delegate. ONE body for all machines, so every transition logs and broadcasts
	 *  exactly once. */
	virtual void OnRep_MatchState() override;

	UFUNCTION()
	void OnRep_Winner();

	/** THE END STAMP, not a countdown. A ticking replicated counter is a per-second write to every
	 *  client for the whole match, and a late joiner reads whatever value happened to be in flight.
	 *  One stamp replicates once and is already correct for a client that joins at any moment. */
	UPROPERTY(Replicated)
	double MatchEndServerTime = 0.0;

	/** Mirrored from the mode's config so a client can render "12 / 25" without asking the server. */
	UPROPERTY(Replicated)
	int32 ScoreLimit = 0;

	/** RepNotify'd, and the notify is not decoration: a client joining during PostMatch can open
	 *  the GameState channel before the winner's PlayerState channel, so this arrives as an
	 *  unmapped GUID — null at the moment the state change is announced, which renders a decided
	 *  match as a tie. The GUID resolves a moment later, and without a notify nothing would ever
	 *  say so. */
	UPROPERTY(ReplicatedUsing = OnRep_Winner)
	TObjectPtr<ABNPlayerState> Winner;

	/** The ring. A replicated array, not a multicast RPC, on purpose: a joiner mid-match gets
	 *  the current feed in the initial bunch for free, and an actor ref that arrives as an
	 *  unmapped GUID re-fires the notify when it resolves — an RPC gives neither. Small and
	 *  bounded; scores are the record, this is the last few seconds of it. */
	UPROPERTY(ReplicatedUsing = OnRep_Killfeed)
	TArray<FBNKillfeedRingEntry> Killfeed;

	/** Monotonic, authority-only. Starts at 0 so the first entry is Sequence 1 and INDEX_NONE
	 *  stays the reader's "never seen anything" sentinel. */
	int32 KillfeedSequence = 0;

	/** Fixed by design, not config: the on-screen pool renders fewer than this, and a bigger
	 *  ring is replication cost buying nothing a scoreboard doesn't already hold. */
	static constexpr int32 KillfeedCapacity = 8;
};
