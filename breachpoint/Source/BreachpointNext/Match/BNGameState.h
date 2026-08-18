#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BNGameState.generated.h"

class ABNPlayerState;

UENUM()
enum class EBNMatchState : uint8
{
	WaitingToStart,
	InProgress,
	PostMatch
};

/** Fires on EVERY machine: the server from SetMatchState, remote clients from OnRep_MatchState.
 *  Native (not dynamic) to match the OnPlayerDeath seam — the readers are C++ systems.
 *
 *  Declared above UCLASS() because UnrealHeaderTool requires the class definition to immediately
 *  follow the macro. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBNMatchStateSignature, EBNMatchState /*NewState*/);

UCLASS()
class BREACHPOINTNEXT_API ABNGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	FBNMatchStateSignature OnMatchStateChanged;

	EBNMatchState GetMatchState() const { return MatchState; }
	double GetMatchEndServerTime() const { return MatchEndServerTime; }
	int32 GetScoreLimit() const { return ScoreLimit; }
	ABNPlayerState* GetWinner() const { return Winner; }

	/** Seconds until MatchEndServerTime, computed LOCALLY against server time on whatever machine
	 *  asks. Zero when no clock is stamped and never negative. */
	float GetRemainingSeconds() const;

	/** Everyone tied at the top of the kill count. Empty when nobody is playing. */
	void GetLeaders(TArray<ABNPlayerState*>& OutLeaders) const;

	/** Authority only. The server has no OnRep, so each of these fires the delegate itself. */
	void SetMatchState(EBNMatchState NewState);
	void SetMatchEndServerTime(double InEndServerTime);
	void SetWinner(ABNPlayerState* InWinner);

protected:
	UFUNCTION()
	void OnRep_MatchState();

	/** The one body both the server's setter and the clients' OnRep run. */
	void HandleMatchStateChanged();

	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	EBNMatchState MatchState = EBNMatchState::WaitingToStart;

	/** THE END STAMP, not a countdown. A ticking replicated counter is a per-second write to every
	 *  client for the whole match, and a late joiner reads whatever value happened to be in flight.
	 *  One stamp replicates once and is already correct for a client that joins at any moment. */
	UPROPERTY(Replicated)
	double MatchEndServerTime = 0.0;

	/** Mirrored from the mode's config so a client can render "12 / 25" without asking the server. */
	UPROPERTY(Replicated)
	int32 ScoreLimit = 0;

	UPROPERTY(Replicated)
	TObjectPtr<ABNPlayerState> Winner;
};
