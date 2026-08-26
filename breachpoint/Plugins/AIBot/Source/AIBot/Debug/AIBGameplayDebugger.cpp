#include "Debug/AIBGameplayDebugger.h"

#include "Brain/AIBAmbitionEngine.h"
#include "Core/AIBBotController.h"
#include "Core/AIBTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIBSensorium.h"
#include "Skills/AIBSkillProfile.h"

namespace
{
	const TCHAR* CompetenceLetter(EAIBCompetence Level)
	{
		switch (Level)
		{
		case EAIBCompetence::Novice:  return TEXT("N");
		case EAIBCompetence::Skilled: return TEXT("S");
		case EAIBCompetence::Expert:  return TEXT("E");
		case EAIBCompetence::Trained:
		default:                      return TEXT("T");
		}
	}
}

void AIBDebug::DrawBotOverlay(const AAIBBotController& Bot)
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = Bot.GetWorld();
	const APawn* Pawn = Bot.GetPawn();
	const UAIBAmbitionEngine* Engine = Bot.GetAmbitionEngine();
	if (!World || !Pawn || !Engine)
	{
		return;
	}

	const FAIBSkillProfile& Profile = Bot.GetSkillProfile();
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("%s [%s] Mv%s Aim%s Gr%s Me%s Cf%s Tw%s"),
		*Bot.GetName(), *Bot.GetTierName().ToString(),
		CompetenceLetter(Profile.Level(EAIBSkill::Movement)),
		CompetenceLetter(Profile.Level(EAIBSkill::Aim)),
		CompetenceLetter(Profile.Level(EAIBSkill::Grenade)),
		CompetenceLetter(Profile.Level(EAIBSkill::Melee)),
		CompetenceLetter(Profile.Level(EAIBSkill::Confidence)),
		CompetenceLetter(Profile.Level(EAIBSkill::Teamwork))));

	// The whole scoreboard, incumbent marked — arbitration is never a black box.
	for (const FAIBScoredAmbition& Row : Engine->GetLastScores())
	{
		Lines.Add(FString::Printf(TEXT("%s%s %.2f"),
			Row.bWasIncumbent ? TEXT("> ") : TEXT("  "),
			*Row.Tag.ToString(), Row.Score));
	}

	const FAIBFacts& Facts = Bot.GetLastFacts();
	Lines.Add(Facts.bConfidenceKnown
		? FString::Printf(TEXT("conf %.2f"), Facts.ConfidenceNorm)
		: FString(TEXT("conf UNKNOWN")));
	Lines.Add(FString::Printf(TEXT("stimuli pending %d"), Bot.GetSensorium().NumPendingStimuli()));

	// One string, one draw, one think of lifetime — the overlay breathes with the
	// brain instead of smearing stale frames.
	DrawDebugString(World, Pawn->GetActorLocation() + FVector(0.f, 0.f, 110.f),
		FString::Join(Lines, TEXT("\n")), nullptr, FColor::Cyan, 0.11f, /*bDrawShadow=*/true);
#endif
}
