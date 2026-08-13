#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BNGameplayAbility.h"
#include "Engine/TimerHandle.h"
#include "BNGA_Grenade.generated.h"

class UAnimMontage;

/**
 * Throw a grenade. Input tag Input.Grenade, granted by the PLAYER's startup set rather than the
 * weapon's — a grenade is not a property of the gun you happen to hold, and a set granted by the
 * weapon would revoke the ability mid-throw on a swap (the trap Wave 2 hit with swap itself).
 *
 * ServerOnly, and that is the deliberate difference from Fire and Melee. Those two predict because
 * their feedback is instant and local: a muzzle flash that waits a round trip feels broken. A
 * grenade's feedback is a spawned ACTOR that then flies, bounces and explodes on its own — a
 * client-predicted copy would diverge from the server's within a bounce or two and there is no
 * sanctioned way to reconcile two simulated projectiles. So the throw animation is cosmetic and
 * local, the projectile is the server's alone, and nothing needs un-predicting later.
 *
 * The shape is the reference's, `MyCharacter::OnGrenadeKey` (.cpp:997-1023): the montage plays
 * immediately and the spawn happens after ThrowDelay, in parallel — the grenade leaves the hand
 * when the animation releases it, not on the key press.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNGA_Grenade : public UBNGameplayAbility
{
	GENERATED_BODY()

public:
	UBNGA_Grenade();

protected:
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Authority only: the actual spawn, once the throw animation has released. */
	void SpawnGrenade();

	/** Soft CLASS ref, so no Blueprint type is named in C++ and nothing loads until first throw.
	 *  The reference hardcoded BP_FPST_Grenade in the graph (MyCharacter.h:267-269); this is the
	 *  same actor, reached through config. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grenade")
	TSoftClassPtr<AActor> GrenadeClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grenade")
	TSoftObjectPtr<UAnimMontage> ThrowMontage;

	/** Measured from the reference: 0.2s between the key and the spawn (MyCharacter.h:403). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grenade")
	float ThrowDelay = 0.2f;

	/** THE rate limit, and it was missing entirely: CommitAbility with no cost and no cooldown
	 *  returns true and spends nothing, so the only bound was ThrowDelay — about five replicated
	 *  90-damage projectiles a second, indefinitely, all server-side. A cooldown GE rather than a
	 *  timer, so GAS predicts and rolls it back for free (purity law 4). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grenade")
	float CooldownDuration = 4.f;

	/** Where it leaves the hand, along the thrower's own axes — the reference's
	 *  forward*50 + up*50 (MyCharacter.cpp:1001), kept as data so it can be nudged to the socket. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grenade")
	float SpawnForwardOffset = 50.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Grenade")
	float SpawnUpOffset = 50.f;

	FTimerHandle ThrowTimer;

	/** Built on first use, not in the constructor — the tag-registration order rule. */
	mutable FGameplayTagContainer CooldownTags;
};
