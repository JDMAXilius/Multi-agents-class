#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "BNLAnimInstance.generated.h"

class ABNCharacter;
class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * Lyra / NewMoons test parent. Publishes the locomotion fields and GameplayTag_Is* bools that
 * `ABP_Mannequin_Base` (MigrateLyra) already binds. `UBNAnimInstance` stays the production spine.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNLAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

protected:
	void ResolveOwner();
	void BindAbilitySystem();
	void UnbindAbilitySystem();

	UFUNCTION()
	void OnTagChanged(const FGameplayTag Tag, int32 NewCount);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character")
	TObjectPtr<ABNCharacter> OwningCharacter;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character")
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> OwningMovementComponent;

	UPROPERTY(BlueprintReadWrite, Category = "Character State Data")
	float GroundDistance = -1.f;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Locomotion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Locomotion")
	FVector LocalVelocity2D = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Locomotion")
	double DisplacementSpeed = 0.0;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Locomotion")
	bool HasVelocity = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Locomotion")
	bool HasAcceleration = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Air")
	bool IsFalling = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Air")
	bool IsOnGround = true;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Crouch")
	bool isCrouching = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsADS = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsFiring = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsDashing = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsCrouching = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsMelee = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsDead = false;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	struct FTagBool
	{
		FGameplayTag Tag;
		bool* Value = nullptr;
		FDelegateHandle Handle;
	};
	TArray<FTagBool> TagBools;

	FVector CachedVelocity = FVector::ZeroVector;
	FVector CachedAcceleration = FVector::ZeroVector;
	FRotator CachedActorRotation = FRotator::ZeroRotator;
	float CachedGroundDistance = -1.f;
	bool bCachedFalling = false;
	bool bCachedOnGround = true;
	bool bCachedCrouching = false;
	bool bAbilitySystemBound = false;
};
