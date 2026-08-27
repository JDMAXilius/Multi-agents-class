#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "BNGA_Grapple.generated.h"

class UBNCharacterMovementComponent;

/**
 * BN23 — the Grappleshot, SELF-PULL ONLY (founder discussion, 27 Aug: first cut walls the
 * Gantry open; WeaponAttract is the safe second; PawnReel awaits its own ruling).
 * LocalPredicted, input tag Input.Grapple, granted by the PlayerState as a body verb
 * (the Melee/Grenade C++ grant, so no ability-set asset edit gates it).
 *
 * TRANSCRIBED from the legacy module's compiled UBRGA_Grapple, SelfPull slice: each side
 * traces its OWN eyes-forward line (the client's is the prediction, the server's the
 * truth), the AUTHORITY re-validates range and line-of-sight before anything moves, and
 * the pull itself is UBNCharacterMovementComponent::StartGrapplePull — a root motion
 * source riding the saved-move pipeline. The ability DECIDES; the component MOVES.
 *
 * ONE deliberate delta from the BR original, dated: on a successful start this ability
 * ENDS immediately and the pull's lifecycle (arrival, jump-cancel, timeout) is entirely
 * the component's — the BR version stayed active with nothing recorded to end it. Only a
 * CANCELLED end (death, the respawn sweep) stops a running pull, so dying mid-rope drops
 * the rope while a clean handoff never yanks back a pull it just started.
 *
 * Rejection leaves zero state: the cooldown is a predicted GE committed only after the
 * trace and the authority's validation both pass, so a whiffed press costs nothing and a
 * server-rejected one rolls back with the prediction window.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNGA_Grapple : public UBNGameplayAbility
{
	GENERATED_BODY()

public:
	UBNGA_Grapple();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	/** Eyes-forward line trace out to MaxRangeUU. Both sides run it independently. */
	bool TraceForTarget(FHitResult& OutHit) const;

	/** The authority's re-check of a hit BOTH sides computed: range from the SERVER's
	 *  viewpoint, and an unobstructed line to the point. */
	bool ValidateTarget(const FHitResult& Hit, FString& OutReason) const;

	UBNCharacterMovementComponent* GetBNMovement() const;

	/** How far the hook reaches. 2200uu covers the Gantry from the mid deck with margin. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grapple")
	float MaxRangeUU = 2200.f;

	/** Seconds between grapples. The Halo cadence: mobility, not flight. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grapple")
	float CooldownDuration = 4.f;

private:
	mutable FGameplayTagContainer CooldownTags;
};
