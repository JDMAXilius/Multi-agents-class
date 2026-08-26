#include "Data/AIBTiers.h"

#include "Core/AIBTypes.h"

namespace
{
	FAIBTierRow MakeRecruit()
	{
		FAIBTierRow Row;
		Row.Movement = EAIBCompetence::Novice;
		Row.Aim = EAIBCompetence::Novice;
		Row.Grenade = EAIBCompetence::Novice;
		Row.Melee = EAIBCompetence::Novice;
		Row.Confidence = EAIBCompetence::Novice;
		Row.Teamwork = EAIBCompetence::Novice;
		// Tunnel vision and a slow hand are the Recruit's perception tells; the sight
		// RADII stay the shared envelope (see the header's anchoring note).
		Row.PeripheralVisionAngleDegrees = 60.f;
		Row.ReactionSecondsMin = 0.34f;
		Row.ReactionSecondsMax = 0.60f;
		Row.MemoryFreshSeconds = 8.f;
		return Row;
	}

	FAIBTierRow MakeMarine()
	{
		// The baseline IS the FAIBTierRow default row (its comment says so) — restating
		// its numbers here would be the second source of truth that drifts.
		return FAIBTierRow();
	}

	FAIBTierRow MakeODST()
	{
		FAIBTierRow Row;
		Row.Movement = EAIBCompetence::Skilled;
		Row.Aim = EAIBCompetence::Skilled;
		Row.Grenade = EAIBCompetence::Skilled;
		Row.Melee = EAIBCompetence::Skilled;
		Row.Confidence = EAIBCompetence::Skilled;
		Row.Teamwork = EAIBCompetence::Trained; // first rung ON the claims board
		Row.PeripheralVisionAngleDegrees = 78.f;
		Row.ReactionSecondsMin = 0.22f;
		Row.ReactionSecondsMax = 0.34f;
		Row.MemoryFreshSeconds = 18.f;
		return Row;
	}

	FAIBTierRow MakeSpartan()
	{
		FAIBTierRow Row;
		Row.Movement = EAIBCompetence::Expert;
		Row.Aim = EAIBCompetence::Expert;
		Row.Grenade = EAIBCompetence::Expert;
		Row.Melee = EAIBCompetence::Expert;
		Row.Confidence = EAIBCompetence::Expert;
		Row.Teamwork = EAIBCompetence::Skilled;
		Row.PeripheralVisionAngleDegrees = 84.f;
		// The min RIDES the F1 floor deliberately — the draw's bottom is the module's
		// constant, not this row's; the max is where the Spartan is still human.
		Row.ReactionSecondsMin = AIB::MinReactionSeconds;
		Row.ReactionSecondsMax = 0.28f;
		Row.MemoryFreshSeconds = AIB::MaxMemorySeconds;
		return Row;
	}

	const TMap<FName, FAIBTierRow>& Registry()
	{
		static const TMap<FName, FAIBTierRow> Tiers = []()
		{
			TMap<FName, FAIBTierRow> Map;
			Map.Add(TEXT("Recruit"), MakeRecruit());
			Map.Add(TEXT("Marine"), MakeMarine());
			Map.Add(TEXT("ODST"), MakeODST());
			Map.Add(TEXT("Spartan"), MakeSpartan());
			return Map;
		}();
		return Tiers;
	}
}

void AIBTiers::GetTierNames(TArray<FName>& OutNames)
{
	OutNames = { TEXT("Recruit"), TEXT("Marine"), TEXT("ODST"), TEXT("Spartan") };
}

const FAIBTierRow* AIBTiers::Find(FName TierName)
{
	return Registry().Find(TierName);
}

TArray<FString> AIBTiers::ValidateRow(FName TierName, const FAIBTierRow& Row)
{
	TArray<FString> Warnings;
	const FString Tier = TierName.ToString();

	if (Row.ReactionSecondsMin < AIB::MinReactionSeconds)
	{
		Warnings.Add(FString::Printf(TEXT("%s: ReactionSecondsMin %.2f is under the F1 floor %.2f — the clock clamps it; the authored number is silently doing nothing."),
			*Tier, Row.ReactionSecondsMin, AIB::MinReactionSeconds));
	}
	if (Row.ReactionSecondsMax < Row.ReactionSecondsMin)
	{
		Warnings.Add(FString::Printf(TEXT("%s: reaction draw is inverted (max %.2f < min %.2f)."),
			*Tier, Row.ReactionSecondsMax, Row.ReactionSecondsMin));
	}
	if (Row.LoseSightRadius < Row.SightRadius)
	{
		Warnings.Add(FString::Printf(TEXT("%s: LoseSightRadius %.0f is inside SightRadius %.0f — acquisition and loss will flap on the boundary."),
			*Tier, Row.LoseSightRadius, Row.SightRadius));
	}
	if (Row.MemoryFreshSeconds > AIB::MaxMemorySeconds)
	{
		Warnings.Add(FString::Printf(TEXT("%s: MemoryFreshSeconds %.0f exceeds the module ceiling %.0f — the read clamps it (F5)."),
			*Tier, Row.MemoryFreshSeconds, AIB::MaxMemorySeconds));
	}
	if (Row.LoseSightRadius < AIB::EngageFadeEndUU)
	{
		Warnings.Add(FString::Printf(TEXT("%s: LoseSightRadius %.0f is inside the band anchor %.0f — grenade/engage bands would poke outside the envelope (the inert-band defect)."),
			*Tier, Row.LoseSightRadius, AIB::EngageFadeEndUU));
	}
	return Warnings;
}
