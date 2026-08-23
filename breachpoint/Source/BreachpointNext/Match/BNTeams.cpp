#include "Match/BNTeams.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Match/BNPlayerState.h"

int32 BNTeams::GetTeamId(const AActor* Actor)
{
	if (!Actor)
	{
		return Unassigned;
	}

	// THE PLAYER STATE IS THE IDENTITY, and the three shapes below are the three things a caller
	// realistically holds: the damage door has actors, the bot has pawns, the UI has PlayerStates.
	// Resolving all three here is what stops each caller inventing its own half-right lookup.
	if (const ABNPlayerState* PS = Cast<ABNPlayerState>(Actor))
	{
		return PS->GetTeamId();
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		const ABNPlayerState* PawnPS = Pawn->GetPlayerState<ABNPlayerState>();
		return PawnPS ? PawnPS->GetTeamId() : Unassigned;
	}
	if (const AController* Controller = Cast<AController>(Actor))
	{
		const ABNPlayerState* ControllerPS = Controller->GetPlayerState<ABNPlayerState>();
		return ControllerPS ? ControllerPS->GetTeamId() : Unassigned;
	}
	return Unassigned;
}

bool BNTeams::AreAllies(const AActor* A, const AActor* B)
{
	const int32 TeamA = GetTeamId(A);
	return IsValidTeam(TeamA) && TeamA == GetTeamId(B);
}
