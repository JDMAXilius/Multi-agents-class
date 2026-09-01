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

	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	/** Idempotent: builds this player's UI and pushes the front-end screen once. Loud on
	 *  every miss (no manager on a server, unset ini, unbuilt WBP) — never a crash. */
	void ShowFrontEnd(APlayerController* ForPlayer);
};
