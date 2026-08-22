#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/BNADSCameraBlend.h"
#include "GameplayTagContainer.h"
#include "BNLAnimInstance.generated.h"

class ABNCharacter;
class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * THE PRODUCTION ANIM SPINE, by the founder's ruling of 22 Aug 2026: Lyra locomotion only.
 * Publishes the locomotion fields and GameplayTag_Is* bools that `ABP_Mannequin_Base`
 * (MigrateLyra) already binds, and that ABP is what the pawn actually runs.
 *
 * THE SENTENCE THAT USED TO BE HERE SAID `UBNAnimInstance` WAS THE SPINE. It was wrong, and it
 * cost three aim fixes that landed on an asset the game never loads — the audit measured it
 * live. `UBNAnimInstance` is now dormant: nothing in the character routes to it.
 *
 * AIM lives on the Blueprint side here, not in C++: this ABP declares its own AimPitch/AimYaw,
 * writes them in UpdateAimingData/SetRootYawOffset, and feeds them to RotationOffsetBlendSpace
 * nodes in the linked layer. Do NOT publish a second aim surface from C++ without proving the
 * layer reads it — a duplicate that nothing consumes is how this went wrong the first time.
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

	/** The ADS lens, shared with UBNAnimInstance. This instance is the one ABP_Mannequin_Base
	 *  actually runs, so without this the zoom never happens on the pawn anyone plays. */
	UPROPERTY(EditDefaultsOnly, Category = "BN|Tuning")
	FBNADSCameraBlend ADSCameraBlend;

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
