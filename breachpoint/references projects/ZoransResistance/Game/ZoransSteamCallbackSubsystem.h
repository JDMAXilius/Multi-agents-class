// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include <steam/steam_api.h>
#include <steam/isteamuser.h>

#include "Data/ZoransAzurePlayFabData.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ZoransSteamCallbackSubsystem.generated.h"

class UZoransSteamCallbackSubsystem;

USTRUCT(BlueprintType)
struct FSteamMtxData
{
	GENERATED_BODY()

	uint64 OrderId;

	UPROPERTY(BlueprintReadOnly)
	bool bAuthorized = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMtxResponse, const FSteamMtxData, MtxData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAuthSessionTicketResponseDelegate, const bool, Success, const FString, SessionTicket);

UCLASS()
class UAuthSessionTickCallback : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	friend UZoransSteamCallbackSubsystem;
	
	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UAuthSessionTickCallback* GetSteamSessionTicket(UZoransSteamCallbackSubsystem* SteamCallbackSubsystem);
	
private:
	UPROPERTY(BlueprintAssignable)
	FOnAuthSessionTicketResponseDelegate Completed;
	
	#define MAX_STEAM_SESSION_TICKET_SIZE 1024
	uint8 SessionTicket[MAX_STEAM_SESSION_TICKET_SIZE];
	uint32 SessionTicketSize;
};

UCLASS()
class ZORANSRESISTANCE_API UZoransSteamCallbackSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	friend UAuthSessionTickCallback;
	
public:
	UPROPERTY(BlueprintAssignable)
	FOnMtxResponse OnMtxResponse;
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	FString GetSteamLanguage(); 

	UFUNCTION(BlueprintCallable)
	FSteamEndPurchaseRequest SteamMtxToRequest(const FSteamMtxData& Data) const;

	UFUNCTION(BlueprintCallable)
	bool IsConnectedToSteam() const;
private:
	STEAM_CALLBACK(UZoransSteamCallbackSubsystem, OnMicroTransactionPurchaseResponse, MicroTxnAuthorizationResponse_t);
	STEAM_CALLBACK(UZoransSteamCallbackSubsystem, OnAuthSessionTicketResponse, GetAuthSessionTicketResponse_t);
	
	UPROPERTY()
	UAuthSessionTickCallback* CurrentCallback = nullptr;
};
