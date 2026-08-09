// Copyright Zollpa LLC


#include "ZoransLatentPlayerPawn.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "ZoransResistance/AbilitySystem/GameplayAbilitySystemFunctionLibrary.h"
#include "ZoransResistance/AbilitySystem/Attributes/CharacterAttributes.h"
#include "ZoransResistance/AbilitySystem/Attributes/HealthAttributes.h"
#include "ZoransResistance/AbilitySystem/Attributes/MovementAttributes.h"
#include "ZoransResistance/Characters/Data/CharacterData.h"
#include "ZoransResistance/Game/ZoransGameMode.h"
#include "ZoransResistance/Game/ZoransWorldSubsystem.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"

AZoransLatentPlayerPawn::AZoransLatentPlayerPawn()
{
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bCanEverTick = false;
}

void AZoransLatentPlayerPawn::FinishSpawnSequence()
{
	CheckGameState();
}

void AZoransLatentPlayerPawn::BeginPlay()
{
	Super::BeginPlay();
}

void AZoransLatentPlayerPawn::CheckGameState()
{
	if (UZoransWorldSubsystem* ZoransWorldSubsystem = GetWorld()->GetSubsystem<UZoransWorldSubsystem>())
	{
		if (const AZoransGameState* ZoransGameState = ZoransWorldSubsystem->GetZoransGameState())
		{
			SpawnPlayerCharacter(ZoransGameState);
		}
		else
		{
			ZoransWorldSubsystem->OnGameStateReadyDelegate.AddDynamic(this, &ThisClass::SpawnPlayerCharacter);
		}
	}	
}

void AZoransLatentPlayerPawn::SpawnPlayerCharacter(const AZoransGameState* ZoransGameState)
{
	if (GetNetMode() == NM_ListenServer && IsLocallyControlled())
	{
		RequestRespawn();

		return;
	}
	if (GetNetMode() == NM_Client && IsLocallyControlled())
	{
		Server_RequestRespawn();
	}
}

void AZoransLatentPlayerPawn::Server_RequestRespawn_Implementation()
{
	RequestRespawn();
}

void AZoransLatentPlayerPawn::RequestRespawn()
{
	if (AZoransGameMode* ZoransGameMode = Cast<AZoransGameMode>(GetWorld()->GetAuthGameMode()))
	{
		ZoransGameMode->RequestRespawn(GetController(), true, nullptr);
	}
}

void AZoransLatentPlayerPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (const AZoransPlayerState* ZoransPlayerState = Cast<AZoransPlayerState>(GetPlayerState()))
	{
		ensure(CharacterDataTable);
		
		AbilitySystemComponent = ZoransPlayerState->GetAbilitySystemComponent();
		AbilitySystemComponent->InitAbilityActorInfo(GetPlayerState(), this);

		FGrantedAbilitySystemData InitialAttributeSets;
		InitialAttributeSets.GrantedAttributes.Add(UHealthAttributes::StaticClass());
		InitialAttributeSets.GrantedAttributes.Add(UCharacterAttributes::StaticClass());
		InitialAttributeSets.GrantedAttributes.Add(UMovementAttributes::StaticClass());
		
		UGameplayAbilitySystemFunctionLibrary::GrantAbilitySystemData(this, GetPlayerState(), GetPlayerState(), InitialAttributeSets);
			
		if (const AZoransGameMode* GameMode = Cast<AZoransGameMode>(GetWorld()->GetAuthGameMode()))
		{
			UGameplayAbilitySystemFunctionLibrary::GrantAbilitySystemData(this, GetPlayerState(), GetPlayerState(), GameMode->AbilitySystemData);
		}
	}
}