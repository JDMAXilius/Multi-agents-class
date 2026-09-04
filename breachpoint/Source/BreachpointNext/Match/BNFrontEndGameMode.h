#pragma once

#include "GameFramework/GameModeBase.h"
#include "BNFrontEndGameMode.generated.h"

/**
 * The FRONT-END map's game mode: no match, no pawn, no HUD — a spectator and a menu.
 *
 * Deliberately AGameModeBase and deliberately NOT ABNGameMode: the match game mode fills
 * bots to TargetPlayers, arms the match clock and spawns the HUD director's whole world,
 * and every one of those on a menu map would be a bug with a spawn point. The two modes
 * meeting only at the travel URL is the design.
 *
 * Boot: FE map's WorldSettings names this mode; this mode pushes the front-end screen at
 * login. The reverse door already exists — ABNPlayerController::LeaveMatch travels to
 * LeaveMatchMapPath, which the ini now points at the FE map, closing the loop
 * menu -> match -> menu with zero new travel code.
 */
UCLASS()
class BREACHPOINTNEXT_API ABNFrontEndGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABNFrontEndGameMode();

	/** BOTH doors, not one. The engine funnels player init through GenericPlayerInitialization,
	 *  which PostLogin() and HandleSeamlessTravelPlayer() BOTH call (GameModeBase.cpp:1005 and
	 *  :635). Overriding PostLogin alone showed the menu on a cold boot and NOT after a match:
	 *  ABNGameMode sets bUseSeamlessTravel=true and ends the match with ServerTravel, and a
	 *  seamless arrival never calls PostLogin. */
	virtual void GenericPlayerInitialization(AController* C) override;

protected:
	/** Idempotent: builds this player's UI and pushes the front-end screen once. Loud on
	 *  every miss (no manager on a server, unset ini, unbuilt WBP) — never a crash. */
	void ShowFrontEnd(APlayerController* ForPlayer);
};
