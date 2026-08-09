// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "Characters/Data/CharacterData.h"
#include "NMAnimInstance.generated.h"

class ANMCharacterBase;
class UCharacterMovementComponent;
class USkeletalMeshComponent;

UCLASS()
class NEWMOONS_API UNMAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	
	ANMCharacterBase* OwningCharacter;
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	USkeletalMeshComponent* MeshComponent;
	UPROPERTY()
	UCharacterMovementComponent* OwningMovementComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	UAnimSequence* IdleAnimation;
	USkeletalMeshComponent* SkelMeshComp;
protected:

	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsADS = false;
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsFiring = false;
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsReloading = false;
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsDashing = false;
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsMelee = false;
	UPROPERTY(BlueprintReadOnly, Category = "GameplayTags")
	bool GameplayTag_IsDead = false;

	// Update boolean properties based on gameplay tags
	void UpdateGameplayTagBooleans();
	
	// Update booleans based and Bind gameplay tags to booleans
	void HandleGameplayTagChanged(const FSGameplayTag GameplayTag);
	void PrintBoolToText(bool bValue);
};
