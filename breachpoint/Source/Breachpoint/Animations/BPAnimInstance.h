#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BPAnimInstance.generated.h"

class ABPCharacter;
class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * BP AnimInstance — C++ only. No FGameplayTagBlueprintPropertyMap (that forces ABP rows).
 * Tag→bool wiring is a C++ RegisterGameplayTagEvent table (same engine seam as Lyra,
 * reviewable in git). Locomotion + GroundDistance computed here; no editor graph required
 * for the *logic*. An AnimBP is only needed later if you want a Tier-4 pose graph.
 */
UCLASS(Config = Game)
class BREACHPOINT_API UBPAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	UBPAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Properties | Character State Data")
	float GroundDistance = -1.0f;

	/** Bound in C++ to existing BRGameplayTags — no ABP property map. */
	UPROPERTY(BlueprintReadOnly, Category = "Properties | GameplayTags")
	bool GameplayTag_IsADS = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | GameplayTags")
	bool GameplayTag_IsFiring = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | GameplayTags")
	bool GameplayTag_IsDashing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | GameplayTags")
	bool GameplayTag_IsMelee = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | GameplayTags")
	bool GameplayTag_IsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | GameplayTags")
	bool GameplayTag_IsDead = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | Locomotion")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | Locomotion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | Locomotion")
	bool bIsOnGround = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | Locomotion")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties | Locomotion")
	bool bHasVelocity = false;

private:

	struct FBPAnimTagState
	{
		bool bADS = false;
		bool bFiring = false;
		bool bDashing = false;
		bool bMelee = false;
		bool bReloading = false;
		bool bDead = false;
	};

	struct FBPTagBinding
	{
		FGameplayTag Tag;
		bool FBPAnimTagState::* Field;
	};

	void BindAbilitySystem();
	void UnbindAbilitySystem();
	void OnStateTagChanged(const FGameplayTag Tag, int32 NewCount);
	const TArray<FBPTagBinding>& GetTagBindings() const;

	float ComputeGroundDistance(const ACharacter* Character, const UCharacterMovementComponent* MoveComp) const;

	TWeakObjectPtr<ABPCharacter> OwningCharacter;
	TWeakObjectPtr<UCharacterMovementComponent> OwningMovement;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TArray<FDelegateHandle> TagHandles;

	FBPAnimTagState TagState;

	FVector CachedVelocity = FVector::ZeroVector;
	float CachedGroundDistance = -1.f;
	bool bCachedOnGround = false;
	bool bCachedFalling = false;

	static constexpr float GroundTraceDistance = 100000.f;
};
