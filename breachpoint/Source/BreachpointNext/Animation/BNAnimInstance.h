#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "BNAnimInstance.generated.h"

class ABNCharacter;
class UCharacterMovementComponent;
class UAbilitySystemComponent;

UCLASS()
class BREACHPOINTNEXT_API UBNAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "BN")
	FVector LocalVelocity2D = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "BN")
	double DisplacementSpeed = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "BN")
	double LocalVelocityDirectionAngle = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "BN")
	bool HasVelocity = false;

	UPROPERTY(BlueprintReadOnly, Category = "BN")
	bool HasAcceleration = false;

	UPROPERTY(BlueprintReadOnly, Category = "BN")
	bool IsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "BN")
	bool IsOnGround = true;

	UPROPERTY(BlueprintReadOnly, Category = "BN")
	bool IsJumping = false;

	UPROPERTY(BlueprintReadOnly, Category = "BN")
	bool isCrouching = false;

private:
	void ResolveAbilitySystem();
	void RefreshAnimLayer();
	void OnTagChanged(const FGameplayTag Tag, int32 NewCount);

	UPROPERTY(Transient)
	TObjectPtr<ABNCharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(Transient)
	TObjectPtr<UClass> LinkedLayerClass;

	FDelegateHandle CrouchTagHandle;
	FDelegateHandle JumpTagHandle;
	FDelegateHandle InAirTagHandle;

	bool bTagInAir = false;
	bool bTagCrouching = false;
	bool bTagJumping = false;

	FVector SnapVelocity2D = FVector::ZeroVector;
	FRotator SnapRotation = FRotator::ZeroRotator;
	bool bSnapHasAcceleration = false;
	bool bSnapFalling = false;
	bool bSnapInAirTag = false;
	bool bSnapCrouching = false;
	bool bSnapJumping = false;
};
