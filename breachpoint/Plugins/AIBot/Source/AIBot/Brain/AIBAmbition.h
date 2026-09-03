#pragma once

#include "CoreMinimal.h"
#include "Brain/AIBConsideration.h"
#include "GameplayTagContainer.h"
#include "AIBAmbition.generated.h"

/**
 * An ambition: what the bot can WANT, as a scoring recipe. Halo's model verbatim —
 * every behaviour is bound to one of these, a utility function picks the winner.
 *
 * Score = BaseUtility × Π Evaluate(consideration). Multiplicative on purpose: any
 * consideration near zero vetoes the ambition (out of grenades ⇒ no grenade ambition,
 * whatever the rest say), which is the behaviour a weighted sum cannot express.
 */
USTRUCT()
struct AIBOT_API FAIBAmbitionSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ambition")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = "Ambition")
	float BaseUtility = 1.f;

	UPROPERTY(EditAnywhere, Category = "Ambition")
	TArray<FAIBConsideration> Considerations;

	/** Minimum hold after this ambition wins — the anti-dither commit. Only a hard
	 *  interrupt (the engine's, not the spec's) may break it early. */
	UPROPERTY(EditAnywhere, Category = "Ambition")
	float CommitSeconds = 3.f;

	/** AIB22 F8-4: with a target held and a melee in hand the want never scores below this
	 *  (0 = no floor). Applied to the raw product BEFORE suppression, so a suppressed want
	 *  stays at 0. Engage's is the Roam floor + 0.05: a dry bot with an enemy in front of it
	 *  closes to punch instead of wandering (WeaponCanFight=0 vetoed it for 270 s). */
	UPROPERTY(EditAnywhere, Category = "Ambition")
	float MeleeFloorUtility = 0.f;
};

/** One ambition's outcome from a rescore — the introspection row the gameplay
 *  debugger and the specs read, so scoring is never a black box. */
USTRUCT()
struct AIBOT_API FAIBScoredAmbition
{
	GENERATED_BODY()

	FGameplayTag Tag;
	float Score = 0.f;
	bool bWasIncumbent = false;
};
