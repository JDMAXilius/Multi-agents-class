#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BNHealthComponent.generated.h"

class UAbilitySystemComponent;
class UBNHealthComponent;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_OneParam(FBNDeathSignature, UBNHealthComponent*);

/**
 * Watches the Health attribute and says "this one is dead" exactly once. That is the whole class:
 * no damage, no mitigation, no respawn — the verdict is UBNGA_Death's and the respawn is the game
 * mode's. Holds no replicated state of its own; Health already replicates on the ASC, so every
 * machine reaches zero on its own and the delegate fires there without a second channel.
 */
UCLASS()
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

	/** Cached, per Wave 2's lesson: the ASC is the PlayerState's and outlives this pawn, so
	 *  EndPlay cannot reach it through a fresh lookup — UnPossessed() nulls PlayerState first. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystem;

	FDelegateHandle HealthChangedHandle;

	/** The fire-once guard. Several writes of zero in one frame are still one death. */
	bool bDeathReported = false;
};
