#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "AIBWorldQuery.generated.h"

/** One thing worth going somewhere for: a pickup, an objective slot, a position. */
USTRUCT()
struct AIBOT_API FAIBPointOfInterest
{
	GENERATED_BODY()

	FVector Location = FVector::ZeroVector;
	FGameplayTag Kind;             // TYPED join key — an untyped FName compare across two
	                               // separately implemented adapters fails silently on
	                               // "objective" vs "Objective" (W-REVIEW L3)
	float Worth = 1.f;             // provider-scaled 0..1
	TWeakObjectPtr<AActor> Actor;  // optional backing actor (a pickup, a flag)

	/** SLOT vs ZONE (W-AUDIT P7): true = one agent can usefully take or occupy this —
	 *  a pickup, a flag-carry, one provider-enumerated defensive position — and the
	 *  claims board may arbitrate it. False (the default) = a zone objective (a hill):
	 *  the board refuses it, because zeroing a zone's want for teammates is how a team
	 *  mode gets a one-defender hill. The PROVIDER declares capacity — a zone that
	 *  wants N bodies exposes N slot-POIs, never one claimable blob. */
	bool bClaimableSlot = false;
};

UINTERFACE(MinimalAPI, NotBlueprintable, meta = (CannotImplementInterfaceInBlueprint))
class UAIBWorldQuery : public UInterface
{
	GENERATED_BODY()
};

/**
 * The bot's questions about the world beyond its own body. Implemented game-side
 * (adapter folder); typically found on the GameState or a subsystem, resolved once by
 * AIBBotManager and handed to controllers.
 *
 * FAIRPLAY F3 governs every answer: results must be bounded by the asker's perception
 * envelope OR be HUD-grade knowledge (scores, objective state — what a human's screen
 * shows). An implementation that iterates all actors unbounded is the 25 Aug wallhack
 * again, and it is the ADAPTER that aib-critic attacks for it.
 */
class IAIBWorldQuery
{
	GENERATED_BODY()

public:
	/** Points worth walking to near Asker, envelope-bounded, Worth-sorted. */
	virtual void QueryPointsOfInterest(const AActor* Asker, float MaxDistance,
		TArray<FAIBPointOfInterest>& OutPoints) const = 0;

	/** Living enemies of Asker inside Radius WITH line of sight — the fair candidate
	 *  set for target acquisition. Never the whole pawn list. */
	virtual void QueryVisibleEnemies(const AActor* Asker, float Radius,
		TArray<AActor*>& OutEnemies) const = 0;

	/** Allies of Asker within Radius (HUD-grade: outlines/radar show teammates). */
	virtual int32 CountNearbyAllies(const AActor* Asker, float Radius) const = 0;

	/** THE hostility authority (W-REVIEW M5): the game answers friend-or-foe, so teams
	 *  land without an edit inside this module. The controller's FFA attitude override
	 *  is the fallback for a host that provides no world query. */
	virtual bool AreEnemies(const AActor* A, const AActor* B) const = 0;
};
