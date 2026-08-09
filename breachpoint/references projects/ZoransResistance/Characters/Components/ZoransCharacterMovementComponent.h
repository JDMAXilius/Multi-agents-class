// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZoransCharacterMovementComponent.generated.h"

class UMovementAttributes;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMovementSpeedChanged, float, NewValue);

namespace ZoransMovementCVars
{
	// Is Async Character Movement enabled?
	extern ENGINE_API int32 AsyncCharacterMovement;
	extern int32 ForceJumpPeakSubstep;
}

UCLASS()
class ZORANSRESISTANCE_API UZoransCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
		
	UZoransCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	void BindMovementAttributes(UAbilitySystemComponent* InAbilitySystemComponent);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Movement")
	float GetWeightModifier(const bool bClamped = false) const { return bClamped ? FMath::Clamp<float>(CurrentWeightModifier, 0.f, CurrentWeightModifier) : CurrentWeightModifier; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Movement")
	float GetJumpModifier() const { return CurrentJumpModifier; }

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement")
	bool bUseStaticGravity = true;

	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FOnMovementSpeedChanged OnMovementSpeedChangedDelegate;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void ResetMovementValues();

protected:

	TSoftObjectPtr<UMovementAttributes> MovementAttributes;

	UMovementAttributes* GetMovementAttributes();

	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;

	void Internal_OnMovementSpeedChanged(const FOnAttributeChangeData& Data);

	void Internal_RecalculateMovementValues();

	float CalculateMovementSpeed();
	void WeightValueChanged(const FOnAttributeChangeData& Data);
	void JumpValueChanged(const FOnAttributeChangeData& Data);

	float CalculateJumpZ() const;
	
	float CurrentWeightModifier = 1.f;
	float CurrentJumpModifier = 1.f;

private:
	static inline float StaticMovementSpeed = 800.f;
	static inline float StaticJumpStrength = 1000.f;
	static inline float StaticGravityScale = 1.5f;
	static inline float StaticAirControl = 0.1f;
	static inline float StaticFallingLateralFriction = 1.f;
};