#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "AIBAvatarInterface.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable, meta = (CannotImplementInterfaceInBlueprint))
class UAIBAvatarInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * THE ONE DOOR between the bot and its body (FAIRPLAY F6). The game implements this in
 * its adapter folder — on a component class named in ini, which AAIBBotController resolves
 * on its pawn at possession. The module never learns what is behind it.
 *
 * Shaped by the 25 Aug seam audit of a real host, not by memory. What generalises:
 *  - The adapter owns the verb->action map (hosts name their actions differently — a
 *    generic "Fire" rarely exists verbatim) and the server-only gate on presses.
 *  - Movement and aim are DELIBERATELY not here: they stay engine surface on the
 *    controller (path-following and control rotation), a documented narrowing of the
 *    roadmap's first interface sketch (AIBOT-ROADMAP is amended; W-REVIEW M3). A host
 *    whose avatar is not an ACharacter supplies its own locomotion adapter at the
 *    controller seam, not here.
 *  - "Can the held weapon fight RIGHT NOW" is ONE question on purpose: in the audited
 *    host it is four separate reads. The adapter assembles the recipe; the brain gets
 *    only the answer.
 *  - Grounded means the avatar's movement truth (a physics/CMC read), never an
 *    animation tag — the audited host's tag provably misses a walk-off.
 */
class IAIBAvatarInterface
{
	GENERATED_BODY()

public:
	// -- verbs out (AIBot.Verb.*; unmapped verb = no-op, logged once) -------------
	virtual void PressVerb(FGameplayTag VerbTag) = 0;
	virtual void ReleaseVerb(FGameplayTag VerbTag) = 0;

	// -- self reads (FAIBFacts' self block) ---------------------------------------
	virtual float GetHealthNorm() const = 0;          // 0..1; 1 when unknowable
	virtual float GetAmmoNorm() const = 0;            // magazine fraction of held weapon
	virtual bool HasReserveAmmo() const = 0;
	virtual bool CanWeaponFight() const = 0;          // the assembled four-read answer
	virtual bool IsGrounded() const = 0;              // CMC truth, not the tag
	virtual bool IsAlive() const = 0;                 // !State.Dead equivalent

	// -- reads about ANOTHER avatar (FAIRPLAY F3: caller must hold a matured
	//    perception of Other before asking; the sensorium enforces that, not this) --
	virtual float GetHealthNormOf(const AActor* Other) const = 0;
	virtual bool IsAliveTarget(const AActor* Other) const = 0;
};
