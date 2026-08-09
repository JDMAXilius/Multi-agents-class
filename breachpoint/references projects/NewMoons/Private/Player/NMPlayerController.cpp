// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/NMPlayerController.h"
#include "Game/NMGameMode.h"
#include "Kismet/GameplayStatics.h"

void ANMPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!NMGameMode) NMGameMode = Cast<ANMGameMode>(UGameplayStatics::GetGameMode(this));
}
void ANMPlayerController::Server_RespawnCharacter_Implementation()
{
	if (GetNMGameMode())
	{
		GetNMGameMode()->RespawnCharacter(this);	
	}
}
