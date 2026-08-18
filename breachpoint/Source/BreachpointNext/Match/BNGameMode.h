#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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
	/** Weak, because the player can leave inside the delay and a raw pointer would outlive them. */
	void RespawnPlayer(TWeakObjectPtr<AController> WeakController);

	UPROPERTY(Config)
	FSoftClassPath DefaultPawnClassPath;

	UPROPERTY(Config)
	float RespawnDelay = 3.f;

	UPROPERTY(Config)
	int32 ScoreLimit = 25;
};
