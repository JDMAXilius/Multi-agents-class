// Breachpoint. What a player is carrying, which of it is in their hands, and how it got there.

#include "Weapons/BREquipmentComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
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
	// Law 4. Everything here is an event, a RepNotify, or a load callback.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;

	// Fixed length for the life of the component: an index on the wire means the same slot on
	// both ends, forever. The length is the enum, never a literal 2.
	Slots.SetNum(static_cast<int32>(EBRWeaponSlot::Count));
	SlotGrants.SetNum(static_cast<int32>(EBRWeaponSlot::Count));

	// A C++ class default, not an asset reference: R18 has no Blueprint for this, and
	// data-and-assets.md's soft-ref law is about ASSET references in data.
	DroppedPickupClass = ABRWeaponPickup::StaticClass();
}

void UBREquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ARCHITECTURE §6.1: "Equipment slots + active index | COND_None". Public on purpose —
	// every client must render the right gun in the right hands. The PRIVATE part (ammo) is
	// COND_OwnerOnly one level down, inside UBRWeaponInstance.
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

	// Weapons given before the component was ready to replicate still have to be registered.
	for (const TObjectPtr<UBRWeaponInstance>& Instance : Slots)
	{
		if (IsValid(Instance))
		{
			// COND_None: the weapon OBJECT is public (everyone renders it); its ammo
			// properties carry their own COND_OwnerOnly.
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

// -- Queries --------------------------------------------------------------------------------

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
	// The ASC lives on the PlayerState (§3.6); the globals helper walks the interface for us,
	// so this file needs no knowledge of AbilitySystem/ at all.
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

// -- Server authority -----------------------------------------------------------------------

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
		UE_LOG(LogBRCombat, Error, TEXT("GiveWeapon: no row supplied; slot %d left untouched."), SlotIndex);
		return nullptr;
	}

	// Whatever was here is gone. The CALLER decides whether it should have hit the ground
	// first (DropWeapon) — this function will not guess and quietly delete a Rocket.
	DestroyWeaponInSlot(SlotIndex);

	UBRWeaponInstance* Instance = NewObject<UBRWeaponInstance>(this);
	Instance->InitializeFromRow(InWeaponRow);

	// A pickup's carried ammunition overrides the row default. INDEX_NONE means "row default",
	// which InitializeFromRow already applied.
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

	// The server does not get a RepNotify for its own write.
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
		return false; // No-op, not an error.
	}
	if (!IsValid(Slots[SlotIndex]))
	{
		// Empty hands are reachable only by losing the weapon, never by asking for an empty
		// slot. Refusing here is what stops "swap to nothing" from being a free reload cancel.
		return false;
	}

	// The grant follows the hands: exactly one weapon's abilities are live at a time, so a
	// swap can never leave the previous weapon's fire ability grantable.
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
		UE_LOG(LogBRCombat, Error, TEXT("DropWeapon: no DroppedPickupClass configured; nothing dropped."));
		return nullptr;
	}

	// Dropped at the owner's own transform. A throw arc is a FEEL decision with numbers in it
	// (impulse, forward offset, spin) and those numbers have no table yet — so this packet
	// does the un-tuned, un-guessable thing rather than inventing three constants.
	ABRWeaponPickup* Pickup = ABRWeaponPickup::SpawnDroppedWeapon(
		World, DroppedPickupClass, RowHandle, Mag, Reserve, Owner->GetActorTransform(), Owner);

	if (!Pickup)
	{
		// The weapon stays in hand: losing it to a failed spawn would delete a player's Rocket.
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

// -- Client intent, server-validated ----------------------------------------------------------

bool UBREquipmentComponent::ServerRequestSwapSlot_Validate(uint8 SlotIndex)
{
	// A REAL check on the wire value: anything outside the slot enum is a malformed or forged
	// packet, and returning false closes the connection rather than continuing to trust it.
	// What this does NOT do is decide whether the swap is legal — that needs server state and
	// happens below, where a rejection is an ordinary outcome instead of a disconnect.
	return SlotIndex < static_cast<uint8>(EBRWeaponSlot::Count);
}

void UBREquipmentComponent::ServerRequestSwapSlot_Implementation(uint8 SlotIndex)
{
	// The check IS the documentation (netcode.md law 1), even on a path that can only be the
	// server.
	if (!HasServerAuthority())
	{
		return;
	}

	const int32 Index = static_cast<int32>(SlotIndex);
	if (!IsValidSlotIndex(Index))
	{
		return;
	}

	// Rejected silently and cheaply when the slot is empty or already active: the client sees
	// no swap, which is exactly what happened.
	SetActiveSlot(static_cast<EBRWeaponSlot>(Index));
}

bool UBREquipmentComponent::ServerRequestPickup_Validate(ABRWeaponPickup* Pickup)
{
	// The only fact about the wire value worth asserting: a null reference is malformed. The
	// object reference itself is type-checked by the RPC serializer, and everything that
	// matters (is it consumed? is the pawn touching it?) is server state, asked below.
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

	// THE decision, and it is the pickup's to make, from the server's own overlap set. The
	// client sent no position, no distance and no claim of contact — there is nothing here to
	// forge. A pawn that is not really touching it is refused, and so is a corpse.
	if (!Pickup->CanBeCollectedBy(Pawn))
	{
		return;
	}

	// Read the payload BEFORE collecting: TryCollect destroys the actor.
	const FDataTableRowHandle RowHandle = Pickup->GetWeaponRowHandle();
	const int32 Mag = Pickup->GetAmmoInMag();
	const int32 Reserve = Pickup->GetAmmoReserve();

	// Prefer an empty slot; otherwise the weapon in hand is what gets replaced, and it goes
	// on the ground rather than into nothing.
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

	// Win the race FIRST. If another player took it a moment ago, nothing below happens and
	// this player keeps what they were holding.
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

// -- Equip flow -------------------------------------------------------------------------------

void UBREquipmentComponent::OnRep_Slots()
{
	// Slots and ActiveSlotIndex are separate properties and may arrive in either order
	// (netcode.md law 7). Both notifies funnel here, and the work is idempotent.
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

// -- Ability set (BP02 seam) --------------------------------------------------------------------

const UBRAbilitySet* UBREquipmentComponent::ResolveAbilitySetForRow(const FDataTableRowHandle& InWeaponRow) const
{
	// CONTRACT_GAP, reported with this packet. DT_Weapons.csv is the source of truth for the
	// weapon schema and it has NO ability-set column, so there is no honest way to answer this
	// question yet. The refusal is deliberate and loud: no fallback set, no "first set found",
	// no guess. Adding the column (TSoftClassPtr<UBRAbilitySet>) turns this into one line.
	UE_LOG(LogBRCombat, Error,
		TEXT("BREquipmentComponent: no ability set is mapped for weapon row '%s' — DT_Weapons.csv "
			 "has no AbilitySet column (BP03 step 1 contract_gap). The weapon equips but cannot fire."),
		*InWeaponRow.RowName.ToString());
	return nullptr;
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
		// Possession has not finished (netcode.md law 7). Not an error: the equip that follows
		// possession will grant. Left as a log so a MISSING grant is still traceable.
		UE_LOG(LogBRCombat, Verbose, TEXT("GrantAbilitySetForSlot: no ASC yet; grant deferred."));
		return;
	}

	// Never stack grants: an un-cleared previous grant is a duplicated fire ability.
	ClearAbilitySetForSlot(SlotIndex);

	const UBRAbilitySet* AbilitySet = ResolveAbilitySetForRow(Instance->GetWeaponRowHandle());
	if (!AbilitySet)
	{
		return;
	}

	// ---------------------------------------------------------------------------------------
	// BP02 SEAM — the one call site. When AbilitySystem/BRAbilitySet.h lands, this becomes:
	//
	//   FBRWeaponAbilityGrant& Grant = SlotGrants[SlotIndex];
	//   AbilitySet->GiveToAbilitySystem(ASC, /*SourceObject*/ Instance,
	//                                   Grant.AbilityHandles, Grant.EffectHandles);
	//
	// and nothing else in this file moves. The include goes in this .cpp, not the header.
	// Step 1 deliberately does not include a header another builder is still authoring.
	// ---------------------------------------------------------------------------------------
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

	// Stock GAS only — this half of the seam needs nothing from BP02.
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
				// ClearAbility, not SetRemoveAbilityOnEnd: an unequipped weapon's ability must
				// not survive as a grantable spec, or a swap becomes a way to keep two guns.
				ASC->ClearAbility(AbilityHandle);
			}
		}
	}

	Grant.Reset();
}

// -- Cosmetic mesh ------------------------------------------------------------------------------

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

	// Cosmetic: it must never be able to answer a gameplay question.
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
	// A dedicated server renders nothing and must not pay for weapon meshes. A listen server
	// does render, so this is a net-mode test, not an authority test.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Every equip invalidates any load still in flight.
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

	// SOFT ref resolved through the streamable manager at the load point (data-and-assets.md).
	// Nothing in this class ever holds a hard mesh reference, and ConstructorHelpers is banned
	// outright — the pre-tool hook blocks it before it can be written.
	MeshLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftMesh.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, SoftMesh, ThisGeneration]()
		{
			// A load that finishes after a later equip belongs to a weapon nobody is holding.
			// Without this the swap "sometimes shows the wrong gun" on a cold asset cache.
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
