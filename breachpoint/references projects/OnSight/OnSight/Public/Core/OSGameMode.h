#pragma once
// Game mode: spawn rules, HandlePlayerDeath (server), and optional character selection. Death flows from Character to here.

#include "CoreMinimal.h"
#include "OSGameState.h"
#include "GameFramework/GameModeBase.h"
#include "Data/OSHitDamageContext.h"
#include "Characters/Player/OSPlayer.h"
#include "OSGameMode.generated.h"

class AOSPlayerState;
class AOSTeamSpawnZone;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerStatesDelegate, AOSPlayerState*, PlayerState);


UCLASS()
class ONSIGHT_API AOSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOSGameMode(const FObjectInitializer& ObjectInitializer);

	/** Server entrypoint: called when a player is confirmed dead. */
	virtual void HandlePlayerDeath(AController* VictimController, const FOSDeathEventInfo& DeathEvent);

	/** Override to spawn correct character based on PlayerState selection */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController);

	/** Blueprint character classes - set these in Blueprint GameMode */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Selection")
	TSubclassOf<AOSPlayer> Character1Class;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Selection")
	TSubclassOf<AOSPlayer> Character2Class;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Selection")
	TSubclassOf<AOSPlayer> Character3Class;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Selection")
	TArray<FLinearColor> CharacterColors;

	/** Broadcast when a player is ready (initial spawn or after respawn). Use for re-binding UI/ASC (e.g. HUD to AttributeSet). */
	UPROPERTY(BlueprintAssignable, Category="OnSight|Player")
	FPlayerStatesDelegate OnPlayerStateReady;
	
	// Call whenever a win condition value changes (score, kills, etc.)
	UFUNCTION(BlueprintCallable, Category="OnSight|GameMode")
	void CheckWinCondition();

	virtual void OnMatchTimeExpired();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="OnSight|Match")
	float PostMatchRematchDelaySeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OnSight|Match")
	int32 MatchDurationSeconds = 0;

	UPROPERTY(EditAnywhere, Category="OnSight|Match")
	float RespawnDelay = 3.f;

	UPROPERTY(EditAnywhere, Category="Respawn")
	float RespawnPenaltyPerDeath = 0.5f;

	UPROPERTY(EditAnywhere, Category="Respawn")
	float MaxRespawnDelay = 15.f;
	
	virtual void    InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	virtual void    InitGameState() override;
	virtual void    PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	
	// Derived gamemodes implement their own win condition. Use this for that.
	virtual bool QueryWinCondition(AOSPlayerState*& OutWinner) const;
	virtual void EndGame(AOSPlayerState* winnerPlayerState);
	virtual void BuildMatchResult(AOSGameState* GS, AOSPlayerState* PS);
	
	UFUNCTION()
	void RestartCurrentMatch();

	virtual void ConfigureWinCondition( AOSGameState* GS );
	virtual void UpdateWinCondition( );
	
	bool bGameEnded = false;
	UPROPERTY(EditDefaultsOnly, Category="Match")
	FOSWinConditionEntry WinCondition;
	
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TArray<TSubclassOf<UOSBaseLayerWidget>> ModeLayerClasses;
	
	AOSGameState* GrabGameState();
	UPROPERTY()
	AOSGameState* _GS;

	void QueueRespawn(AController* VictimController);

public:
	/** Override to check team spawn zones before falling back to PlayerStart.
	 *  Public: OSPlayerController calls this directly for manual respawn. */
	virtual void RestartPlayer(AController* NewPlayer) override;

protected:
	/** Find the best spawn zone for a player based on team and enemy proximity. Returns nullptr for FFA. */
	AOSTeamSpawnZone* FindBestSpawnZone(AController* Player) const;

private:
	void InitializePlayerState( const APlayerController* PlayerController) const;
	void PerformRespawn(AController* VictimController);
	float CalculateRespawnDelay(const AOSPlayerState* VictimPS) const;
	AOSPlayerState* FindMatchLeaderFallback(const AOSGameState* GS) const;

	/** Iterate RespawnTimers, clear each via the world timer manager, empty the map.
	 *  Called from EndGame so in-flight respawn timers don't fire during the post-match
	 *  rematch delay. */
	void ClearAllRespawnTimers();

	/** Tracks one respawn timer per controller so we don't double-schedule. */
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
	FTimerHandle RestartTimerHandle;
	

};

