#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Templates/SubclassOf.h"
#include "BNPlayerState.generated.h"

class UBNAbilitySet;
class UBNAbilitySystemComponent;
class UBNAttributeSet;
class UGameplayEffect;

UCLASS()
class BREACHPOINTNEXT_API ABNPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABNPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UBNAbilitySystemComponent* GetBNAbilitySystemComponent() const { return AbilitySystemComponent; }
	UBNAttributeSet* GetAttributeSet() const { return AttributeSet; }

	void GrantDefaults();

	/** The ONE way attributes reach their starting numbers — first life and every respawn.
	 *  Authority only. Nothing anywhere hand-sets Health, Shield or MoveSpeed. */
	void ApplyInitAttributes();

protected:
	UPROPERTY()
	TObjectPtr<UBNAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UBNAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UBNAbilitySet> DefaultAbilitySet;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> InitEffect;

	bool bDefaultsGranted = false;
};
