#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BNPlayerController.generated.h"

class UBNAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class BREACHPOINTNEXT_API ABNPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpPressed();
	void HandleJumpReleased();
	void HandleCrouchPressed();
	void HandleCrouchReleased();

	UBNAbilitySystemComponent* GetBNAbilitySystemComponent() const;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY()
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY()
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
