// Breachpoint. Weapons lying in the world, and the node that puts the Rocket there.

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

// ===========================================================================================
// ABRWeaponPickup
// ===========================================================================================

ABRWeaponPickup::ABRWeaponPickup()
{
	// Law 4. Nothing here needs a frame.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicatingMovement(false);

	// netcode.md law 4 (minimum replication): a pickup changes state twice in its life
	// (collected, maybe attracted). It sleeps in between. EVERY mutator below is paired with
	// FlushNetDormancy — an un-flushed write to a dormant actor is a silent desync, so if you
	// add a replicated property here, you add the flush with it.
	NetDormancy = DORM_Initial;

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	SetRootComponent(InteractionSphere);
	InteractionSphere->InitSphereRadius(InteractionRadiusCm);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);

	// Cosmetic. It must never be able to answer a gameplay question, so it answers no query.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(InteractionSphere);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetGenerateOverlapEvents(false);
}

void ABRWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// All COND_None: this is world state on a contested object. Compare UBRWeaponInstance,
	// where the same two ammo numbers are COND_OwnerOnly because there they describe what is
	// in an enemy's hands.
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

	// The editable radius is the truth; the constructor's InitSphereRadius only seeds the CDO.
	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(InteractionRadiusCm);
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABRWeaponPickup::HandleBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ABRWeaponPickup::HandleEndOverlap);

		// Seed the set from the overlaps that already exist. Begin-overlap fires at component
		// registration, BEFORE this bind — so a weapon dropped at a player's feet would
		// otherwise be un-collectable by that player until they stepped out and back in.
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

	// Clients get their mesh when the row arrives (OnRep_WeaponRow); the server-spawned path
	// already has it here.
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

	// Refuse, never substitute. A drop with no configured pickup class is a setup defect; a
	// silently-chosen default class would hide it until someone found a floating AR.
	if (!PickupClass)
	{
		UE_LOG(LogBRCombat, Error, TEXT("SpawnDroppedWeapon: no PickupClass supplied; nothing dropped."));
		return nullptr;
	}
	if (InWeaponRow.RowName.IsNone() || !InWeaponRow.DataTable)
	{
		UE_LOG(LogBRCombat, Error, TEXT("SpawnDroppedWeapon: no weapon row supplied; nothing dropped."));
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
	return WeaponRow.DataTable->FindRow<FBRWeaponRow>(WeaponRow.RowName, TEXT("ABRWeaponPickup"), /*bWarnIfRowMissing*/ false);
}

bool ABRWeaponPickup::CanBeCollectedBy(APawn* Collector) const
{
	// Only the server has an opinion worth having. A client asking this question is welcome
	// to a hint for its prompt, but the answer that matters is computed here.
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
		// A pickup bound to a row that no longer exists grants nothing. Refuse rather than
		// hand out an unarmed weapon.
		return false;
	}

	// THE geometric check, and it is the server's own collision, not a client's claim about
	// where it is standing. There is no distance parameter here to spoof.
	if (!OverlappingPawns.Contains(TWeakObjectPtr<APawn>(Collector)))
	{
		return false;
	}

	// A corpse does not pick things up. State is a GE-applied tag (gas-purity.md), so this is
	// a tag query, not a bool on the pawn.
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

	// Re-check rather than trust the caller's earlier check: between their call and ours,
	// another player may have won the race. Two collectors on one Rocket is exactly the case
	// this line exists for.
	if (!CanBeCollectedBy(Collector))
	{
		return false;
	}

	bConsumed = true;
	FlushNetDormancy();

	// Stop answering overlap questions the moment the race is decided.
	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	OverlappingPawns.Reset();

	// The spawner is listening: this is what starts its countdown.
	OnCollected.Broadcast(this, Collector);

	Destroy();
	return true;
}

bool ABRWeaponPickup::AttractTo(const FVector& InTargetLocation, AActor* InRequester)
{
	// BP06 seam. The authority gate is the part BP03 owns and guarantees.
	if (!HasAuthority())
	{
		UE_LOG(LogBRCombat, Warning, TEXT("ABRWeaponPickup::AttractTo called without authority; ignored."));
		return false;
	}
	if (bConsumed)
	{
		return false;
	}

	if (!OnAttractRequested.IsBound())
	{
		// Explicit refusal, not a silent no-op: without a driver the pickup would sit still
		// while the grapple animation played, and the bug would read as "grapple feels bad".
		UE_LOG(LogBRCombat, Error,
			TEXT("ABRWeaponPickup::AttractTo: no attract driver is bound (BP06 owns it). Request refused."));
		return false;
	}

	bAttracting = true;
	AttractTargetLocation = InTargetLocation;
	AttractRequester = InRequester;

	// An actor in flight is not a resting actor: wake it and replicate its movement for the
	// duration, then put it back to sleep in CancelAttract.
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

void ABRWeaponPickup::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	// The set is SERVER truth and exists only there. A client maintaining one would be
	// maintaining an opinion.
	if (!HasAuthority())
	{
		return;
	}
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		OverlappingPawns.Add(Pawn);
	}
}

