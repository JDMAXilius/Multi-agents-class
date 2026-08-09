// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Base actor: GetGameMode() helper. Use for level actors that need game mode access.

#include "CoreMinimal.h"
#include "Core/OSGameMode.h"
#include "GameFramework/Actor.h"
#include "OSActor.generated.h"

class AOSGameMode;

UCLASS()
class ONSIGHT_API AOSActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AOSActor();
	
protected:
	AOSGameMode* GetGameMode();
	
	// attempts to get gamemode of specific subclass
	template<typename T>
	T* GetGameModeCasted()
	{

		auto gm = GetGameMode();
		if (!gm || !IsValid(gm)) return nullptr;
		return Cast<T>(gm);
	}
};
