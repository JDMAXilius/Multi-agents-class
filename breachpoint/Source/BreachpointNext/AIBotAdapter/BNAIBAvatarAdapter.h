#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "BNAIBAvatarAdapter.generated.h"

class ABNCharacter;
class ABNWeapon;
class UBNAbilitySystemComponent;

/**
 * THE ADAPTER — BreachpointNext's implementation of the AIBot module's one avatar door.
 * The whole game-side footprint of the bot framework lives in this folder; if it ever
 * grows past it, self-containment is broken and that is a finding (AIBOT-ROADMAP).
 *
 * Presses land on the SAME path a human's keyboard reaches: the PlayerState's ASC via
 * AbilityInputTagPressed — the 25 Aug seam audit's proven chain. The verb->input-tag map
 * lives here because the module must not know the game's tag names (there is no
 * "Input.Fire"; ours is "Input.Weapon.Fire"). The server-only gate on presses lives here
 * too, per the same audit: the ASC does not gate, controllers do.
 *
 * Added to the pawn at possession by EnsureOn (one line in ABNCharacter::PossessedBy),
 * only when the possessing controller is an AIBot controller — humans never carry it.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNAIBAvatarAdapter : public UActorComponent, public IAIBAvatarInterface
{
	GENERATED_BODY()

public:
	/** Idempotent: creates and registers the adapter on Character when Possessor is an
	 *  AIBot controller and none exists yet. The one wiring call the game makes. */
	static void EnsureOn(ABNCharacter* Character, AController* Possessor);

	// -- IAIBAvatarInterface: verbs ------------------------------------------------
	virtual void PressVerb(FGameplayTag VerbTag) override;
	virtual void ReleaseVerb(FGameplayTag VerbTag) override;

	// -- IAIBAvatarInterface: self reads --------------------------------------------
	virtual float GetHealthNorm() const override;
	virtual float GetAmmoNorm() const override;
	virtual bool HasReserveAmmo() const override;
	virtual bool CanWeaponFight() const override;
	virtual int32 GetGrenadeCount() const override;
	virtual bool IsGrounded() const override;
	virtual bool IsAlive() const override;

	// -- IAIBAvatarInterface: reads about another avatar ---------------------------
	virtual float GetHealthNormOf(const AActor* Other) const override;
	virtual bool IsAliveTarget(const AActor* Other) const override;

private:
	/** AIBot.Verb.* -> the game's Input.* tag; invalid when unmapped (no-op, one log). */
	static FGameplayTag MapVerb(FGameplayTag VerbTag);

	UBNAbilitySystemComponent* GetASC() const;
	ABNWeapon* GetHeldWeapon() const;

	/** THE HAND IS EMPTY ON SPAWN — the fact that stranded every AIB bot on 25 Aug.
	 *  The carry's index 0 is the null Unarmed slot (DefaultGame.ini: "list order is
	 *  switch order and index 0 is equipped on spawn"), so a pawn owns four weapons and
	 *  HOLDS none until something presses Input.Weapon.Next: a human's mouse wheel, or
	 *  BN's own bots' Arm state (BNBotAuthoring.cpp). The AIB brain has no weapon
	 *  vocabulary before Phase 6, and "this game spawns you unarmed" is game knowledge
	 *  the module must never carry — so the door presses the same button for it, which
	 *  is the door's job: present an avatar that CAN fight.
	 *
	 *  A TIMER, not one press: BNGA_Equip does not set bIgnoreMatchFreeze, so every
	 *  press during warmup is refused by UBNGameplayAbility::CanActivateAbility. It
	 *  clears itself the first tick the hand is full. */
	void BeginArming();
	void ArmIfEmptyHanded();

	FTimerHandle ArmTimer;
	int32 ArmPresses = 0;
};
