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

	/** False while MaxShield is 0 — the shields-off configuration. Everything shield-shaped asks
	 *  this first, so turning shields back on is one number in UBNGE_InitAttributes and nothing
	 *  else. Health is untouched by it either way. */
	bool HasShieldPool() const;

	/** Authority only. Idempotent — the handle is what makes a second call a no-op. */
	void SetShieldRechargeActive(bool bActive);

	/** HEALTH REGEN (founder, 27 Aug). The shield dance's twin with TWO gate tags: the
	 *  regen runs only while State.Combat.HealthRegenDelay AND State.Dead are both
	 *  absent. The dead half is the load-bearing difference from shields — see the GE's
	 *  header for the corpse-resurrection hazard it closes. */
	void HandleHealthRegenGateChanged(const FGameplayTag Tag, int32 NewCount);

	/** Authority only. Idempotent — the handle is the guard, like the recharge's. */
	void SetHealthRegenActive(bool bActive);

	/** Recomputed from the two gate tags; the one truth for "may heal right now". */
	bool ShouldHealthRegenRun() const;

	/** The shield's twin truth (shields ON, 27 Aug): window absent AND not dead. The
	 *  dead half was tolerable while MaxShield was 0 — a live pool makes a recharging
	 *  corpse a real edge (the window can expire before the respawn destroys the body,
	 *  and knife-edge timing between the two numbers is the N1 lesson, not a guard). */
	bool ShouldShieldRechargeRun() const;

	/** ON, as of the founder's go-ahead. It was gated off for one commit while the rest of R3
	 *  landed, because the recharge is a real change to how a fight FEELS — shields returning after
	 *  2.5s rewrites every duel — and that should not arrive unannounced mid-playtest.
	 *
	 *  Kept as a switch rather than hardcoded: it is the fastest way to answer "is the recharge
	 *  making this fight strange?" without a rebuild. `bShieldRechargeEnabled=False` in
	 *  DefaultGame.ini under [/Script/BreachpointNext.BNHealthComponent] turns the dance off. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Health")
	bool bShieldRechargeEnabled = true;

	/** Same contract as the shield switch: the fastest "is the regen making this fight
	 *  strange?" answer without a rebuild. `bHealthRegenEnabled=False` in DefaultGame.ini
	 *  under [/Script/BreachpointNext.BNHealthComponent] turns healing off. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Health")
	bool bHealthRegenEnabled = true;

	FActiveGameplayEffectHandle ShieldRechargeHandle;

	FActiveGameplayEffectHandle HealthRegenHandle;

	FDelegateHandle HealthRegenDelayHandle;

	FDelegateHandle DeadTagHandle;

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
