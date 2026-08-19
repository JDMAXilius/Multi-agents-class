#include "AI/BNBotBrain.h"

FBNBotAmbitionRow UBNBotBrain::DefaultRow(EBNBotAmbition Ambition)
{
	// The roadmap's shipped-game numbers (G3 3.1): Survive out-bids Fight below ~35% health.
	FBNBotAmbitionRow Row;
	switch (Ambition)
	{
	case EBNBotAmbition::Fight:
		Row.BaseUtility = 1.f;
		Row.HealthWeight = 0.f;
		Row.TargetWeight = 1.f;
		Row.DistanceWeight = 0.f;
		Row.CommitSeconds = 3.f;
		break;
	case EBNBotAmbition::Survive:
		Row.BaseUtility = 1.2f;
		Row.HealthWeight = 1.f;
		Row.TargetWeight = 0.f;
		Row.DistanceWeight = 0.f;
		Row.CommitSeconds = 5.f;
		Row.InterruptBelowHealthNorm = 0.35f;
		break;
	case EBNBotAmbition::Roam:
	default:
		Row.BaseUtility = 0.2f;
		Row.HealthWeight = 0.f;
		Row.TargetWeight = 0.f;
		Row.DistanceWeight = 0.f;
		Row.CommitSeconds = 2.f;
		break;
	}
	return Row;
}

FName UBNBotBrain::AmbitionRowName(EBNBotAmbition Ambition)
{
	switch (Ambition)
	{
	case EBNBotAmbition::Fight:   return FName(TEXT("Fight"));
	case EBNBotAmbition::Survive: return FName(TEXT("Survive"));
	default:                      return FName(TEXT("Roam"));
	}
}

bool UBNBotBrain::Rescore(const FBNBotFacts& Facts, const FBNBotAmbitionRow& FightRow,
	const FBNBotAmbitionRow& SurviveRow, const FBNBotAmbitionRow& RoamRow, double NowSeconds)
{
	// Fight: only a visible target makes fighting worth anything — no target, zero.
	const float FightU = FightRow.BaseUtility * (Facts.bHasTarget ? FightRow.TargetWeight : 0.f);

	// Survive's shape: (1 - HealthNorm) rises as health falls; with no threat in sight the -0.5
	// term sinks it below Roam's floor, so a hurt-but-safe bot decays to roaming, not cowering.
	const float SurviveU = SurviveRow.BaseUtility *
		((1.f - Facts.HealthNorm) * SurviveRow.HealthWeight + (Facts.bHasTarget ? 0.f : -0.5f));

	// Roam: constant consideration 1.0 — its BaseUtility IS its score, the floor to beat.
	const float RoamU = RoamRow.BaseUtility;

	EBNBotAmbition Winner = EBNBotAmbition::Roam;
	float WinnerU = RoamU;
	if (SurviveU > WinnerU)
	{
		Winner = EBNBotAmbition::Survive;
		WinnerU = SurviveU;
	}
	if (FightU > WinnerU)
	{
		Winner = EBNBotAmbition::Fight;
		WinnerU = FightU;
	}


	// The ONE interrupt: health below the Survive row's threshold breaks any commit window,
	// immediately. NOT a utility ratio — the critic proved that form unreachable with real
	// weights (Survive's ceiling 1.2 can never double Fight's 1.0), which left a bot at 5%
	// health standing and firing through its whole commit. One number, on the row, data.
	const bool bSurviveInterrupt = CurrentAmbition != EBNBotAmbition::Survive
		&& SurviveRow.InterruptBelowHealthNorm > 0.f
		&& Facts.HealthNorm < SurviveRow.InterruptBelowHealthNorm;

	// Hysteresis: the held ambition stands until its window passes — bots visibly COMMIT.
	if (NowSeconds < CommitEndSeconds && !bSurviveInterrupt)
	{
		return false;
	}

	if (bSurviveInterrupt)
	{
		Winner = EBNBotAmbition::Survive;
		WinnerU = SurviveU;
	}

	if (Winner == CurrentAmbition)
	{
		CurrentUtility = WinnerU;
		return false;
	}

	CurrentAmbition = Winner;
	CurrentUtility = WinnerU;
	const FBNBotAmbitionRow& WinnerRow = Winner == EBNBotAmbition::Fight ? FightRow
		: Winner == EBNBotAmbition::Survive ? SurviveRow : RoamRow;
	CommitEndSeconds = NowSeconds + WinnerRow.CommitSeconds;

	switch (Winner)
	{
	case EBNBotAmbition::Fight:
		WinningConsideration = TEXT("a target is in sight");
		break;
	case EBNBotAmbition::Survive:
		WinningConsideration = Facts.bHasTarget ? TEXT("health is low under fire") : TEXT("health is low");
		break;
	default:
		WinningConsideration = TEXT("nothing better to want");
		break;
	}
	return true;
}
