// Breachpoint. The ability base: activation policy, generic cooldown, cancel hygiene, one death gate.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BRGameplayAbility.generated.h"

class UBRAbilitySystemComponent;

/**
 * How an ability relates to the input tag that activated it.
 *
 * The ASC activates on the PRESS EDGE for every policy (UBRAbilitySystemComponent::
 * AbilityInputTagPressed) — the policy decides what RELEASE means, which is the only part that
 * differs between a shot and a sprint.
 */
UENUM(BlueprintType)
enum class EBRAbilityActivationPolicy : uint8
{
	/**
	 * Activate on press; the ability decides when it is done. Fire, reload, swap, melee, grapple.
	 * Releasing the key does nothing on its own — an ability that wants the release must ask for
	 * it (WaitInputRelease) because it means something specific to it (a cooked grenade).
	 */
	OnInputPressed,

	/**
	 * Activate on press, END ON RELEASE, automatically. Sprint is the archetype.
	 *
	 * The base class wires this, once, with a WaitInputRelease task — NOT with an override of
	 * InputReleased(). That distinction is load-bearing: on the SERVER, an ability's
	 * InputReleased() is only called when `bReplicateInputDirectly` is set, which costs an RPC per
	 * press for every ability that opts in. WaitInputRelease listens to the replicated generic
	 * input event that our ASC already sends, so the server ends the ability with no extra traffic.
	 * The task is created with bTestAlreadyReleased=true, which closes the tap case: a key
	 * released before activation finished still ends the ability instead of leaving it on forever.
	 */
	WhileInputHeld,

	/**
	 * Activate as soon as it is granted, and never through input. Passive/ambient abilities.
	 * Nothing uses this yet; it is declared because the policy enum is the place that answers
	 * "how does this ability start", and leaving a hole invites a bool somewhere else.
	 */
	OnSpawn
};

/**
 * UBRGameplayAbility — the base every Breachpoint ability derives from (§3.3).
 *
 * WHAT IT GUARANTEES, so that no subclass has to remember it:
 *
 *  1. DEATH BLOCKS EVERY VERB THROUGH ONE MECHANISM. `State.Dead` is in ActivationBlockedTags on
 *     this class, so `GE_Death` disables fire, reload, swap, melee, grenade, grapple and sprint at
 *     once (gas-purity.md law 8). The alternative — an `if (bIsDead)` per ability — is seven
 *     checks that eventually disagree, and the one that is missing is always found in a playtest.
 *
 *  2. COMMIT HAPPENS AT ACTIVATION, unless the subclass says otherwise (`bCommitOnActivate`).
 *     Cost and cooldown are GEs and are applied atomically by CommitAbility, which is what lets
 *     GAS roll BOTH back for free when the server rejects a predicted activation (law 4).
 *
 *  3. ONE COOLDOWN EFFECT FOR THE WHOLE GAME. `GE_Cooldown` is generic; this class injects the
 *     ability's own cooldown tag into the spec and reports it from GetCooldownTags(), so a
 *     per-ability cooldown is a tag plus a number from data, never a new effect class.
 *
 *  4. A PUBLIC EXTERNAL END. See ExternalEndAbility().
 *
 * WHAT IT DELIBERATELY DOES NOT DO: it does not implement a single verb, it spawns no FX (cues
 * only, law 6), and it holds no per-life state. Anything an ability remembers across lives is a
 * bug waiting for a respawn.
 *
 * SUBCLASS CONTRACT: an override of ActivateAbility MUST call Super::ActivateAbility first and
 * return if the ability is no longer active. The base does the commit and the WhileInputHeld
 * wiring there, and skipping it produces an ability that never pays its cost and never ends.
 */
UCLASS(Abstract, meta = (DisplayName = "BR Gameplay Ability"))
class BREACHPOINT_API UBRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBRGameplayAbility(const FObjectInitializer& ObjectInitializer);

	/**
	 * END THIS ABILITY FROM OUTSIDE IT — the hole steps 1-2 named and left open, closed here.
	 *
	 * UE 5.8 exposes no public external end: `UGameplayAbility::EndAbility` and `K2_EndAbility` are
	 * both PROTECTED, and `CancelAbilityHandle` is a *cancel*, which is a different thing with
	 * different consequences (bWasCancelled propagates into every task's cleanup, and cancel-tags
	 * fire). `UBRAbilitySystemComponent::BatchRPCTryActivateAbility(Handle, bEndAbilityImmediately)`
	 * needs exactly this: end the ability inside the RPC batch so activate + TargetData + end
	 * travel as ONE packet per shot.
	 *
	 * Ends with bReplicateEndAbility=true (the other side must learn about it) and
	 * bWasCancelled=false (this is a completion, not an interruption). Safe to call on an ability
	 * that is already ending — the engine's EndAbility is guarded.
	 */
	void ExternalEndAbility();

	/** How this ability relates to its input tag. Set by the subclass constructor. */
	EBRAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	/** The ASC, already typed. Null before actor info is set. */
	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const;

	// -- UGameplayAbility -----------------------------------------------------------------------

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/**
	 * The generic-cooldown half one: report the ability's own cooldown tag so CheckCooldown blocks
	 * on it. Returns {CooldownTag} when one is set, and the base's tags otherwise.
	 */
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	/**
	 * The generic-cooldown half two: build a `GE_Cooldown` spec, inject this ability's cooldown tag
	 * as a DYNAMIC granted tag, and set `SetByCaller.CooldownDuration` from
	 * GetCooldownDurationSeconds().
	 *
	 * REFUSES LOUDLY rather than applying a zero-length cooldown when an ability declares a
	 * cooldown tag and no duration: a zero cooldown that looks applied is an ability with no
	 * cooldown that nobody notices until it is the rocket.
	 */
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/**
	 * The cooldown duration in seconds, resolved at commit time.
	 *
	 * Base returns 0 — "this ability has no cooldown" — and 0 is structural, not tuning. A subclass
	 * with a cooldown overrides this and reads its own data row (`DT_Weapons`) or a `CT_Combat`
	 * curve. A number typed into an ability constructor would be a law-3 violation, and there is no
	 * editor surface to set one on anyway (R18: abilities are C++ classes, not Blueprints).
	 */
	virtual float GetCooldownDurationSeconds() const { return 0.f; }

	/** How this ability relates to its input tag. Subclass constructors set it; nothing else does. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Ability")
	EBRAbilityActivationPolicy ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	/**
	 * The tag `GE_Cooldown` grants while this ability is cooling down, and the tag CheckCooldown
	 * blocks on. Empty means "no cooldown"; set it in the subclass constructor.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Ability")
	FGameplayTag CooldownTag;

	/**
	 * Commit cost + cooldown inside the base's ActivateAbility. True for everything that pays up
	 * front. An ability that must commit later (a charged shot that only pays when it fires) sets
	 * this false in its constructor and calls CommitAbility itself — and is then responsible for
	 * ending itself if the commit fails.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Ability")
	bool bCommitOnActivate = true;

private:
	/** WhileInputHeld's end. Bound to the WaitInputRelease task the base creates. */
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	/** Scratch container for GetCooldownTags(); the engine's contract is a pointer to storage we own. */
	mutable FGameplayTagContainer CooldownTagsScratch;
};
