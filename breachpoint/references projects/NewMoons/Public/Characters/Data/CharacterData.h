// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterData.generated.h"

USTRUCT(BlueprintType)
struct FSGameplayTag
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool GameplayTag_IsADS = false;
	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool GameplayTag_IsFiring = false;
	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool GameplayTag_IsReloading = false;
	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool GameplayTag_IsDashing = false;
	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool GameplayTag_IsCrouching = false;
	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool GameplayTag_IsMelee = false;
	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool GameplayTag_IsDead = false;
};
