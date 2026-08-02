#include "Weapons/BREquipmentComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/BRAbilitySet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/BRCore.h"
#include "Data/BRDataRows.h"
#include "Engine/ActorChannel.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/BRWeaponInstance.h"
#include "Weapons/BRWeaponPickup.h"

UBREquipmentComponent::UBREquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;

	Slots.SetNum(static_cast<int32>(EBRWeaponSlot::Count));
	SlotGrants.SetNum(static_cast<int32>(EBRWeaponSlot::Count));

	DroppedPickupClass = ABRWeaponPickup::StaticClass();
}

void UBREquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBREquipmentComponent, Slots);
	DOREPLIFETIME(UBREquipmentComponent, ActiveSlotIndex);
}

void UBREquipmentComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	if (!IsUsingRegisteredSubObjectList())
	{
		return;
	}

	for (const TObjectPtr<UBRWeaponInstance>& Instance : Slots)
	{
		if (IsValid(Instance))
		{
			AddReplicatedSubObject(Instance, COND_None);
		}
	}
}

bool UBREquipmentComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (const TObjectPtr<UBRWeaponInstance>& Instance : Slots)
	{
		if (IsValid(Instance))
		{
			bWroteSomething |= Channel->ReplicateSubobject(Instance, *Bunch, *RepFlags);
		}
	}

	return bWroteSomething;
}

void UBREquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MeshLoadHandle.IsValid())
	{
		MeshLoadHandle->CancelHandle();
		MeshLoadHandle.Reset();
	}

	if (HasServerAuthority())
	{
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			ClearAbilitySetForSlot(SlotIndex);
		}
	}

	OnEquippedWeaponChanged.Clear();

	Super::EndPlay(EndPlayReason);
}

int32 UBREquipmentComponent::ToSlotIndex(EBRWeaponSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	return IsValidSlotIndex(Index) ? Index : INDEX_NONE;
}

bool UBREquipmentComponent::IsValidSlotIndex(int32 SlotIndex)
{
	return SlotIndex >= 0 && SlotIndex < static_cast<int32>(EBRWeaponSlot::Count);
}

UBRWeaponInstance* UBREquipmentComponent::GetWeaponInSlot(EBRWeaponSlot Slot) const
{
	const int32 Index = ToSlotIndex(Slot);
	return Slots.IsValidIndex(Index) ? Slots[Index] : nullptr;
}

UBRWeaponInstance* UBREquipmentComponent::GetActiveWeapon() const
{
	return Slots.IsValidIndex(ActiveSlotIndex) ? Slots[ActiveSlotIndex] : nullptr;
}

bool UBREquipmentComponent::IsSlotOccupied(EBRWeaponSlot Slot) const
{
	return IsValid(GetWeaponInSlot(Slot));
}

bool UBREquipmentComponent::HasServerAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

APawn* UBREquipmentComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}

UAbilitySystemComponent* UBREquipmentComponent::GetOwnerAbilitySystemComponent() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

UBRWeaponInstance* UBREquipmentComponent::GiveWeapon(const FDataTableRowHandle& InWeaponRow, EBRWeaponSlot Slot)
{
	return GiveWeaponWithAmmo(InWeaponRow, Slot, INDEX_NONE, INDEX_NONE);
}

UBRWeaponInstance* UBREquipmentComponent::GiveWeaponWithAmmo(const FDataTableRowHandle& InWeaponRow,
	EBRWeaponSlot Slot, int32 InAmmoInMag, int32 InAmmoReserve)
{
	if (!ensureMsgf(HasServerAuthority(), TEXT("UBREquipmentComponent::GiveWeapon is server-only.")))
	{
		return nullptr;
	}

	const int32 SlotIndex = ToSlotIndex(Slot);
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}

	if (InWeaponRow.RowName.IsNone() || !InWeaponRow.DataTable)
	{
		return nullptr;
	}

	DestroyWeaponInSlot(SlotIndex);

	UBRWeaponInstance* Instance = NewObject<UBRWeaponInstance>(this);
	Instance->InitializeFromRow(InWeaponRow);

	if (InAmmoInMag != INDEX_NONE || InAmmoReserve != INDEX_NONE)
	{
		const int32 Mag = (InAmmoInMag != INDEX_NONE) ? InAmmoInMag : Instance->GetAmmoInMag();
		const int32 Reserve = (InAmmoReserve != INDEX_NONE) ? InAmmoReserve : Instance->GetAmmoReserve();
		Instance->OverrideAmmo(Mag, Reserve);
	}

	Slots[SlotIndex] = Instance;

	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		AddReplicatedSubObject(Instance, COND_None);
	}

	if (SlotIndex == ActiveSlotIndex)
	{
		HandleActiveWeaponChanged();
	}

	return Instance;
}

