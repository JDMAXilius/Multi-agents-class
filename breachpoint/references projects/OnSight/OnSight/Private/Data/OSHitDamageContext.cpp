#include "Data/OSHitDamageContext.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

APlayerState* FOSDeathEventInfo::GetInstigatorPlayerState(const UWorld* World) const
{
	if (InstigatorPS) return InstigatorPS;
	if (!World || !InstigatorPlayerStateUniqueId.IsValid()) return nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APlayerState* PS = PC->PlayerState)
			{
				if (PS->GetUniqueId() == InstigatorPlayerStateUniqueId) return PS;
			}
		}
	}
	return nullptr;
}

APlayerState* FOSDeathEventInfo::GetVictimPlayerState(const UWorld* World) const
{
	if (VictimPS) return VictimPS;
	if (!World || !VictimPlayerStateUniqueId.IsValid()) return nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APlayerState* PS = PC->PlayerState)
			{
				if (PS->GetUniqueId() == VictimPlayerStateUniqueId) return PS;
			}
		}
	}
	return nullptr;
}

AController* FOSDeathEventInfo::GetInstigatorController(const UWorld* World) const
{
	if (APlayerState* PS = GetInstigatorPlayerState(World)) return PS->GetPlayerController();
	return nullptr;
}
