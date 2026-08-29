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
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNLAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** The owner-aware "is this pawn being told to move?" decision, lifted out of the update so
	 *  it can be proved without a world, a pawn or a controller. See the definition for why the
	 *  two sources are not interchangeable. */
	static bool ComputeHasAcceleration(bool bAIControlled, const FVector& InputAcceleration,
		const FVector& RequestedVelocity);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

protected:
	void ResolveOwner();

	/** Re-reads the controller and refreshes the four owner bools. Game thread only. */
	void ResolveOwnerDriver();

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

	/** "Is this pawn being told to move?" — the gate Lyra's locomotion state machine uses to
	 *  leave Idle. Its SOURCE depends on who is driving; see ResolveOwnerDriver(). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Locomotion")
	bool HasAcceleration = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Air")
	bool IsFalling = false;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Air")
	bool IsOnGround = true;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Crouch")
	bool isCrouching = false;

	/* --- WHO IS DRIVING THIS PAWN ----------------------------------------------------------
	 * Published so the graph can branch: a bot and a player legitimately want different
	 * behaviour out of the same ABP (turn-in-place, first-person-only work, aim sets).
	 *
	 * Re-evaluated EVERY frame, never cached at initialise. Possession is not stable for the
	 * life of an anim instance: pawns are possessed after the mesh already has one, and BN
	 * respawns characters mid-match. A driver resolved once at init is how a respawned bot
	 * ends up animating as though a human were holding the stick.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Owner")
	bool bIsPlayerControlled = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Owner")
	bool bIsAIControlled = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Owner")
	bool bLocallyControlled = false;

	/** Locally controlled AND player controlled — the one pawn that owns a first-person view.
	 *  Named to match UBNAnimInstance's bFPSMode so the two spines read the same. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Owner")
	bool bFPSMode = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsADS = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsFiring = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsDashing = false;

	/** THE REAL DASH (BN29). GameplayTag_IsDashing above is a MISNOMER that predates the
	 *  ability — it is bound to State.Movement.Sprinting and drives the sprint pose, so it
	 *  is deliberately left alone: ABP graphs bind these by NAME and renaming it would break
	 *  the sprint branch silently. This one is bound to the tag the dash actually applies. */
	UPROPERTY(BlueprintReadOnly, Category = "BN|State")
	bool GameplayTag_IsDashingActual = false;

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
		/** Config so the zoom can be tuned by FEEL without a rebuild — the ini is the source of
	 *  truth and beats both this C++ default and anything an ABP asset serialised. Set it as
	 *  ADSCameraBlend=(ADSFOV=60.000000,InterpSpeed=18.000000). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Tuning")
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
	/** Path following's ask, which is the AI's equivalent of a player's input vector. */
	FVector CachedRequestedVelocity = FVector::ZeroVector;
	FRotator CachedActorRotation = FRotator::ZeroRotator;
	float CachedGroundDistance = -1.f;
	bool bCachedFalling = false;
	bool bCachedOnGround = true;
	bool bCachedCrouching = false;
	bool bCachedAIControlled = false;
	bool bAbilitySystemBound = false;
};
