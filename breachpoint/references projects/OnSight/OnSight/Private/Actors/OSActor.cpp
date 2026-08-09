// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/OSActor.h"

#include "Core/OSGameMode.h"


// Sets default values
AOSActor::AOSActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

AOSGameMode* AOSActor::GetGameMode()
{
	if (!HasAuthority() || !GetWorld()) return nullptr;
	auto gm = GetWorld()->GetAuthGameMode();
	if (!gm) return nullptr;
	return Cast<AOSGameMode>(gm);
}

