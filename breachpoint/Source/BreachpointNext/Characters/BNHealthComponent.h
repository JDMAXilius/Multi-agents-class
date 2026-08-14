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

	/** Shield crossed zero in either direction: raise or clear State.Shields.Broken. */
	void HandleShieldChanged(const FOnAttributeChangeData& Data);

	/** Authority only. Idempotent, like the recharge — the handle is the guard. */
	void SetShieldsBroken(bool bBroken);

	/** Authority only. Idempotent — the handle is what makes a second call a no-op. */
	void SetShieldRechargeActive(bool bActive);

	/** ON, as of the founder's go-ahead. It was gated off for one commit while the rest of R3
	 *  landed, because the recharge is a real change to how a fight FEELS — shields returning after
	 *  2.5s rewrites every duel — and that should not arrive unannounced mid-playtest.
	 *
	 *  Kept as a switch rather than hardcoded: it is the fastest way to answer "is the recharge
	 *  making this fight strange?" without a rebuild. `bShieldRechargeEnabled=False` in
	 *  DefaultGame.ini under [/Script/BreachpointNext.BNHealthComponent] turns the dance off. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Health")
	bool bShieldRechargeEnabled = true;

	FActiveGameplayEffectHandle ShieldRechargeHandle;

	FActiveGameplayEffectHandle ShieldBrokenHandle;

	FDelegateHandle RecentDamageHandle;

	FDelegateHandle ShieldChangedHandle;

	/** Cached, per Wave 2's lesson: the ASC is the PlayerState's and outlives this pawn, so
	 *  EndPlay cannot reach it through a fresh lookup — UnPossessed() nulls PlayerState first. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystem;

	FDelegateHandle HealthChangedHandle;

	/** The fire-once guard. Several writes of zero in one frame are still one death. */
	bool bDeathReported = false;
};