void ABRWeaponPickup::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
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
	// Cosmetic only (netcode.md law 3): the row arriving is a client's cue to load the mesh.
	RequestMeshLoad();
}

void ABRWeaponPickup::RequestMeshLoad()
{
	// A dedicated server renders nothing and must not pay for a mesh. A listen server does
	// render, so this is a net-mode test, not an authority test.
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

	// SOFT ref resolved through the streamable manager at the load point (data-and-assets.md).
	// The row never holds the mesh; nothing in this class ever hard-references one.
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

// ===========================================================================================
// ABRPowerWeaponSpawner
// ===========================================================================================

ABRPowerWeaponSpawner::ABRPowerWeaponSpawner()
{
	// Law 4: the countdown is a timer plus a replicated deadline. Nothing counts per frame.
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

	// ARCHITECTURE §6.1: the rocket countdown is COND_None. Everyone contests it, so everyone
	// reads the same deadline.
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

	// The three refusals. Each one is a configuration mistake that MUST be visible now, at
	// arm time, rather than 90 seconds into a match as "the rocket never spawned".
	if (!PickupClass)
	{
		UE_LOG(LogBRCombat, Error, TEXT("%s: no PickupClass set; node not armed."), *GetName());
		return;
	}
	if (WeaponRow.RowName.IsNone() || !WeaponRow.DataTable)
	{
		UE_LOG(LogBRCombat, Error, TEXT("%s: no WeaponRow set; node not armed."), *GetName());
		return;
	}
	if (RespawnIntervalSeconds <= 0.f)
	{
		// See the property comment: ruling R4's 90 s belongs in DT_MatchRules, and this packet
		// may not invent that column. An unconfigured node refuses rather than guessing 90.
		UE_LOG(LogBRCombat, Error,
			TEXT("%s: RespawnIntervalSeconds is unset (%.1f). Ruling R4's 90 s must come from data — "
				 "node not armed, and no default was invented."),
			*GetName(), RespawnIntervalSeconds);
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
		// No GameState yet (a client's first frames). "Unknown" reads as 0 to the HUD, which
		// draws nothing — better than drawing a wrong deadline computed from a local clock.
		return 0.f;
	}

	return FMath::Max(0.f, RespawnAvailableServerTime - Now);
}

float ABRPowerWeaponSpawner::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	// GetServerWorldTimeSeconds is a double; the replicated deadline is a float because a
	// match clock does not need 15 digits and bandwidth is budgeted (QUALITY-BARS §2).
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

	// The node spawns a FULL weapon of its row: mag full, reserve per the row (the Rocket's
	// ReserveMags 0 is ruling R4 doing its job, not a missing number).
	const FBRWeaponRow* Row = WeaponRow.DataTable
		? WeaponRow.DataTable->FindRow<FBRWeaponRow>(WeaponRow.RowName, TEXT("ABRPowerWeaponSpawner"), /*bWarnIfRowMissing*/ false)
		: nullptr;
	if (!Row)
	{
		UE_LOG(LogBRCombat, Error, TEXT("%s: row '%s' missing from the table; nothing spawned."),
			*GetName(), *WeaponRow.RowName.ToString());
		return;
	}

	// ONE definition of "a full weapon of this row", shared with the equipment component.
	int32 StartMag = 0;
	int32 StartReserve = 0;
	UBRWeaponInstance::CalcInitialAmmo(*Row, StartMag, StartReserve);

	CurrentPickup = ABRWeaponPickup::SpawnDroppedWeapon(
		World, PickupClass, WeaponRow, StartMag, StartReserve, GetActorTransform(), this);

	if (CurrentPickup)
	{
		CurrentPickup->OnCollected.AddUObject(this, &ABRPowerWeaponSpawner::HandlePickupCollected);
		// A pickup is present: the countdown is not running.
		RespawnAvailableServerTime = -1.f;
	}
	else
	{
		UE_LOG(LogBRCombat, Error, TEXT("%s: pickup spawn failed; node idle until re-armed."), *GetName());
	}
}

void ABRPowerWeaponSpawner::HandlePickupCollected(ABRWeaponPickup* Pickup, APawn* /*Collector*/)
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

	// Publish the DEADLINE, not a counter. One replicated write per cycle; every client
	// computes the same remaining time from it.
	const float Now = GetServerTimeSeconds();
	RespawnAvailableServerTime = (Now >= 0.f) ? (Now + RespawnIntervalSeconds) : -1.f;

	World->GetTimerManager().SetTimer(
		RespawnTimerHandle, this, &ABRPowerWeaponSpawner::SpawnPickup, RespawnIntervalSeconds, /*bLoop*/ false);
}

void ABRPowerWeaponSpawner::OnRep_RespawnAvailableServerTime()
{
	// Cosmetic only: the HUD/GameState mirror refreshes. No gameplay decision is made here,
	// and removing this body changes no outcome.
}
