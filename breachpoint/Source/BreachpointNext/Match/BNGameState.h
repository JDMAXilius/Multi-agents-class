#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "BNGameState.generated.h"

class ABNPlayerState;

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

	double GetMatchEndServerTime() const { return MatchEndServerTime; }
	int32 GetScoreLimit() const { return ScoreLimit; }
	ABNPlayerState* GetWinner() const { return Winner; }

	/** Seconds until MatchEndServerTime, computed LOCALLY against server time on whatever machine
	 *  asks. Zero when no clock is stamped and never negative. */
	float GetRemainingSeconds() const;

	/** Everyone tied at the top of the kill count. Empty when nobody is playing. */
	void GetLeaders(TArray<ABNPlayerState*>& OutLeaders) const;

	/** Authority only. Neither has an OnRep body of its own that announces — the state machine's
	 *  announcement (OnRep_MatchState below) is the one broadcast readers subscribe to. */
	void SetMatchEndServerTime(double InEndServerTime);
	void SetWinner(ABNPlayerState* InWinner);

protected:
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
};