bool UBREquipmentComponent::SetActiveSlot(EBRWeaponSlot Slot)
{
	if (!ensureMsgf(HasServerAuthority(), TEXT("UBREquipmentComponent::SetActiveSlot is server-only.")))
	{
		return false;
	}

	const int32 SlotIndex = ToSlotIndex(Slot);
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	if (SlotIndex == ActiveSlotIndex)
	{
		return false;
	}
	if (!IsValid(Slots[SlotIndex]))
	{
		return false;
	}

	ClearAbilitySetForSlot(ActiveSlotIndex);
	ActiveSlotIndex = SlotIndex;
	HandleActiveWeaponChanged();
	return true;
}

ABRWeaponPickup* UBREquipmentComponent::DropWeapon(EBRWeaponSlot Slot)
{
	if (!ensureMsgf(HasServerAuthority(), TEXT("UBREquipmentComponent::DropWeapon is server-only.")))
	{
		return nullptr;
	}

	const int32 SlotIndex = ToSlotIndex(Slot);
	if (!Slots.IsValidIndex(SlotIndex) || !IsValid(Slots[SlotIndex]))
	{
		return nullptr;
	}

	UBRWeaponInstance* Instance = Slots[SlotIndex];
	const FDataTableRowHandle RowHandle = Instance->GetWeaponRowHandle();
	const int32 Mag = Instance->GetAmmoInMag();
	const int32 Reserve = Instance->GetAmmoReserve();

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	if (!DroppedPickupClass)
	{
		return nullptr;
	}

	ABRWeaponPickup* Pickup = ABRWeaponPickup::SpawnDroppedWeapon(
		World, DroppedPickupClass, RowHandle, Mag, Reserve, Owner->GetActorTransform(), Owner);

	if (!Pickup)
	{
		return nullptr;
	}

	ClearAbilitySetForSlot(SlotIndex);
	DestroyWeaponInSlot(SlotIndex);

	if (SlotIndex == ActiveSlotIndex)
	{
		ActiveSlotIndex = INDEX_NONE;
		HandleActiveWeaponChanged();
	}

	return Pickup;
}

void UBREquipmentComponent::DestroyWeaponInSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !IsValid(Slots[SlotIndex]))
	{
		return;
	}

	UBRWeaponInstance* Instance = Slots[SlotIndex];
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		RemoveReplicatedSubObject(Instance);
	}

	Slots[SlotIndex] = nullptr;
	Instance->MarkAsGarbage();
}

bool UBREquipmentComponent::ServerRequestSwapSlot_Validate(uint8 SlotIndex)
{
	return SlotIndex < static_cast<uint8>(EBRWeaponSlot::Count);
}

void UBREquipmentComponent::ServerRequestSwapSlot_Implementation(uint8 SlotIndex)
{
	if (!HasServerAuthority())
	{
		return;
	}

	const int32 Index = static_cast<int32>(SlotIndex);
	if (!IsValidSlotIndex(Index))
	{
		return;
	}

	SetActiveSlot(static_cast<EBRWeaponSlot>(Index));
}

bool UBREquipmentComponent::ServerRequestPickup_Validate(ABRWeaponPickup* Pickup)
{
	return Pickup != nullptr;
}

void UBREquipmentComponent::ServerRequestPickup_Implementation(ABRWeaponPickup* Pickup)
{
	if (!HasServerAuthority())
	{
		return;
	}

	APawn* Pawn = GetOwnerPawn();
	if (!IsValid(Pawn) || !IsValid(Pickup))
	{
		return;
	}

	if (!Pickup->CanBeCollectedBy(Pawn))
	{
		return;
	}

	const FDataTableRowHandle RowHandle = Pickup->GetWeaponRowHandle();
	const int32 Mag = Pickup->GetAmmoInMag();
	const int32 Reserve = Pickup->GetAmmoReserve();

	int32 TargetSlot = INDEX_NONE;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!IsValid(Slots[Index]))
		{
			TargetSlot = Index;
			break;
		}
	}

	const bool bMustDrop = (TargetSlot == INDEX_NONE);
	if (bMustDrop)
	{
		TargetSlot = Slots.IsValidIndex(ActiveSlotIndex) ? ActiveSlotIndex : 0;
	}

	if (!Pickup->TryCollect(Pawn))
	{
		return;
	}

	if (bMustDrop)
	{
		DropWeapon(static_cast<EBRWeaponSlot>(TargetSlot));
	}

	if (GiveWeaponWithAmmo(RowHandle, static_cast<EBRWeaponSlot>(TargetSlot), Mag, Reserve))
	{
		SetActiveSlot(static_cast<EBRWeaponSlot>(TargetSlot));
	}
}

bool UBREquipmentComponent::ServerRequestDropSlot_Validate(uint8 SlotIndex)
{
	return SlotIndex < static_cast<uint8>(EBRWeaponSlot::Count);
}

void UBREquipmentComponent::ServerRequestDropSlot_Implementation(uint8 SlotIndex)
{
	if (!HasServerAuthority())
	{
		return;
	}

	const int32 Index = static_cast<int32>(SlotIndex);
	if (!IsValidSlotIndex(Index))
	{
		return;
	}

	DropWeapon(static_cast<EBRWeaponSlot>(Index));
}

