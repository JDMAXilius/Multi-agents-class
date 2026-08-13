#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "BNHealthComponent.generated.h"

class UAbilitySystemComponent;
class UBNHealthComponent;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_OneParam(FBNDeathSignature, UBNHealthComponent*);

/**
 * Two jobs, and no more than two.
 *
 * 1. Watches Health and says "this one is dead" exactly once. The verdict is UBNGA_Death's and the
 *    respawn is the game mode's. No replicated state of its own: Health already replicates on the
 *    ASC, so every machine reaches zero on its own without a second channel.
 *
 * 2. Runs the shield dance's gate. UBNGE_ShieldRecharge is a dumb periodic adder that does not know
 *    when to run; this watches State.Combat.RecentDamage and removes the recharge while that tag is
 *    up, re-applying it when the tag expires. Authority only — the recharge is an attribute change
 *    and attribute changes are the server's. Clients see the result replicate like any other.
 *
 * The gate lives HERE rather than as OngoingTagRequirements on the GE because those would have to
 * be set during CDO construction, where native tags are not guaranteed registered — see the GE's
 * own comment. This is the same RegisterGameplayTagEvent mechanism UBNAnimInstance already uses.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBNHealthComponent();

	FBNDeathSignature OnDeath;

	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	bool IsDead() const { return bDeathReported; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** RecentDamage arrived or expired: stop the recharge, or start it again. */
	void HandleRecentDamageChanged(const FGameplayTag Tag, int32 NewCount);

	/** Authority only. Idempotent — the handle is what makes a second call a no-op. */
	void SetShieldRechargeActive(bool bActive);

	/** OFF by default, deliberately (founder: "do health component after you're done with
	 *  everything"). The recharge is a real change to how a fight FEELS — shields coming back after
	 *  four seconds rewrites every duel — and it must not arrive unannounced in the middle of a
	 *  playtest aimed at aim, melee and grenades.
	 *
	 *  The structural half of this work is NOT gated and stays live: MaxHealth/MaxShield and their
	 *  clamps are prerequisites, not features. Nothing can draw a health bar without a denominator,
	 *  and an unclamped pool is a bug waiting whether or not anything recharges.
	 *
	 *  Flip to True in DefaultGame.ini under [/Script/BreachpointNext.BNHealthComponent]. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Health")
	bool bShieldRechargeEnabled = false;

	FActiveGameplayEffectHandle ShieldRechargeHandle;

	FDelegateHandle RecentDamageHandle;

	/** Cached, per Wave 2's lesson: the ASC is the PlayerState's and outlives this pawn, so
	 *  EndPlay cannot reach it through a fresh lookup — UnPossessed() nulls PlayerState first. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystem;

	FDelegateHandle HealthChangedHandle;

	/** The fire-once guard. Several writes of zero in one frame are still one death. */
	bool bDeathReported = false;
};
