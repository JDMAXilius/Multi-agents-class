#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "Core/BRCore.h"

#include "BRCharacter.generated.h"

class UBRCharacterMovementComponent;
class UBRInputConfig;
class UCameraComponent;
class UEnhancedInputComponent;
class UInputAction;
class USkeletalMeshComponent;
struct FInputActionValue;

UCLASS(config = Game, meta = (DisplayName = "BR Character"))
class BREACHPOINT_API ABRCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	ABRCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

protected:

	void MoveInput(const FInputActionValue& Value);

	void LookInput(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	USkeletalMeshComponent* GetMesh3P() const { return GetMesh(); }

	void InitializeAbilitySystem();
	UBRCharacterMovementComponent* GetBRCharacterMovement() const;
};
