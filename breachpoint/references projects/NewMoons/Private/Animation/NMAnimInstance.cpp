// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/NMAnimInstance.h"
#include "Characters/NMCharacterBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "Animation/BlendSpace.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimInstance.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
//#include "Animation/AnimExtractContext.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

void UNMAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (!OwningCharacter)
	{
		OwningCharacter = Cast<ANMCharacterBase>(TryGetPawnOwner());
	}
	
	// Get the skeletal mesh component
	SkelMeshComp = GetSkelMeshComponent();

	if (OwningCharacter)
	{
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
		// Bind gameplay tags to booleans and lees performance
		OwningCharacter->OnGameplayTagChanged.AddUObject(this, &UNMAnimInstance::HandleGameplayTagChanged);
		MeshComponent = OwningCharacter->GetMesh();
		/*if (MeshComponent)
		{
			if (IdleAnimation)
			{
				UE_LOG(LogTemp, Log, TEXT("Playing Idle Animation: %s"), *IdleAnimation->GetName());

				// Ensure the AnimationMode is set to AnimationSingleNode
				if (MeshComponent->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
				{
					MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
					UE_LOG(LogTemp, Log, TEXT("AnimationMode set to AnimationSingleNode"));
				}

				// Set the animation and play it
				MeshComponent->SetAnimation(IdleAnimation);
				MeshComponent->Play(true); // Loop the animation
				UE_LOG(LogTemp, Log, TEXT("Idle Animation is playing successfully"));

				// Debug: Verify the first bone's mesh space transform
				if (MeshComponent->GetBoneTransform(0).IsValid())
				{
					const FTransform& RootTransform = MeshComponent->GetBoneTransform(0);
					UE_LOG(LogTemp, Log, TEXT("Root Bone Mesh-Space Transform: Location(%s), Rotation(%s)"),
						   *RootTransform.GetLocation().ToString(),
						   *RootTransform.GetRotation().Rotator().ToString());
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("IdleAnimation is NULL"));
			}
			UE_LOG(LogTemp, Log, TEXT("Initialized UAILAnimInstance: OwningCharacter=%s, MeshComponent=%s"),
				   *OwningCharacter->GetName(), *MeshComponent->GetName());
		}*/
	}
}

void UNMAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter || !OwningMovementComponent)
		return;

	
	/*if (IdleAnimation)
	{
		UE_LOG(LogTemp, Log, TEXT("Playing Idle Animation: %s"), *IdleAnimation->GetName());

		// Ensure the AnimationMode is set to AnimationSingleNode
	if (MeshComponent->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
	{
		MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		UE_LOG(LogTemp, Log, TEXT("AnimationMode set to AnimationSingleNode"));
	}

		// Set the animation and play it
		MeshComponent->SetAnimation(IdleAnimation);
		MeshComponent->Play(true); // Loop the animation
		UE_LOG(LogTemp, Log, TEXT("Idle Animation is playing successfully"));

		// Debug: Verify the first bone's mesh space transform
		if (MeshComponent->GetBoneTransform(0).IsValid())
		{
			const FTransform& RootTransform = MeshComponent->GetBoneTransform(0);
			UE_LOG(LogTemp, Log, TEXT("Root Bone Mesh-Space Transform: Location(%s), Rotation(%s)"),
				   *RootTransform.GetLocation().ToString(),
				   *RootTransform.GetRotation().Rotator().ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("IdleAnimation is NULL"));
	}*/
	//UpdateGameplayTagBooleans();
}

void UNMAnimInstance::UpdateGameplayTagBooleans()
{
	if (!OwningCharacter)
		return;
	
	GameplayTag_IsADS = OwningCharacter->GameplaytoTag.GameplayTag_IsADS;
	GameplayTag_IsFiring = OwningCharacter->GameplaytoTag.GameplayTag_IsFiring;
	GameplayTag_IsReloading = OwningCharacter->GameplaytoTag.GameplayTag_IsReloading;
	GameplayTag_IsDashing = OwningCharacter->GameplaytoTag.GameplayTag_IsDashing;
	GameplayTag_IsMelee = OwningCharacter->GameplaytoTag.GameplayTag_IsMelee;
	GameplayTag_IsDead = OwningCharacter->GameplaytoTag.GameplayTag_IsDead;
}

void UNMAnimInstance::HandleGameplayTagChanged(const FSGameplayTag GameplayTag)
{
	GameplayTag_IsADS = GameplayTag.GameplayTag_IsADS;
	GameplayTag_IsFiring = GameplayTag.GameplayTag_IsFiring;
	GameplayTag_IsReloading = GameplayTag.GameplayTag_IsReloading;
	GameplayTag_IsDashing = GameplayTag.GameplayTag_IsDashing;
	GameplayTag_IsMelee = GameplayTag.GameplayTag_IsMelee;
	GameplayTag_IsDead = GameplayTag.GameplayTag_IsDead;
	PrintBoolToText(GameplayTag_IsFiring);
}

void UNMAnimInstance::PrintBoolToText(bool bValue)
{
	FString BoolAsText = bValue ? TEXT("True") : TEXT("False");
	UE_LOG(LogTemp, Warning, TEXT("Boolean Value: %s"), *BoolAsText);
}