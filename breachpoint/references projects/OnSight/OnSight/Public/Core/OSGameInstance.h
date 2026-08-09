#pragma once
// Game instance: Init/Shutdown. Holds OSSessionsSubsystem for multiplayer sessions.

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OSGameInstance.generated.h"

UCLASS()
class ONSIGHT_API UOSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UOSGameInstance(const FObjectInitializer& ObjectInitializer);

	/** Client or solo player voluntarily leaves — destroys session then ClientTravels to main menu */
	UFUNCTION(BlueprintCallable)
	void LeaveServer();

	/** Server-authoritative match end — EndSession → DestroySession → ServerTravel all clients to main menu */
	UFUNCTION(BlueprintCallable)
	void EndMatchAndReturnToMenu();

protected:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** ClientTravel: moves only the local player (used for LeaveServer / network failure) */
	void TravelToMainMenu() const;

	/** ServerTravel: moves the server and all connected clients (used for EndMatchAndReturnToMenu) */
	void ServerTravelToMainMenu() const;

private:
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	// LeaveServer callback — fires when DestroySession completes (success or failure)
	UFUNCTION()
	void OnLeaveServerDestroyComplete(bool bWasSuccessful);

	// EndMatchAndReturnToMenu callbacks — EndSession → DestroySession → ServerTravel
	UFUNCTION()
	void OnEndMatchEndSessionComplete(bool bWasSuccessful);

	UFUNCTION()
	void OnEndMatchDestroyComplete(bool bWasSuccessful);

	/** True while EndMatchAndReturnToMenu is in progress — prevents re-entrant ServerTravel loop. */
	bool bIsReturningToMenu = false;
};
