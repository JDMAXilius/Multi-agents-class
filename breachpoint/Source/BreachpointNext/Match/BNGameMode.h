#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayEffectTypes.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "BNGameMode.generated.h"

class ABNBotController;
class ABNPlayerState;

UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABNGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** The ONE initialization seam for both kinds of player: the engine calls it for humans in the
	 *  login flow, the bot fill calls it for bots — an AIController never sees OnPostLogin.
	 *  Subscribes this mode to the player's death announcement (law 7: the mode LISTENS for deaths
	 *  rather than being called by the ability that causes them) and freezes a warmup/post-match
	 *  joiner. The engine may call it more than once per controller, so the subscription is
	 *  guarded against doubling. */
	virtual void GenericPlayerInitialization(AController* C) override;

	/** Humans only. The arrival is what can satisfy MinPlayers, so only the start gate lives here;
	 *  everything both kinds of player need moved to GenericPlayerInitialization. */
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

	/** G4 — the fill. Authority, WaitingToStart only: tops the lobby up to TargetPlayers with
	 *  bots, counting humans (GetNumPlayers) plus the bots already spawned, so a human joining
	 *  warmup re-triggering TryStartMatch never over-spawns. Never runs mid-match or post-match —
	 *  mid-match backfill is R5's named deferral. */
	void EnsureBotFill();

	/** One bot, Lyra's way: spawn the controller (transient — never saved into a map), name its
	 *  PlayerState, GenericPlayerInitialization, RestartPlayer. Null on failure, loudly. */
	ABNBotController* SpawnBot(int32 Index);

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

	/** The lobby size bots fill up to. Humans + bots together reach this, never exceed it. */
	UPROPERTY(Config)
	int32 TargetPlayers = 4;

	/** In order; a bot past the end of the list falls back to "Bot <n>". */
	UPROPERTY(Config)
	TArray<FString> BotNames;

	/** Soft (law 3): the ini names the C++ class path, nothing here hard-references it. */
	UPROPERTY(Config)
	TSoftClassPtr<ABNBotController> BotControllerClass;

	/** Server-only bookkeeping — bots exist nowhere else, so nothing here replicates. */
	UPROPERTY()
	TArray<TObjectPtr<ABNBotController>> SpawnedBots;

	UPROPERTY(Config)
	float PostMatchDuration = 10.f;

	/** Bumped by every BeginMatch. A respawn timer armed in an older generation is dropped when it
	 *  fires rather than destroying a pawn that belongs to the round after it. */
	int32 MatchGeneration = 0;

	/** ONE timer for the whole match clock (law 4: no Tick, no per-frame poll). */
	FTimerHandle MatchTimerHandle;
	FTimerHandle PostMatchTimerHandle;

	/** The active freeze GE per player, so it is removed by handle rather than by sweeping tags off
	 *  an ASC that holds other State.* effects. Weak keys: a player can leave while frozen. */
	TMap<TWeakObjectPtr<ABNPlayerState>, FActiveGameplayEffectHandle> FrozenHandles;
};
