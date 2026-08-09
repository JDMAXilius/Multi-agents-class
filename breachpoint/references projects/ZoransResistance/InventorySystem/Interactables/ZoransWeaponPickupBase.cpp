// Copyright Zollpa LLC.


#include "ZoransWeaponPickupBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/InventorySystem/Weapons/Data/ZoransWeaponData.h"

AZoransWeaponPickupBase::AZoransWeaponPickupBase()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	RootComponent = SceneRoot;
	
	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlatformMesh->SetupAttachment(RootComponent);

	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	WeaponRoot->SetupAttachment(RootComponent);
	
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(WeaponRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	InteractCollisionShape = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractCollisionShape"));
	InteractCollisionShape->SetupAttachment(RootComponent);
	InteractCollisionShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AZoransWeaponPickupBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransWeaponPickupBase, CooldownEndTime, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransWeaponPickupBase, Weaponname, COND_None, REPNOTIFY_Always);
}

UStaticMeshComponent* AZoransWeaponPickupBase::GetPlatformMesh() const
{
	return PlatformMesh.Get();
}

USkeletalMeshComponent* AZoransWeaponPickupBase::GetWeaponMesh() const
{
	return WeaponMesh.Get();
}

void AZoransWeaponPickupBase::StartCooldownTimer()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		if(IsRandomize)
		{
			SetRandomWeapon();
		}
		
		GetWorldTimerManager().SetTimer(CooldownTimerHandle, this, &AZoransWeaponPickupBase::OnCooldownTimerExpired, CooldownTime);

		const AGameStateBase* GameState = GetWorld()->GetGameState();
		
		CooldownEndTime = GameState->GetServerWorldTimeSeconds() + CooldownTime;
		
		OnRep_CooldownEndTime();
	}
}

float AZoransWeaponPickupBase::GetCooldownTimeRemaining() const
{
	const float CurrentServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

	return CooldownEndTime - CurrentServerTime;
}

void AZoransWeaponPickupBase::SetRandomWeapon()
{
	                         
	TArray<FName> RowNames = WeaponRowHandle.DataTable->GetRowNames();

	// Filter out the rows based on special weapon status
	TArray<FName> AvailableRows;

	for (FName RowName : RowNames)
	{
		const FZoransWeaponData* RowData = WeaponRowHandle.DataTable->FindRow<FZoransWeaponData>(RowName, "None");
		if (RowData->bCanBeRandomizedOnPickup)
		{
			if (RowData && RowData->bisSpecial == IsSpecialWeapon )
			{
				AvailableRows.Add(RowName);
			}
		}
	}
	// Pick a random row from the available rows
	if (AvailableRows.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, AvailableRows.Num() - 1);
		//not selecting same index in a row
		if (OldRandomIndex == RandomIndex)
		{
			if (AvailableRows.Num() > 1)
				if (RandomIndex > 0)
				RandomIndex --;
				else
					RandomIndex = AvailableRows.Num()- 1;
			else
				RandomIndex ++;
		}
		OldRandomIndex = RandomIndex;
		if (AvailableRows.IsValidIndex(RandomIndex))
		{
			WeaponRowHandle.RowName = AvailableRows[RandomIndex];//replicate name result
			
		}
		else
		{
			// If no available rows for the chosen category, set it to a default value
			WeaponRowHandle.RowName = "Pistol";
		}
		const FZoransWeaponData* RowData = WeaponRowHandle.DataTable->FindRow<FZoransWeaponData>(WeaponRowHandle.RowName, "None");
		WeaponMesh->SetSkeletalMesh(RowData->SkeletalMesh);
	}
	else
	{
		// If no available rows for the chosen category, set it to a default value
		WeaponRowHandle.RowName = "Pistol";
	}

	Weaponname = WeaponRowHandle.RowName;
	OnRep_Weaponname();
}

void AZoransWeaponPickupBase::BeginPlay()
{
	if (CooldownTime < 0.f)
	{
		if (const FZoransWeaponData *WeaponData = WeaponRowHandle.DataTable->FindRow<FZoransWeaponData>(WeaponRowHandle.RowName, "", false))
		{
			CooldownTime = WeaponData->PickupCooldown;
		}
		else
		{
			CooldownTime = 10.f;
		}
	}

	Super::BeginPlay();
	

	if (GetLocalRole() == ROLE_Authority && bStartWithCooldownEnabled)
	{
		StartCooldownTimer();
	}
}

void AZoransWeaponPickupBase::OnRep_CooldownEndTime()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		const float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		

		if (CooldownEndTime > CurrentTime)
		{
			if (CooldownEndTime > CurrentTime)
			{
				OnCooldownStarted();
			}
		}
		else
		{
			OnCooldownEnded();
		}
	}
}

void AZoransWeaponPickupBase::OnRep_Weaponname()
{
	OnWeaponnameChange();
}

void AZoransWeaponPickupBase::OnCooldownTimerExpired()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		CooldownEndTime = 0.0f;

		OnRep_CooldownEndTime();
	}
}
