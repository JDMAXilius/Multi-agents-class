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

public:
	ABNPlayerController();

protected:
	virtual void SetupInputComponent() override;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UBNInputConfig> InputConfig;

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
	void HandleADSPressed();
	void HandleADSReleased();
	void HandleMeleePressed();
	void HandleGrenadePressed();

	UBNAbilitySystemComponent* GetBNAbilitySystemComponent() const;

	/** State.Dead replicates, so the machine that reads the input is the machine that refuses it. */
	bool IsDead() const;
};
