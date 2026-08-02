#pragma once

#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpecHandle.h"
#include "UObject/SoftObjectPtr.h"

#include "BRPlayerState.generated.h"

class UBRAbilitySet;
class UBRAbilitySystemComponent;
class UBRAttributeSet;

UCLASS(config = Game, meta = (DisplayName = "BR Player State"))
class BREACHPOINT_API ABRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABRPlayerState(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UBRAbilitySystemComponent* GetBRAbilitySystemComponent() const { return AbilitySystemComponent; }

	const UBRAttributeSet* GetBRAttributeSet() const { return AttributeSet; }

	virtual void PostInitializeComponents() override;

	// Server only. Idempotent: a second call while the previous grant is still live is a
	// no-op, so re-running it from every possession cannot stack duplicate specs.
	void GiveStartupLoadout();

	void ClearStartupLoadout();

protected:
	// Soft and ini-pinned: a hard ref would drag every ability class into memory with the
	// PlayerState class.
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Abilities")
	TSoftObjectPtr<UBRAbilitySet> StartupAbilitySet;

private:
	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Abilities")
	TObjectPtr<UBRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UBRAttributeSet> AttributeSet;

	TArray<FGameplayAbilitySpecHandle> StartupAbilityHandles;

	TArray<FActiveGameplayEffectHandle> StartupEffectHandles;
};
