#include "Weapons/BRWeaponPickup.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Data/BRDataRows.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapons/BRWeaponInstance.h"

ABRWeaponPickup::ABRWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicatingMovement(false);

	NetDormancy = DORM_Initial;

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	SetRootComponent(InteractionSphere);
	InteractionSphere->InitSphereRadius(InteractionRadiusCm);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(InteractionSphere);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetGenerateOverlapEvents(false);
}

void ABRWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRWeaponPickup, WeaponRow);
	DOREPLIFETIME(ABRWeaponPickup, AmmoInMag);
	DOREPLIFETIME(ABRWeaponPickup, AmmoReserve);
	DOREPLIFETIME(ABRWeaponPickup, bConsumed);
	DOREPLIFETIME(ABRWeaponPickup, bAttracting);
	DOREPLIFETIME(ABRWeaponPickup, AttractTargetLocation);
	DOREPLIFETIME(ABRWeaponPickup, AttractRequester);
}

void ABRWeaponPickup::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(InteractionRadiusCm);
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABRWeaponPickup::HandleBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ABRWeaponPickup::HandleEndOverlap);

		if (HasAuthority())
		{
			TArray<AActor*> AlreadyOverlapping;
			InteractionSphere->GetOverlappingActors(AlreadyOverlapping, APawn::StaticClass());
			for (AActor* Actor : AlreadyOverlapping)
			{
				if (APawn* Pawn = Cast<APawn>(Actor))
				{
					OverlappingPawns.Add(Pawn);
				}
			}
		}
	}

	if (!WeaponRow.RowName.IsNone())
	{
		RequestMeshLoad();
	}
}

void ABRWeaponPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MeshLoadHandle.IsValid())
	{
		MeshLoadHandle->CancelHandle();
		MeshLoadHandle.Reset();
	}

	OverlappingPawns.Reset();
	OnCollected.Clear();
	OnAttractRequested.Clear();

	Super::EndPlay(EndPlayReason);
}

ABRWeaponPickup* ABRWeaponPickup::SpawnDroppedWeapon(
	UWorld* World,
	TSubclassOf<ABRWeaponPickup> PickupClass,
	const FDataTableRowHandle& InWeaponRow,
	int32 InAmmoInMag,
	int32 InAmmoReserve,
	const FTransform& SpawnTransform,
	AActor* InInstigatorActor)
{
	if (!World)
	{
		return nullptr;
	}

	if (!PickupClass)
	{
		return nullptr;
	}
	if (InWeaponRow.RowName.IsNone() || !InWeaponRow.DataTable)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InInstigatorActor;
	SpawnParams.Instigator = Cast<APawn>(InInstigatorActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ABRWeaponPickup* Pickup = World->SpawnActor<ABRWeaponPickup>(PickupClass, SpawnTransform, SpawnParams);
	if (Pickup)
	{
		Pickup->InitializePickup(InWeaponRow, InAmmoInMag, InAmmoReserve);
	}
	return Pickup;
}

void ABRWeaponPickup::InitializePickup(const FDataTableRowHandle& InWeaponRow, int32 InAmmoInMag, int32 InAmmoReserve)
{
	if (!ensureMsgf(HasAuthority(), TEXT("ABRWeaponPickup::InitializePickup is server-only.")))
	{
		return;
	}

	WeaponRow = InWeaponRow;
	AmmoInMag = FMath::Max(0, InAmmoInMag);
	AmmoReserve = FMath::Max(0, InAmmoReserve);
	FlushNetDormancy();

	RequestMeshLoad();
}

const FBRWeaponRow* ABRWeaponPickup::GetRow() const
{
	if (!WeaponRow.DataTable || WeaponRow.RowName.IsNone())
	{
		return nullptr;
	}
	return WeaponRow.DataTable->FindRow<FBRWeaponRow>(WeaponRow.RowName, TEXT("ABRWeaponPickup"), false);
}

bool ABRWeaponPickup::CanBeCollectedBy(APawn* Collector) const
{
	if (!HasAuthority())
	{
		return false;
	}
	if (bConsumed || !IsValid(Collector))
	{
		return false;
	}
	if (WeaponRow.RowName.IsNone() || !GetRow())
	{
		return false;
	}

	if (!OverlappingPawns.Contains(TWeakObjectPtr<APawn>(Collector)))
	{
		return false;
	}

	if (const UAbilitySystemComponent* ASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Collector))
	{
		if (ASC->HasMatchingGameplayTag(BRGameplayTags::State_Dead))
		{
			return false;
		}
	}

	return true;
}

bool ABRWeaponPickup::TryCollect(APawn* Collector)
{
	if (!ensureMsgf(HasAuthority(), TEXT("ABRWeaponPickup::TryCollect is server-only.")))
	{
		return false;
	}

	if (!CanBeCollectedBy(Collector))
	{
		return false;
	}

	bConsumed = true;
	FlushNetDormancy();

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	OverlappingPawns.Reset();

	OnCollected.Broadcast(this, Collector);

	Destroy();
	return true;
}

bool ABRWeaponPickup::AttractTo(const FVector& InTargetLocation, AActor* InRequester)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (bConsumed)
	{
		return false;
	}

	if (!OnAttractRequested.IsBound())
	{
		return false;
	}

	bAttracting = true;
	AttractTargetLocation = InTargetLocation;
	AttractRequester = InRequester;

	SetNetDormancy(DORM_Awake);
	SetReplicatingMovement(true);
	FlushNetDormancy();

	OnAttractRequested.Broadcast(this, InTargetLocation, InRequester);
	return true;
}

