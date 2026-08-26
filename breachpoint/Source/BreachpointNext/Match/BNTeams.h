#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"

/**
 * THE ONE TEAM QUERY (BN-TEAMS-PACKET, 26 Aug 2026). Everything that asks "friend or
 * foe?" asks here — combat, scoring, spawns, the bot adapter. The single sentinel is
 * FGenericTeamId::NoTeam; no int accessor ships (the OnSight reference ran three
 * different "no team" encodings and its own docs lied about one — the trap this
 * header exists to make unrepresentable).
 *
 * THE NoTeam GUARD is load-bearing: the engine's default attitude solver answers
 * GetAttitude(NoTeam, NoTeam) == Friendly, so an unguarded query would make FFA
 * players "friendly" and block ALL damage. Guarded, one combat codebase serves FFA
 * and team modes with zero mode branching — FFA is simply "everyone NoTeam".
 *
 * This helper is the ONLY caller of FGenericTeamId::GetAttitude in game code
 * (grep-enforced at review, the module-law way) — the reference's one latent bug was
 * a stale ExecCalc calling GetAttitude raw, without the guard.
 */
namespace BNTeams
{
	/** Two sides for the TDM framework; NoTeam (255) is FFA/unassigned. */
	inline constexpr uint8 NumTeams = 2;

	inline bool AreFriendly(FGenericTeamId A, FGenericTeamId B)
	{
		if (A == FGenericTeamId::NoTeam || B == FGenericTeamId::NoTeam)
		{
			return false; // the guard — unassigned is nobody's friend, never everybody's
		}
		return FGenericTeamId::GetAttitude(A, B) == ETeamAttitude::Friendly;
	}

	/** Actor-level ask: null, self, or either side lacking the interface answers
	 *  NOT-friendly — the honest default for every combat consumer. */
	inline bool AreActorsFriendly(const AActor* A, const AActor* B)
	{
		if (!A || !B || A == B)
		{
			return false;
		}
		const IGenericTeamAgentInterface* TeamA = Cast<IGenericTeamAgentInterface>(A);
		const IGenericTeamAgentInterface* TeamB = Cast<IGenericTeamAgentInterface>(B);
		if (!TeamA || !TeamB)
		{
			return false;
		}
		return AreFriendly(TeamA->GetGenericTeamId(), TeamB->GetGenericTeamId());
	}
}
