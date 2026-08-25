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
 * Shaped by the 25 Aug seam audit of the host game, not by memory:
 *  - Verbs land as ASC input-tag presses (PlayerState ASC -> AbilityInputTagPressed ->
 *    TryActivateAbility). The adapter owns the verb->input-tag map (there is no
 *    a generic "Fire" tag in the host — its real names differ, which is why the map exists) and the
 *    server-only gate, which the audit showed lives per-controller, not in the ASC.
 *  - Movement/aim stay ENGINE surface on the controller (MoveTo* rides the player's
 *    ConsumeInputVector road via bUseAccelerationForPaths; aim is SetControlRotation) —
 *    so they are not part of this interface.
 *  - Reads mirror what the host's proven bot consumers call: attribute getters for vitals,
 *    weapon getters for ammo, CMC IsFalling for grounded (the audit's caveat: the InAir
 *    TAG misses a walk-off, so the interface asks the avatar, which asks its CMC).
 *  - "Can the held weapon fight RIGHT NOW" is deliberately one question here, because in
 *    BN it is four reads (ammo + no fire cooldown + no reloading/melee tag + not
 *    dead/frozen) — the adapter assembles them; the brain must not know the recipe.
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
