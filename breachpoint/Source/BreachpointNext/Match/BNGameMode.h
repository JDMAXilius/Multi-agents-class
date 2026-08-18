#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayEffectTypes.h"
#include "UObject/SoftObjectPath.h"
#include "BNGameMode.generated.h"

class ABNPlayerState;

UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABNGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** Subscribes this mode to a player's death announcement. Called as each PlayerState appears —
	 *  the mode LISTENS for deaths rather than being called by the ability that causes them (law 7),
	 *  so a mode with no respawn simply does not subscribe. */
	virtual void OnPostLogin(AController* NewPlayer) override;

	/** The subscriber. Turns "this player died" into this mode's answer: THE kill line, then a
	 *  timed respawn. Scoring attaches here when it exists — the seam already carries the killer. */
	void HandlePlayerDeath(ABNPlayerState* Victim, ABNPlayerState* Killer);

	/** Read by ABNGameState, which mirrors it so clients can render "12 / 25". */
	int32 GetScoreLimit() const { return ScoreLimit; }

	/** Authority: the ONE respawn path. Delay then RestartPlayer. */
	void RequestRespawn(AController* Controller);

protected:
	/** Weak, because the player can leave inside the delay and a raw pointer would outlive them.
	 *  Refuses outside InProgress — a corpse must not stand up during the post-match. */
	void RespawnPlayer(TWeakObjectPtr<AController> WeakController);

	/** MinPlayers present while still waiting, and the match begins. */
	void TryStartMatch();

	/** Restamps the clock, arms the ONE time-limit timer, thaws everyone, announces InProgress.
	 *  Both the first start and the in-place restart run this body. */
	void BeginMatch();

	/** InWinner may be null: a tie at the buzzer is a legal, renderable outcome. */
	void EndMatch(ABNPlayerState* InWinner);

	/** The time limit's timer fires here. Sole leader wins; a tie leaves Winner null. */
	void OnTimeLimitReached();

	/** IN PLACE — no ServerTravel, no map reload, so the listen server's connections survive. */
	void RestartMatch();

	/** The freeze, as GAS state: a UBNGE_State spec carrying State.Match.Frozen. The tag rides the
	 *  SPEC, never a CDO container — native tags are not guaranteed registered while CDOs build. */
	void SetPlayerFrozen(ABNPlayerState* InPlayerState, bool bFrozen);
	void SetAllPlayersFrozen(bool bFrozen);

	UPROPERTY(Config)
	FSoftClassPath DefaultPawnClassPath;

	UPROPERTY(Config)
	float RespawnDelay = 3.f;

	UPROPERTY(Config)
	int32 ScoreLimit = 25;

	UPROPERTY(Config)
	float TimeLimit = 600.f;

	/** Default 1 so solo PIE always runs. */
	UPROPERTY(Config)
	int32 MinPlayers = 1;

	UPROPERTY(Config)
	float PostMatchDuration = 10.f;

	/** ONE timer for the whole match clock (law 4: no Tick, no per-frame poll). */
	FTimerHandle MatchTimerHandle;
	FTimerHandle PostMatchTimerHandle;

	/** The active freeze GE per player, so it is removed by handle rather than by sweeping tags off
	 *  an ASC that holds other State.* effects. Weak keys: a player can leave while frozen. */
	TMap<TWeakObjectPtr<ABNPlayerState>, FActiveGameplayEffectHandle> FrozenHandles;
};
