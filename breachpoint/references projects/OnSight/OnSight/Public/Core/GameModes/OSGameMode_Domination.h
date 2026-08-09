#pragma once

#include "CoreMinimal.h"
#include "Core/OSGameMode.h"
#include "OSGameMode_Domination.generated.h"

class AOSGameState_Domination;
class AOSControlPoint;
class AOSPlayerState;

UCLASS()
class ONSIGHT_API AOSGameMode_Domination : public AOSGameMode
{
	GENERATED_BODY()

public:
	AOSGameMode_Domination(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION()
	void RegisterControlPoint(AOSControlPoint* Point);

	UFUNCTION()
	void UnregisterControlPoint(const FName& Name);

	void OnScoreTick(AOSControlPoint* Point, int32 OwningTeamIndex);
	void OnPointStateChanged(AOSControlPoint* Point);

	/** Force-end the match with the given team as winner. Use for cheats/debug. -1 = draw. */
	UFUNCTION(BlueprintCallable, Category="Domination")
	void EndDominationMatch(int32 WinningTeamIndex);

	void SetScoreLimit(int32 NewLimit);

protected:
	UPROPERTY(EditDefaultsOnly, Category="Domination", meta=(ClampMin="1"))
	int32 ScoreLimit = 200;

	UPROPERTY(EditDefaultsOnly, Category="Domination", meta=(ClampMin="0"))
	int32 TimeLimitSeconds = 600;

	UPROPERTY(EditDefaultsOnly, Category="Domination", meta=(ClampMin="1"))
	int32 CaptureRate = 5;

	UPROPERTY(EditDefaultsOnly, Category="Domination", meta=(ClampMin="1"))
	int32 DecayRate = 5;

	UPROPERTY(EditDefaultsOnly, Category="Domination", meta=(ClampMin="1"))
	int32 MaxCap = 100;

	UPROPERTY(EditDefaultsOnly, Category="Domination", meta=(ClampMin="1"))
	int32 ScoreRate = 1;

	UPROPERTY(EditDefaultsOnly, Category="Domination", meta=(ClampMin="0.1"))
	float CaptureInterval = 0.5f;

	/** Logarithmic scaling for teammates on a capture point. Higher = more benefit per extra player. */
	UPROPERTY(EditDefaultsOnly, Category="Domination|Capture", meta=(ClampMin="0.0"))
	float TeamPresenceCaptureScaling = 0.6f;

	/** Maximum capture speed multiplier regardless of player count. */
	UPROPERTY(EditDefaultsOnly, Category="Domination|Capture", meta=(ClampMin="1.0"))
	float MaxTeamPresenceMultiplier = 2.0f;

	/** Base interval between score ticks when a team owns exactly 1 point. */
	UPROPERTY(EditDefaultsOnly, Category="Domination|Scoring", meta=(ClampMin="0.1"))
	float BaseScoreInterval = 3.0f;

	/** Per-additional-point multiplier for score tick speed. More points = faster ticks. */
	UPROPERTY(EditDefaultsOnly, Category="Domination|Scoring", meta=(ClampMin="0.0"))
	float PointScoreMultiplier = 0.5f;

	/** Recalculates score timer intervals for all captured points based on team ownership counts. */
	void RecalculateScoreIntervals();

	virtual void InitGameState() override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal) override;
	virtual void HandlePlayerDeath(AController* VictimController, const FOSDeathEventInfo& DeathEvent) override;
	virtual void Logout(AController* Exiting) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual bool QueryWinCondition(AOSPlayerState*& OutWinner) const override;
	virtual void BuildMatchResult(AOSGameState* GS, AOSPlayerState* PS) override;
	virtual void ConfigureWinCondition(AOSGameState* GS) override;
	virtual void OnMatchTimeExpired() override;

private:
	UPROPERTY()
	TMap<FName, AOSControlPoint*> RegisteredPoints;

	int32 NextPointLabelIndex = 0;
	TSet<FString> UsedLabels;

	void RemovePlayerFromAllPoints(AController* Controller);
	void CancelAllPlayerAbilities();
	void ReportKill(AController* KillerController, AController* VictimController, AController* AssistController = nullptr);
	void RequestRespawn(AController* VictimController);

	/** Called on spawn/respawn — forces overlap re-evaluation so spawning inside a CP triggers entry. */
	UFUNCTION()
	void OnPlayerReady(AOSPlayerState* PS);

	AOSGameState_Domination* GetDomGameState() const;
	void CheckDomWinCondition(int32 TeamIndex);

	FTimerHandle DomRematchHandle;
};
