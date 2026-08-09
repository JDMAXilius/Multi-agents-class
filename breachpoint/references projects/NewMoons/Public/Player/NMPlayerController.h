// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NMPlayerController.generated.h"

class ANMGameMode;

UCLASS()
class NEWMOONS_API ANMPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure)
	FORCEINLINE ANMGameMode* GetNMGameMode() const { return NMGameMode; }
	
	// Temporarily blueprint callable to test respawning
	UFUNCTION(BlueprintCallable, Server, Reliable)
	virtual void Server_RespawnCharacter();

protected:
	virtual void BeginPlay() override;

	TObjectPtr<ANMGameMode> NMGameMode;
	
};
