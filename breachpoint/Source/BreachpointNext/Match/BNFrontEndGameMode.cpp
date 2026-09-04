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

void ABNFrontEndGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);
	// NOT PostLogin, and NOT BeginPlay.
	//
	// BeginPlay is too early -- the controller may not exist, and the UI manager's layout build
	// needs one (it resolves the owning player through LocalPlayer->GetPlayerController).
	//
	// PostLogin is too NARROW, and that was the bug: the menu appeared on a cold boot and never
	// again after a match. ABNGameMode sets bUseSeamlessTravel = true (BNGameMode.cpp:51) and
	// ends a match with World->ServerTravel (TravelToFrontEnd), so the return trip is a SEAMLESS
	// arrival -- the engine carries the player across and calls HandleSeamlessTravelPlayer, which
	// never calls PostLogin. ShowFrontEnd therefore never ran, and the player landed on the menu
	// map looking at the backdrop with no menu on it.
	//
	// GenericPlayerInitialization is the seam both doors share: PostLogin calls it
	// (GameModeBase.cpp:1005) and HandleSeamlessTravelPlayer calls it (:635). One override, both
	// arrivals, no double-push -- exactly one of the two paths runs per arrival.
	ShowFrontEnd(Cast<APlayerController>(C));
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
