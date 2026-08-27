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

	/** HOW BIG "BEING THERE" IS, in uu, as the OBJECTIVE defines it — a hill's own
	 *  radius, a capture zone's extent. 0 means "a point": the mover keeps its own
	 *  acceptance and nothing changes.
	 *
	 *  Exists because the alternative is a generic task default deciding arrival for
	 *  every objective in every mode, and that default was measurably wrong: a 600uu
	 *  hill against a 200uu acceptance meant a bot standing squarely ON the hill,
	 *  313uu from its centre and scoring, was still told to close — then gave up on
	 *  no-progress and reported it could not REACH an objective it was already
	 *  occupying. 63 failures, minimum distance 201uu, one metre outside acceptance.
	 *
	 *  The provider knows the shape of its own objective; the mover cannot guess it. */
	float ReachRadiusUU = 0.f;
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
	 *  is the fallback for a host that provides no world query.
	 *
	 *  AreEnemies answers "is B a valid live target of A" — liveness is PART of the
	 *  answer (a corpse is nobody's enemy), so its NEGATION is not "ally". */
	virtual bool AreEnemies(const AActor* A, const AActor* B) const = 0;

	/** AIB17 — the tag door for the ally-fight tap: does this HEARING stimulus tag mean
	 *  weapon noise? The module must not name a host's noise tags (boundary law), so the
	 *  host answers. Default false: a host that wires nothing keeps friendly hearing
	 *  fully dropped — exactly the pre-AIB17 behavior. */
	virtual bool IsWeaponNoiseTag(FName Tag) const { return false; }

	/** Alliance WITHOUT liveness: true iff the game says A and B stand on the same side
	 *  — a dead teammate is still a teammate, a dead enemy is still an enemy. Exists
	 *  because two consumers (the attitude consult, the claims board's binding
	 *  predicate) were caught reading !AreEnemies as "ally", which made every corpse
	 *  everybody's friend for the respawn window (teams W-REVIEW 26 Aug, both critics).
	 *  Defaulted, not pure: a host that wires nothing is FFA, where nobody has allies. */
	virtual bool AreAllies(const AActor* A, const AActor* B) const { return false; }
};
