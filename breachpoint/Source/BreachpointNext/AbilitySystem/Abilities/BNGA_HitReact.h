#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "Data/BNHitReactionSet.h"
#include "Engine/TimerHandle.h"
#include "BNGA_HitReact.generated.h"

/**
 * The flinch. Activated BY CLASS from UBNHealthComponent when health goes down and stays above
 * zero — death's exact shape, one delegate over. Not a gameplay-event trigger, deliberately: a
 * trigger tag is registered in the CDO constructor, and this codebase's proven rule is that native
 * tags are not guaranteed registered there. Every trigger-shaped thing in BN routes around that
 * (state tags on specs, cooldown tags on first use, death by class); this follows.
 *
 * ServerOnly, and the visibility that buys is the point: the montage plays through the ASC, which
 * replicates it to the server's view and every simulated proxy — one activation, every observer
 * sees the same server-picked variant. The montage channel is COND_SkipOwner, so the victim's OWN
 * first-person screen deliberately shows nothing: the camera rides the head bone, and a full-body
 * flinch on your own body is aim punch — a tuning decision that belongs to its own cue later, not
 * a default smuggled in with the third-person reaction. The victim's own feedback is the HUD's
 * damage-direction indicator when the HUD exists; the data for it is already captured.
 *
 * A GA rather than a cue because reactions are about to grow gameplay (stagger, interrupts) that a
 * cue can never hold — and because dead men do not flinch: the base class's State.Dead check
 * refuses activation with zero code here.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNGA_HitReact : public UBNGameplayAbility
{
	GENERATED_BODY()

public:
	UBNGA_HitReact();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** The reaction table — direction × severity → montages, plus the severity thresholds. One
	 *  asset is a character's whole reaction personality; the terminal sets it per ASSET-RULES §7.
	 *  Unset or unloadable degrades to no flinch, silently, like every cue asset. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|HitReaction")
	TSoftObjectPtr<UBNHitReactionSet> ReactionSet;

	FTimerHandle ReactTimer;
};
