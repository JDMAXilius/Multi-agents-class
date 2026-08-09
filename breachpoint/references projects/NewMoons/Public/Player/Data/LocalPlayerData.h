// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LocalPlayerData.generated.h"

USTRUCT(BlueprintType)
struct FPlayerLoadout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName PrimaryWeapon = NAME_None;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName SecondaryWeapon = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName ThirdWeapon = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName FourthWeapon = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Loadout")
	FName FifthWeapon = NAME_None;
};