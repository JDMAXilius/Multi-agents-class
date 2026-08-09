// Copyright Zollpa LLC


#include "ZoransGameModeLogic.h"

#include "ZoransGameState.h"

void UZoransGameModeLogic::OnKill_Implementation(AZoransPlayerState* KillerPlayerState, AZoransPlayerState* KilledPlayerState, FTransform KilledTransform, AActor* DeathCauseActor){}

void UZoransGameModeLogic::InitLoadout_Implementation(const AZoransPlayerState* ZoransPlayerState, FName& FirstWeapon, FName& SecondWeapon)
{
	FirstWeapon = FName("Unarmed");
	SecondWeapon = FName("Unarmed");
}

void UZoransGameModeLogic::OnGamePhaseChange_Implementation(EGamePhase NewPhase){}
