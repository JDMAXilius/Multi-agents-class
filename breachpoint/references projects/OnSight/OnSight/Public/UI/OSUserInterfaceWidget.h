#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OSUserInterfaceWidget.generated.h"

class UOSAbilitySystemComponent;
class UOSAttributeSet;
class AOSPlayerState;

/**
 * Base UI Widget class for OnSight
 * Provides automatic Ability System Component access from PlayerState
 * Reusable for all UI widgets that need GAS integration
 */
UCLASS()
class ONSIGHT_API UOSUserInterfaceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/**
	 * Gets the Ability System Component from the owning PlayerState
	 * Caches the reference for performance
	 * @return The Ability System Component, or nullptr if not available
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OnSight|UI|GAS")
	UOSAbilitySystemComponent* GetOwningAbilitySystemComponent();

	/**
	 * Gets the Attribute Set from the owning PlayerState
	 * @return The Attribute Set, or nullptr if not available
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OnSight|UI|GAS")
	UOSAttributeSet* GetOwningAttributeSet();

	/**
	 * Gets the PlayerState as AOSPlayerState
	 * @return The PlayerState, or nullptr if not available
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OnSight|UI")
	AOSPlayerState* GetOSPlayerState() const;

	/**
	 * Gets the player name from Steam/Online Subsystem
	 * Falls back to "Player Awesome" if Steam name is not available
	 * @return The player name
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OnSight|UI")
	FString GetPlayerName() const;

protected:
	/** Cached reference to the Ability System Component */
	UPROPERTY()
	TWeakObjectPtr<UOSAbilitySystemComponent> CachedAbilitySystemComponent;
};

