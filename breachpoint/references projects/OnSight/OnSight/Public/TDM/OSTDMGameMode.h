#pragma once

#include "CoreMinimal.h"
#include "Core/OSGameMode.h"
#include "Data/OSHitDamageContext.h"
#include "TDM/OSTDMTeamData.h"
#include "OSTDMGameMode.generated.h"

/**
 * TDM GameMode.
 * - Assigns teams in InitNewPlayer (runs before PostLogin).
 * - Routes kill/death events from abilities into GameState stats and PlayerState scores.
 * - Checks kill limit and broadcasts win condition through GameState.
 * - Selects spawn points via AOSTDMTeamSpawner.
 */
UCLASS()
class ONSIGHT_API AOSTDMGameMode : public AOSGameMode
{
	GENERATED_BODY()

public:
	AOSTDMGameMode(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "OnSight|Match|TDM")
	FOSTDMMatchConfig MatchConfig;

	virtual FString InitNewPlayer(APlayerController* NewPlayerController,
	                              const FUniqueNetIdRepl& UniqueId,
	                              const FString& Options,
	                              const FString& Portal) override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void InitGameState() override;

	virtual void ConfigureWinCondition(AOSGameState* GS) override;

	/** TDM scoring + killfeed + team respawn; does not call base FFA scoring/QueueRespawn. */
	virtual void HandlePlayerDeath(AController* VictimController, const FOSDeathEventInfo& DeathEvent) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "OnSight|TDM|Match")
	void ReportKill(AController* KillerController,
	               AController* VictimController,
	               AController* AssistController = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "OnSight|TDM|Match")
	void RequestRespawn(AController* VictimController);

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** When time limit expires, resolve winner by team kills (or draw if tied). */
	virtual void OnMatchTimeExpired() override;

};
