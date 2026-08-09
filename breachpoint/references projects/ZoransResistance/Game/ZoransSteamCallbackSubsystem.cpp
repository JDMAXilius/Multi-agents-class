// Copyright Zollpa LLC


#include "ZoransSteamCallbackSubsystem.h"

#include "Data/ZoransAzurePlayFabData.h"


void UAuthSessionTickCallback::Activate()
{
	SteamUser()->GetAuthSessionTicket(SessionTicket, MAX_STEAM_SESSION_TICKET_SIZE, &SessionTicketSize);
}

UAuthSessionTickCallback* UAuthSessionTickCallback::GetSteamSessionTicket(UZoransSteamCallbackSubsystem* SteamCallbackSubsystem)
{
	ensure(IsValid(SteamCallbackSubsystem));
	SteamCallbackSubsystem->CurrentCallback = NewObject<UAuthSessionTickCallback>();
	return SteamCallbackSubsystem->CurrentCallback;
}

void UZoransSteamCallbackSubsystem::OnMicroTransactionPurchaseResponse(MicroTxnAuthorizationResponse_t* pParam)
{
	FSteamMtxData MtxData;
	MtxData.bAuthorized = static_cast<bool>(pParam->m_bAuthorized);
	MtxData.OrderId = pParam->m_ulOrderID;
	
	OnMtxResponse.Broadcast(MtxData);
}

void UZoransSteamCallbackSubsystem::OnAuthSessionTicketResponse(GetAuthSessionTicketResponse_t* pParam)
{
	if(IsValid(CurrentCallback))
	{
		const bool bSuccess = pParam->m_eResult == k_EResultOK;
		const FString TicketString = bSuccess ? BytesToHex(CurrentCallback->SessionTicket, CurrentCallback->SessionTicketSize) : "";
		CurrentCallback->Completed.Broadcast(bSuccess, TicketString);
		CurrentCallback = nullptr;
	}
}

FString UZoransSteamCallbackSubsystem::GetSteamLanguage()
{
	return FString(SteamApps()->GetCurrentGameLanguage());
}

FSteamEndPurchaseRequest UZoransSteamCallbackSubsystem::SteamMtxToRequest(const FSteamMtxData& Data) const
{
	FSteamEndPurchaseRequest Request;
	Request.Success = Data.bAuthorized;
	Request.OrderId = FString::Printf(TEXT("%llu"), Data.OrderId);
	return Request;
}

bool UZoransSteamCallbackSubsystem::IsConnectedToSteam() const
{
	return SteamUser() != nullptr;
}