void UBREquipmentComponent::OnRep_Slots()
{
	HandleActiveWeaponChanged();
}

void UBREquipmentComponent::OnRep_ActiveSlotIndex()
{
	HandleActiveWeaponChanged();
}

void UBREquipmentComponent::HandleActiveWeaponChanged()
{
	if (HasServerAuthority())
	{
		GrantAbilitySetForSlot(ActiveSlotIndex);
	}

	RefreshEquippedMesh();
	OnEquippedWeaponChanged.Broadcast(this, GetActiveWeapon());
}

const UBRAbilitySet* UBREquipmentComponent::ResolveAbilitySetForRow(const FDataTableRowHandle& InWeaponRow) const
{
	const FBRWeaponRow* Row = InWeaponRow.GetRow<FBRWeaponRow>(TEXT("BREquipmentComponent::ResolveAbilitySetForRow"));
	if (!Row)
	{
		return nullptr;
	}

	if (Row->AbilitySet.IsNull())
	{
		return nullptr;
	}

	const UBRAbilitySet* Set = Row->AbilitySet.LoadSynchronous();
	return Set;
}

void UBREquipmentComponent::GrantAbilitySetForSlot(int32 SlotIndex)
{
	if (!BRGas::IsStageEnabled(EBRGasStage::Granting))
	{
		return;
	}

	if (!HasServerAuthority() || !Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	UBRWeaponInstance* Instance = Slots[SlotIndex];
	if (!IsValid(Instance))
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ClearAbilitySetForSlot(SlotIndex);

	const UBRAbilitySet* AbilitySet = ResolveAbilitySetForRow(Instance->GetWeaponRowHandle());
	if (!AbilitySet)
	{
		return;
	}

	FBRWeaponAbilityGrant& Grant = SlotGrants[SlotIndex];
	AbilitySet->GiveToAbilitySystem(ASC, Instance,
		Grant.AbilityHandles, Grant.EffectHandles);

	const int32 GrantedAbilities = Grant.AbilityHandles.Num();
	const int32 GrantedEffects = Grant.EffectHandles.Num();
	const FName WeaponRowName = Instance->GetWeaponRowHandle().RowName;

	if (GrantedAbilities == 0)
	{
		return;
	}
}

void UBREquipmentComponent::ClearAbilitySetForSlot(int32 SlotIndex)
{
	if (!HasServerAuthority() || !SlotGrants.IsValidIndex(SlotIndex))
	{
		return;
	}

	FBRWeaponAbilityGrant& Grant = SlotGrants[SlotIndex];
	if (Grant.IsEmpty())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent())
	{
		for (const FActiveGameplayEffectHandle& EffectHandle : Grant.EffectHandles)
		{
			if (EffectHandle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(EffectHandle);
			}
		}
		for (const FGameplayAbilitySpecHandle& AbilityHandle : Grant.AbilityHandles)
		{
			if (AbilityHandle.IsValid())
			{
				ASC->ClearAbility(AbilityHandle);
			}
		}
	}

	Grant.Reset();
}

void UBREquipmentComponent::EnsureWeaponMeshComponent()
{
	if (IsValid(EquippedMeshComponent))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	EquippedMeshComponent = NewObject<UStaticMeshComponent>(Owner, TEXT("BREquippedWeaponMesh"));
	if (!EquippedMeshComponent)
	{
		return;
	}

	EquippedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedMeshComponent->SetGenerateOverlapEvents(false);
	EquippedMeshComponent->SetIsReplicated(false);

	USceneComponent* AttachParent = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!AttachParent)
	{
		AttachParent = Owner->GetRootComponent();
	}

	EquippedMeshComponent->SetupAttachment(AttachParent, WeaponAttachSocket);
	EquippedMeshComponent->RegisterComponent();
}

void UBREquipmentComponent::RefreshEquippedMesh()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	++EquipGeneration;
	const uint32 ThisGeneration = EquipGeneration;

	if (MeshLoadHandle.IsValid())
	{
		MeshLoadHandle->CancelHandle();
		MeshLoadHandle.Reset();
	}

	const UBRWeaponInstance* Instance = GetActiveWeapon();
	const FBRWeaponRow* Row = Instance ? Instance->GetRow() : nullptr;

	if (!Row || Row->MeshSoftPath.IsNull())
	{
		if (IsValid(EquippedMeshComponent))
		{
			EquippedMeshComponent->SetStaticMesh(nullptr);
		}
		return;
	}

	EnsureWeaponMeshComponent();
	if (!IsValid(EquippedMeshComponent) || !UAssetManager::IsInitialized())
	{
		return;
	}

	const TSoftObjectPtr<UStaticMesh> SoftMesh = Row->MeshSoftPath;

	MeshLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftMesh.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, SoftMesh, ThisGeneration]()
		{
			if (ThisGeneration != EquipGeneration || !IsValid(EquippedMeshComponent))
			{
				return;
			}
			if (UStaticMesh* Mesh = SoftMesh.Get())
			{
				EquippedMeshComponent->SetStaticMesh(Mesh);
			}
		}));
}
