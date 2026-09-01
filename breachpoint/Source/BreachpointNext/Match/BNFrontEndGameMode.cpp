#include "Match/BNFrontEndGameMode.h"
#include "BreachpointNext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/SpectatorPawn.h"
#include "UI/BNUIManager.h"
#include "UI/BNUITypes.h"

ABNFrontEndGameMode::ABNFrontEndGameMode()
{
	// A menu has no body. The spectator pawn exists so the controller has a view target
	// for whatever backdrop the FE map stages; players never leave spectator here.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	bStartPlayersAsSpectators = true;
}

void ABNFrontEndGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// PostLogin, not BeginPlay: the controller provably exists here, which is the one
	// precondition the UI manager's layout build has (it resolves the layout's owning
	// player through LocalPlayer->GetPlayerController).
	ShowFrontEnd(NewPlayer);
}

void ABNFrontEndGameMode::ShowFrontEnd(APlayerController* ForPlayer)
{
	ULocalPlayer* LocalPlayer = ForPlayer ? ForPlayer->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;   // a remote client on a listen host owns its own UI; nothing to do here
	}
	UBNUIManager* Manager = UBNUIManager::Get(this);
	if (!Manager)
	{
		UE_LOG(LogBN, Warning, TEXT("BNFrontEndGameMode: no UI manager (dedicated server?) — no menu."));
		return;
	}
	if (!Manager->EnsureLocalPlayerUI(LocalPlayer))
	{
		return;   // the manager already said why, loudly
	}
	if (!Manager->PushWidgetToLayer(LocalPlayer, FBNUITags::Get().Layer_Menu, Manager->GetFrontEndScreenClass()))
	{
		UE_LOG(LogBN, Warning, TEXT("BNFrontEndGameMode: front-end screen did not push — check "
			"FrontEndScreenClass under [/Script/BreachpointNext.BNUIManager]."));
	}
}
