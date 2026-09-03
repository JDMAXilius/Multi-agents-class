#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BNGameplayAbility.generated.h"

UCLASS()
class BREACHPOINTNEXT_API UBNGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBNGameplayAbility();

	/** Public because the ASC asks it from outside when the match freezes: the freeze has to cancel
	 *  what is already running, and it must not cancel the abilities that ignore it. */
	bool IgnoresMatchFreeze() const { return bIgnoreMatchFreeze; }

	/** Called on the CDO by the ASC when an input press could NOT activate this ability, after the
	 *  ability-list lock has been released. The refusal is the only place some answers exist:
	 *  CanActivateAbility runs CheckCost, so "the trigger was pulled on an empty magazine" never
	 *  reaches ActivateAbility at all — it is refused upstream and, without this, silently.
	 *  Default does nothing; an override must be cheap, must not assume it is instanced, and must
	 *  not re-enter the ability list synchronously. */
	virtual void OnActivationRefused(const FGameplayAbilityActorInfo* ActorInfo) const {}

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** The match freeze exists to stop players ACTING — shooting, swinging, throwing. It is not
	 *  meant to gag the abilities the game itself fires in reaction to something that already
	 *  happened: a grenade thrown before the buzzer still kills, and a body that takes that damage
	 *  must still die and still flinch. Without this, such a player stands at zero health, alive
	 *  and un-ragdolled, until the restart quietly rebodies them. Death and hit-react set it; no
	 *  input-driven ability ever should. */
	UPROPERTY(EditDefaultsOnly, Category = "BN")
	bool bIgnoreMatchFreeze = false;

	FActiveGameplayEffectHandle ApplyStateTag(FGameplayTag Tag);
	void RemoveStateTag(FActiveGameplayEffectHandle& Handle);
};