void ABRWeaponPickup::CancelAttract()
{
	if (!HasAuthority() || !bAttracting)
	{
		return;
	}

	bAttracting = false;
	AttractRequester = nullptr;
	SetReplicatingMovement(false);
	FlushNetDormancy();
	SetNetDormancy(DORM_DormantAll);
}

void ABRWeaponPickup::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool,
	const FHitResult&)
{
	if (!HasAuthority())
	{
		return;
	}
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		OverlappingPawns.Add(Pawn);
	}
}

void ABRWeaponPickup::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (!HasAuthority())
	{
		return;
	}
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		OverlappingPawns.Remove(Pawn);
	}
}

void ABRWeaponPickup::OnRep_WeaponRow()
{
	RequestMeshLoad();
}

void ABRWeaponPickup::RequestMeshLoad()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FBRWeaponRow* Row = GetRow();
	if (!Row || Row->MeshSoftPath.IsNull())
	{
		return;
	}

	if (MeshLoadHandle.IsValid())
	{
		MeshLoadHandle->CancelHandle();
		MeshLoadHandle.Reset();
	}

	if (!UAssetManager::IsInitialized())
	{
		return;
	}

	MeshLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Row->MeshSoftPath.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ABRWeaponPickup::OnMeshLoaded));
}

void ABRWeaponPickup::OnMeshLoaded()
{
	const FBRWeaponRow* Row = GetRow();
	if (!Row || !MeshComponent)
	{
		return;
	}

	if (UStaticMesh* Mesh = Row->MeshSoftPath.Get())
	{
		MeshComponent->SetStaticMesh(Mesh);
	}
}

ABRPowerWeaponSpawner::ABRPowerWeaponSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicatingMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ABRPowerWeaponSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRPowerWeaponSpawner, CurrentPickup);
	DOREPLIFETIME(ABRPowerWeaponSpawner, RespawnAvailableServerTime);
	DOREPLIFETIME(ABRPowerWeaponSpawner, bArmed);
}

void ABRPowerWeaponSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bArmOnBeginPlay)
	{
		ArmSpawner();
	}
}

void ABRPowerWeaponSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ABRPowerWeaponSpawner::ArmSpawner()
{
	if (!ensureMsgf(HasAuthority(), TEXT("ABRPowerWeaponSpawner::ArmSpawner is server-only.")))
	{
		return;
	}

	if (!PickupClass)
	{
		return;
	}
	if (WeaponRow.RowName.IsNone() || !WeaponRow.DataTable)
	{
		return;
	}
	if (RespawnIntervalSeconds <= 0.f)
	{
		return;
	}

	bArmed = true;
	if (!IsPickupAvailable())
	{
		SpawnPickup();
	}
}

void ABRPowerWeaponSpawner::DisarmSpawner()
{
	if (!HasAuthority())
	{
		return;
	}

	bArmed = false;
	RespawnAvailableServerTime = -1.f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}
}

bool ABRPowerWeaponSpawner::IsPickupAvailable() const
{
	return IsValid(CurrentPickup) && !CurrentPickup->IsConsumed();
}

float ABRPowerWeaponSpawner::GetSecondsUntilRespawn() const
{
	if (RespawnAvailableServerTime < 0.f)
	{
		return 0.f;
	}

	const float Now = GetServerTimeSeconds();
	if (Now < 0.f)
	{
		return 0.f;
	}

	return FMath::Max(0.f, RespawnAvailableServerTime - Now);
}

float ABRPowerWeaponSpawner::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : -1.f;
}

void ABRPowerWeaponSpawner::SpawnPickup()
{
	if (!HasAuthority() || !bArmed)
	{
		return;
	}
	if (IsPickupAvailable())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FBRWeaponRow* Row = WeaponRow.DataTable
		? WeaponRow.DataTable->FindRow<FBRWeaponRow>(WeaponRow.RowName, TEXT("ABRPowerWeaponSpawner"), false)
		: nullptr;
	if (!Row)
	{
		return;
	}

	int32 StartMag = 0;
	int32 StartReserve = 0;
	UBRWeaponInstance::CalcInitialAmmo(*Row, StartMag, StartReserve);

	CurrentPickup = ABRWeaponPickup::SpawnDroppedWeapon(
		World, PickupClass, WeaponRow, StartMag, StartReserve, GetActorTransform(), this);

	if (CurrentPickup)
	{
		CurrentPickup->OnCollected.AddUObject(this, &ABRPowerWeaponSpawner::HandlePickupCollected);
		RespawnAvailableServerTime = -1.f;
	}
}

void ABRPowerWeaponSpawner::HandlePickupCollected(ABRWeaponPickup* Pickup, APawn*)
{
	if (!HasAuthority() || Pickup != CurrentPickup)
	{
		return;
	}

	CurrentPickup = nullptr;
	if (!bArmed)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = GetServerTimeSeconds();
	RespawnAvailableServerTime = (Now >= 0.f) ? (Now + RespawnIntervalSeconds) : -1.f;

	World->GetTimerManager().SetTimer(
		RespawnTimerHandle, this, &ABRPowerWeaponSpawner::SpawnPickup, RespawnIntervalSeconds, false);
}

void ABRPowerWeaponSpawner::OnRep_RespawnAvailableServerTime()
{
}
