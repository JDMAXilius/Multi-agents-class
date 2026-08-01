// Breachpoint. Sprint: the WhileHeld prover and the first predicted movement state.

#pragma once

#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "CoreMinimal.h"

#include "BRGA_Sprint.generated.h"

class UBRCharacterMovementComponent;

/**
 * UBRGA_Sprint — hold `InputTag.Sprint` to run faster; release to stop.
 *
 * It is the smallest ability in the game and it exists to prove the whole chain end to end:
 *
 *     key -> IMC -> UBRInputComponent -> InputTag -> ABRPlayerController
 *         -> UBRAbilitySystemComponent::AbilityInputTagPressed (press edge)
 *         -> TryActivateAbility (LocalPredicted)
 *         -> ActivationOwnedTags grants State.Movement.Sprinting
 *         -> this ability sets the CMC's sprint intent
 *         -> FSavedMove_BR carries it through prediction/correction/replay
 *         -> UBRCharacterMovementComponent::GetMaxSpeed applies the CT_Combat multiplier
 *
 * NO COST AND NO COOLDOWN, by design (GDD: "Sprint — +20% move speed, no cost"). That is why this
 * is the right ability to prove the base class with: nothing about it can fail for economic
 * reasons, so a failure is a plumbing failure.
 *
 * -------------------------------------------------------------------------------------------
 * THE DIVISION OF LABOUR, which is the movement exception in `gas-purity.md` made concrete:
 *
 *   THIS ABILITY decides WHETHER: it is the thing that can be blocked (`State.Dead`), cancelled
 *   (firing), and predicted. It grants the STATE (`State.Movement.Sprinting`) that the rest of the
 *   game reads — bots, UI, cues, animation.
 *
 *   THE CMC executes the MOTION: the speed multiplier, the saved move, the replay. This ability
 *   never writes a velocity, a location or a speed, and the component never decides whether the
 *   player may sprint.
 *
 * WHY THE ABILITY PUSHES THE INTENT INSTEAD OF THE CMC READING THE TAG. §3.4 says "the CMC reads
 * [the tag] into the FSavedMove_BR sprint flag". It cannot read it *per move*: a tag query returns
 * the tag as it is NOW, and a replayed move must reproduce the intent as it was THEN. So the tag
 * causes the flag once, at activation, exactly the way `ACharacter::Crouch()` sets
 * `bWantsToCrouch` — and the flag, not the tag, is what replays. The tag remains the replicated
 * state that everything else reads. Recorded here because it is the one place this packet's code
 * reads differently from §3.4's sentence, and it is a deliberate correction, not a drift.
 *
 * HOW FIRING ENDS A SPRINT, with zero code in this file: `BRGA_WeaponFire`/`Melee`/`Grenade` list
 * **`Ability.Sprint`** in their CancelAbilitiesWithTag. §3.3 says they list
 * `State.Movement.Sprinting`, and that does not work: CancelAbilitiesWithTag is matched against
 * the target ability's ASSET tags, and `State.Movement.Sprinting` is granted to the ACTOR, not
 * owned by the ability. Hence `Ability.Sprint` (ruling R23, declared in `BRGameplayTags.h`). Filed
 * as a finding against §3.3 rather than silently patched by giving this ability a State tag.
 */
UCLASS(meta = (DisplayName = "BRGA_Sprint"))
class BREACHPOINT_API UBRGA_Sprint : public UBRGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGA_Sprint(const FObjectInitializer& ObjectInitializer);

	/**
	 * The stage gate lives HERE, not in ActivateAbility, and the reason is bots.
	 *
	 * `ABRBotController` builds `FBRBotFacts::bCanGrenade` / `bCanGrapple` by CALLING
	 * `CanActivateAbility` on the granted spec. With the gate one level down in
	 * `ActivateAbility`, a bot below the stage still believes the verb is available, presses it,
	 * and is refused at activation — a bot reasoning from a fact that is false. Declaring this
	 * override makes the bot's picture and the gate's answer the same answer.
	 *
	 * It also refuses BEFORE `PreActivate`, so no prediction key is spent and no
	 * `ServerTryActivateAbility` crosses the wire for an ability the stage has switched off.
	 */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/**
	 * Runs on EVERY end — released, cancelled by firing, blocked by death, interrupted by a
	 * correction. That is why the intent is cleared here and nowhere else: there is exactly one
	 * exit from this ability, so there is exactly one place the sprint can fail to stop.
	 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** The typed CMC of this ability's avatar, or null (a bot's pawn mid-respawn legitimately has none). */
	UBRCharacterMovementComponent* GetBRCharacterMovement(const FGameplayAbilityActorInfo* ActorInfo) const;
};
