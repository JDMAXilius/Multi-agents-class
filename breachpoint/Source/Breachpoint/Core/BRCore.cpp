#include "Core/BRCore.h"

#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogBRCombat);
DEFINE_LOG_CATEGORY(LogBRNet);
DEFINE_LOG_CATEGORY(LogBRAI);
DEFINE_LOG_CATEGORY(LogBRAbility);

namespace BRTeams
{
	ETeamAttitude::Type GetAttitude(const AActor* A, const AActor* B)
	{
		// GetTeamIdentifier is null-safe and returns NoTeam for actors that do not
		// implement IGenericTeamAgentInterface.
		const FGenericTeamId TeamA = FGenericTeamId::GetTeamIdentifier(A);
		const FGenericTeamId TeamB = FGenericTeamId::GetTeamIdentifier(B);

		if (TeamA == FGenericTeamId::NoTeam || TeamB == FGenericTeamId::NoTeam)
		{
			return ETeamAttitude::Neutral;
		}

		return TeamA == TeamB ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
	}
}
