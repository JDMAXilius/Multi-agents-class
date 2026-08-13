#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BNPlayerController.generated.h"

class UBNAbilitySystemComponent;
class UBNInputConfig;
class UInputMappingContext;
struct FInputActionValue;

UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

	// The controller's only asset references, and the only place input assets are named.
	// Soft + Config, so DefaultGame.ini sets them and no Blueprint child is needed.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UBNInputConfig> InputConfig;

	// Added at priority = index: ini order is context order.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TArray<TSoftObjectPtr<UInputMappingContext>> MappingContexts;

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpPressed();
	void HandleJumpReleased();
	void HandleCrouchPressed();
	void HandleCrouchReleased();
	void HandleWeaponNextPressed();
	void HandleWeaponPreviousPressed();
	void HandleFirePressed();
	void HandleFireReleased();
	void HandleReloadPressed();
	void HandleSprintPressed();
	void HandleSprintReleased();
	void HandleLeanLeftPressed();
	void HandleLeanLeftReleased();
	void HandleLeanRightPressed();
	void HandleLeanRightReleased();

	UBNAbilitySystemComponent* GetBNAbilitySystemComponent() const;
};
