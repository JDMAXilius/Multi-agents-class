#include "Weapons/BNEquipmentComponent.h"

#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Data/BNDataRows.h"
#include "Weapons/BNWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UBNEquipmentComponent::UBNEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UBNEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBNEquipmentComponent, Weapons);
	DOREPLIFETIME(UBNEquipmentComponent, CurrentIndex);
}

void UBNEquipmentComponent::InitializeCarriedWeapons()
{
	AActor* Owner = GetOwner();
	ACharacter* Character = Cast<ACharacter>(Owner);
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !Mesh || !World || !Weapons.IsEmpty())
	{
		return;
	}

	APawn* InstigatorPawn = Cast<APawn>(Owner);
	for (const FName& StartupRow : StartupWeaponRows)
	{
		if (StartupRow.IsNone())
		{
			continue;
		}

		// Deferred so the identity is set before BeginPlay — the weapon never exists, on any
		// machine, in a state where its row is unknown.
		ABNWeapon* Weapon = World->SpawnActorDeferred<ABNWeapon>(
			ABNWeapon::StaticClass(), FTransform::Identity, Owner, InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Weapon)
		{
			continue;
		}
		Weapon->SetRowName(StartupRow);
		Weapon->FinishSpawning(FTransform::Identity);

		const FBNWeaponRow* Row = Weapon->GetRow();
		const FName Socket = Row ? Row->AttachSocketName : NAME_None;
		Weapon->AttachToComponent(Mesh,
			FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget,
				EAttachmentRule::KeepWorld, /*bWeldSimulatedBodies=*/false),
			Socket);

		// MyCharacter.cpp:676 learned this the loud way: a bad socket is silent, and the weapon
		// ends up at the mesh root — under the character's feet rather than in its hand.
		if (Socket.IsNone() || !Mesh->DoesSocketExist(Socket))
		{
			UE_LOG(LogBN, Warning, TEXT("BNEquipmentComponent: row '%s' AttachSocketName '%s' is neither a socket nor a bone on the character mesh — the weapon sits at the mesh root, at the character's feet."),
				*StartupRow.ToString(), *Socket.ToString());
		}

		Weapons.Add(Weapon);
	}

	EquipIndex(0);
}

void UBNEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			RevokeGrantedAbilitySet();

			for (ABNWeapon* Weapon : Weapons)
			{
				if (IsValid(Weapon))
				{
					Weapon->Destroy();
				}
			}
			Weapons.Reset();
			CurrentIndex = INDEX_NONE;
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UBNEquipmentComponent::EquipNext()
{
	if (!Weapons.IsEmpty())
	{
		EquipIndex((CurrentIndex + 1) % Weapons.Num());
	}
}

void UBNEquipmentComponent::EquipPrevious()
{
	if (Weapons.IsEmpty())
	{
		return;
	}

	int32 NewIndex = (CurrentIndex - 1) % Weapons.Num();
	if (NewIndex < 0)
	{
		NewIndex += Weapons.Num();
	}
	EquipIndex(NewIndex);
}

void UBNEquipmentComponent::EquipIndex(int32 NewIndex)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Weapons.IsValidIndex(NewIndex) || NewIndex == CurrentIndex)
	{
		return;
	}

	CurrentIndex = NewIndex;
	UpdateGrantedAbilitySet();
	// The authority applies its own visibility and layer; every client does it in the OnRep.
	ApplyCurrentWeapon();
}

ABNWeapon* UBNEquipmentComponent::GetCurrentWeapon() const
{
	return Weapons.IsValidIndex(CurrentIndex) ? Weapons[CurrentIndex].Get() : nullptr;
}

UClass* UBNEquipmentComponent::GetCurrentWeaponAnimLayer() const
{
	const ABNWeapon* Weapon = GetCurrentWeapon();
	return Weapon ? Weapon->GetAnimLayerClass() : nullptr;
}

void UBNEquipmentComponent::OnRep_Weapons()
{
	ApplyCurrentWeapon();
}

void UBNEquipmentComponent::OnRep_CurrentIndex()
{
	ApplyCurrentWeapon();
}

void UBNEquipmentComponent::ApplyCurrentWeapon()
{
	for (int32 Index = 0; Index < Weapons.Num(); ++Index)
	{
		if (ABNWeapon* Weapon = Weapons[Index])
		{
			Weapon->SetActorHiddenInGame(Index != CurrentIndex);
		}
	}

	// R1's seam does the rest: the character owns linking and links once per class, so
	// re-calling it after every index change — here, on every machine — IS the layer swap.
	if (ABNCharacter* Character = Cast<ABNCharacter>(GetOwner()))
	{
		Character->InitializeAnimLayer();
	}
}

void UBNEquipmentComponent::UpdateGrantedAbilitySet()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	RevokeGrantedAbilitySet();

	UBNAbilitySystemComponent* ASC = GetAbilitySystem();
	const ABNWeapon* Weapon = GetCurrentWeapon();
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	if (!ASC || !Row || Row->AbilitySet.IsNull())
	{
		return;
	}

	// Authority-side, one small data asset: an async grant would leave the weapon equipped
	// with dead verbs for as long as the load took. The swap verbs are NOT in here — they are
	// the inventory's, granted once by ABNPlayerState::GrantDefaults.
	if (UBNAbilitySet* Set = Row->AbilitySet.LoadSynchronous())
	{
		Set->GiveToAbilitySystem(ASC, GrantedHandles);
		GrantedSet = Set;
		GrantedASC = ASC;
	}
}

void UBNEquipmentComponent::RevokeGrantedAbilitySet()
{
	// Through the CACHED ASC, never a fresh PlayerState lookup: APawn::UnPossessed() nulls
	// PlayerState before the corpse is destroyed, so at EndPlay the lookup answers null, the
	// revoke silently never happens, and the next body grants a SECOND copy onto the same
	// persistent ASC — two specs on one input tag, compounding every life.
	if (GrantedSet)
	{
		if (UBNAbilitySystemComponent* ASC = GrantedASC.Get())
		{
			GrantedSet->TakeFromAbilitySystem(ASC, GrantedHandles);
		}
		GrantedSet = nullptr;
	}
	GrantedASC.Reset();
}

UBNAbilitySystemComponent* UBNEquipmentComponent::GetAbilitySystem() const
{
	const ABNCharacter* Character = Cast<ABNCharacter>(GetOwner());
	return Character ? Cast<UBNAbilitySystemComponent>(Character->GetAbilitySystemComponent()) : nullptr;
}
