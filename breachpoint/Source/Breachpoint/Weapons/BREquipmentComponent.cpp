#include "Weapons/BREquipmentComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/BRAbilitySet.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/BRCore.h"
#include "Data/BRDataRows.h"
#include "Engine/ActorChannel.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
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
	RefreshOwnerAnimLayers();
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

void UBREquipmentComponent::EnsureWeaponMeshComponents()
{
	if (!EquippedMeshComponents.IsEmpty())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const auto AddWeaponMesh = [this, Owner](USceneComponent* AttachParent, const UPrimitiveComponent* CopyVisibilityFrom)
	{
		if (!AttachParent)
		{
			return;
		}

		USkeletalMeshComponent* WeaponMesh = NewObject<USkeletalMeshComponent>(Owner);
		if (!WeaponMesh)
		{
			return;
		}

		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMesh->SetGenerateOverlapEvents(false);
		WeaponMesh->SetIsReplicated(false);

		if (CopyVisibilityFrom)
		{
			// Inherited rather than reasoned about: a weapon held by a mesh only its owner
			// sees must be seen by only its owner, and the first-person primitive type is
			// what makes it render at the first-person FOV and scale instead of clipping
			// through the geometry the arms holding it pass over.
			WeaponMesh->SetOnlyOwnerSee(CopyVisibilityFrom->bOnlyOwnerSee);
			WeaponMesh->SetOwnerNoSee(CopyVisibilityFrom->bOwnerNoSee);
			WeaponMesh->SetFirstPersonPrimitiveType(CopyVisibilityFrom->FirstPersonPrimitiveType);
		}

		// SnapToTarget, matching AShooterCharacter::AttachWeaponMeshes: the socket carries
		// the grip transform, so the weapon must take it whole rather than keep its own.
		WeaponMesh->SetupAttachment(AttachParent, WeaponAttachSocket);
		WeaponMesh->RegisterComponent();
		WeaponMesh->AttachToComponent(AttachParent,
			FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false), WeaponAttachSocket);

		EquippedMeshComponents.Add(WeaponMesh);
	};

	USkeletalMeshComponent* FirstPersonMesh = nullptr;
	USkeletalMeshComponent* ThirdPersonMesh = nullptr;
	ResolveOwnerMeshes(FirstPersonMesh, ThirdPersonMesh);

	AddWeaponMesh(FirstPersonMesh, FirstPersonMesh);
	AddWeaponMesh(ThirdPersonMesh, ThirdPersonMesh);

	if (EquippedMeshComponents.IsEmpty())
	{
		// No skeletal body at all — a test pawn or a blockout dummy. The weapon still
		// exists so the ammo and HUD path stays exercisable; it just hangs off the root.
		AddWeaponMesh(Owner->GetRootComponent(), nullptr);
	}
}

void UBREquipmentComponent::ResolveOwnerMeshes(USkeletalMeshComponent*& OutFirstPerson,
	USkeletalMeshComponent*& OutThirdPerson) const
{
	OutFirstPerson = nullptr;
	OutThirdPerson = nullptr;

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// The character mesh is third person by definition (ACharacter owns it and the pawn's
	// world-space representation is drawn from it). Asked for by identity and not by
	// "the first skeletal mesh found", because component order is not a contract.
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		OutThirdPerson = Character->GetMesh();
	}

	TArray<USkeletalMeshComponent*> OwnerMeshes;
	Owner->GetComponents(OwnerMeshes);

	for (USkeletalMeshComponent* OwnerMesh : OwnerMeshes)
	{
		if (OwnerMesh == OutThirdPerson)
		{
			continue;
		}
		// bOnlyOwnerSee is what MAKES a mesh first person here — ABRCharacter sets it on the
		// first-person body and bOwnerNoSee on the third. A name test would break the first
		// time someone renamed a component.
		if (OwnerMesh->bOnlyOwnerSee)
		{
			OutFirstPerson = OwnerMesh;
			break;
		}
	}

	if (!OutThirdPerson && !OwnerMeshes.IsEmpty())
	{
		// Not an ACharacter: take any mesh that is not the first-person one.
		for (USkeletalMeshComponent* OwnerMesh : OwnerMeshes)
		{
			if (OwnerMesh != OutFirstPerson)
			{
				OutThirdPerson = OwnerMesh;
				break;
			}
		}
	}
}

void UBREquipmentComponent::RefreshOwnerAnimLayers()
{
	USkeletalMeshComponent* FirstPersonMesh = nullptr;
	USkeletalMeshComponent* ThirdPersonMesh = nullptr;
	ResolveOwnerMeshes(FirstPersonMesh, ThirdPersonMesh);

	if (!FirstPersonMesh && !ThirdPersonMesh)
	{
		return;
	}

	if (!bCapturedDefaultAnimClasses)
	{
		DefaultFirstPersonAnimClass = FirstPersonMesh ? FirstPersonMesh->GetAnimClass() : nullptr;
		DefaultThirdPersonAnimClass = ThirdPersonMesh ? ThirdPersonMesh->GetAnimClass() : nullptr;
		bCapturedDefaultAnimClasses = true;
	}

	const UBRWeaponInstance* Instance = GetActiveWeapon();
	const FBRWeaponRow* Row = Instance ? Instance->GetRow() : nullptr;

	// A row that names no anim BP falls back to the unarmed default rather than to null:
	// clearing the anim class outright leaves a T-posed pawn, which reads as a broken
	// skeleton rather than as an unconfigured column.
	const auto Apply = [](USkeletalMeshComponent* Mesh, const TSoftClassPtr<UAnimInstance>& Wanted,
		const TSubclassOf<UAnimInstance>& Fallback)
	{
		if (!Mesh)
		{
			return;
		}

		UClass* Desired = Wanted.IsNull() ? Fallback.Get() : Wanted.LoadSynchronous();
		if (Mesh->GetAnimClass() != Desired)
		{
			Mesh->SetAnimInstanceClass(Desired);
		}
	};

	static const TSoftClassPtr<UAnimInstance> NoAnimBP;

	Apply(FirstPersonMesh, Row ? Row->FirstPersonAnimBP : NoAnimBP, DefaultFirstPersonAnimClass);
	Apply(ThirdPersonMesh, Row ? Row->ThirdPersonAnimBP : NoAnimBP, DefaultThirdPersonAnimClass);
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

	// The HELD mesh, not MeshSoftPath — that one is the static prop a dropped pickup uses.
	if (!Row || Row->HeldMeshSoftPath.IsNull())
	{
		for (const TObjectPtr<USkeletalMeshComponent>& WeaponMesh : EquippedMeshComponents)
		{
			if (IsValid(WeaponMesh))
			{
				WeaponMesh->SetSkeletalMesh(nullptr);
			}
		}
		return;
	}

	EnsureWeaponMeshComponents();
	if (EquippedMeshComponents.IsEmpty() || !UAssetManager::IsInitialized())
	{
		return;
	}

	const TSoftObjectPtr<USkeletalMesh> SoftMesh = Row->HeldMeshSoftPath;

	MeshLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftMesh.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, SoftMesh, ThisGeneration]()
		{
			if (ThisGeneration != EquipGeneration)
			{
				return;
			}
			USkeletalMesh* Mesh = SoftMesh.Get();
			if (!Mesh)
			{
				return;
			}
			for (const TObjectPtr<USkeletalMeshComponent>& WeaponMesh : EquippedMeshComponents)
			{
				if (IsValid(WeaponMesh))
				{
					WeaponMesh->SetSkeletalMesh(Mesh);
				}
			}
		}));
}
