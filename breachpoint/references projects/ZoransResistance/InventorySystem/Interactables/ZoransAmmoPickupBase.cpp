// Copyright Zollpa LLC


#include "ZoransAmmoPickupBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/Characters/Components/ComponentInterface.h"
#include "ZoransResistance/Characters/Components/ZoransWeaponComponent.h"


AZoransAmmoPickupBase::AZoransAmmoPickupBase()
{
	bUseClientInteract = false;
	PrimaryActorTick.bCanEverTick = true;

	PickupRoot = CreateDefaultSubobject<USceneComponent>("PickupRoot");
	RootComponent = PickupRoot;

	InteractCollisionShape = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractCollisionShape"));
	InteractCollisionShape->SetupAttachment(RootComponent);
	InteractCollisionShape->SetCollisionObjectType(ECC_INTERACTABLE);
	InteractCollisionShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractCollisionShape->SetCollisionResponseToChannel(ECC_INTERACTABLE, ECR_Overlap);
	InteractCollisionShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AZoransAmmoPickupBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransAmmoPickupBase, CooldownEndTime, COND_None, REPNOTIFY_Always);
}

void AZoransAmmoPickupBase::Interact_Implementation(APawn* InstigatorPawn)
{
	if (InstigatorPawn->GetLocalRole() == ROLE_Authority && bUseServerInteract)
	{
		if (GetCooldownTimeRemaining() > 0.1f) return;
		
		if (const IComponentInterface* ComponentInterface = Cast<IComponentInterface>(InstigatorPawn))
		{
			if (UZoransWeaponComponent* WeaponComponent = ComponentInterface->GetWeaponComponent())
			{
				if (WeaponComponent->GiveAmmoForCurrentWeapon(AmmoRatio))
				{
					StartCooldownTimer();
					On_Interaction(InstigatorPawn, true);
				}
			}
		}
	}
}

void AZoransAmmoPickupBase::StartCooldownTimer()
{
	if (GetNetMode() < NM_Client)
	{
		GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &AZoransAmmoPickupBase::OnCooldownTimerExpired, CooldownTime);

		if (const AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			CooldownEndTime = GameState->GetServerWorldTimeSeconds() + CooldownTime;
			OnRep_CooldownEndTime();
		}
	}
}

float AZoransAmmoPickupBase::GetCooldownTimeRemaining() const
{
	const float CurrentServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	return CooldownEndTime - CurrentServerTime;
}

void AZoransAmmoPickupBase::OnRep_CooldownEndTime()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		const float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

		if (CooldownEndTime > CurrentTime)
		{
			OnCooldownStarted();
		}
		else
		{
			OnCooldownEnded();
		}
	}
}

void AZoransAmmoPickupBase::OnCooldownTimerExpired()
{
	if (GetNetMode() < NM_Client)
	{
		CooldownEndTime = 0.0f;
		OnRep_CooldownEndTime();
	}
}