#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "AIBAmbitionProvider.generated.h"

/** A game mode's standing contribution to the utility layer — Halo's Game Mode
 *  Ambitions, verbatim in concept. Pure data; the engine scores it like any other. */
USTRUCT()
struct AIBOT_API FAIBModeAmbition
{
	GENERATED_BODY()

	FGameplayTag AmbitionTag;      // under AIBot.Ambition.Mode
	float BaseUtility = 0.5f;
	FGameplayTag ObjectiveKind;    // joins FAIBPointOfInterest::Kind — typed, same
	                               // adapter owns both ends (W-REVIEW L3)
};

UINTERFACE(MinimalAPI, NotBlueprintable, meta = (CannotImplementInterfaceInBlueprint))
class UAIBAmbitionProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by the game mode (adapter folder). Slayer returns nothing and bots fight;
 * CTF returns capture/return/defend and they compete on the SAME scale as combat — a
 * mode adds ambitions, never a system. Urgency is re-read each think through
 * FAIBFacts::ObjectiveUrgency, so a dropped flag can outshout a fistfight.
 */
class IAIBAmbitionProvider
{
	GENERATED_BODY()

public:
	virtual void GetModeAmbitions(TArray<FAIBModeAmbition>& OutAmbitions) const = 0;

	/** How loudly the mode wants Bot on this ambition's objective RIGHT NOW.
	 *  CONTRACT: 0 = the mode does not care, 1 = drop everything. The facts builder
	 *  clamps to 0..1 at its one site (W-REVIEW F-6.7) — a provider returning 5.0
	 *  does not get to pin the bot on the objective forever. */
	virtual float GetObjectiveUrgency(const AActor* Bot, FGameplayTag AmbitionTag) const = 0;
};
