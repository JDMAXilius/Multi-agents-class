#include "Weapons/BNEquipmentComponent.h"

#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Data/BNDataRows.h"
#include "Data/BNGameData.h"
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

		// Empty hands are a slot, not a weapon actor. A null entry keeps the swap index honest
		// (Unarmed → Pistol → Rifle → Shotgun → Knife) and ResolveAnimLayerClass falls through
		// to UnarmedAnimLayer. An optional DT row named Unarmed still feeds melee numbers.
		if (StartupRow == FName(TEXT("Unarmed")))
		{
			Weapons.Add(nullptr);
			continue;
		}

		// THE ROW FIRST — it decides whether to spawn at all and WHAT to spawn. A name with no row
		// used to spawn anyway and join the cycle as a meshless ghost; now the ini names every
		// intended weapon and each joins the rotation the moment its DT row lands.
		const UGameInstance* GameInstance = World->GetGameInstance();
		const UBNGameData* GameData = GameInstance ? GameInstance->GetSubsystem<UBNGameData>() : nullptr;
		const FBNWeaponRow* Row = GameData ? GameData->FindWeaponRow(StartupRow) : nullptr;
		if (!Row)
		{
			UE_LOG(LogBN, Error, TEXT("BNEquipmentComponent: startup row '%s' does not exist in the weapon table — skipped. Add the row to DT_BNWeapons and this weapon appears."),
				*StartupRow.ToString());
			continue;
		}

		// Subclass-through-data: the row may name an ABNWeapon subclass; none = the base, which is
		// every current weapon. This is what makes ABNWeapon a BASE without a hierarchy existing.
		UClass* SpawnClass = Row->WeaponClass.IsNull() ? ABNWeapon::StaticClass() : Row->WeaponClass.LoadSynchronous();
		if (!SpawnClass)
		{
			UE_LOG(LogBN, Error, TEXT("BNEquipmentComponent: row '%s' names WeaponClass '%s' which failed to load — using ABNWeapon."),
				*StartupRow.ToString(), *Row->WeaponClass.ToString());
			SpawnClass = ABNWeapon::StaticClass();
		}

		// Deferred so the identity is set before BeginPlay — the weapon never exists, on any
		// machine, in a state where its row is unknown.
		ABNWeapon* Weapon = World->SpawnActorDeferred<ABNWeapon>(
			SpawnClass, FTransform::Identity, Owner, InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Weapon)
		{
			continue;
		}
		Weapon->SetRowName(StartupRow);
		Weapon->FinishSpawning(FTransform::Identity);
		// Born HIDDEN, so the first ApplyCurrentWeapon is a real edge: the current weapon
		// transitions hidden->shown and OnEquipped fires for the spawn equip too, not only for
		// later swaps. bHidden replicates, so joining clients start from the same state.
		Weapon->SetActorHiddenInGame(true);

		const FName Socket = Row->AttachSocketName;
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

		if (ABNCharacter* BNChar = Cast<ABNCharacter>(Owner))
		{
			BNChar->AttachWeaponMeshes(Weapon);
		}
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
			const bool bIsCurrent = Index == CurrentIndex;
			const bool bWasHidden = Weapon->IsHidden();
			Weapon->SetActorHiddenInGame(!bIsCurrent);
			// The base-class seams, fired on the EDGE only — this function re-runs on every index
			// change and both OnReps, and a hook that fires on every re-apply is not a hook, it is
			// a tick with extra steps.
			if (bIsCurrent && bWasHidden)
			{
				Weapon->OnEquipped();
			}
			else if (!bIsCurrent && !bWasHidden)
			{
				Weapon->OnUnequipped();
			}
		}
	}

	// R1's seam does the rest: the character owns linking and links once per class, so
	// re-calling it after every index change — here, on every machine — IS the layer swap.
	if (ABNCharacter* Character = Cast<ABNCharacter>(GetOwner()))
	{
		for (ABNWeapon* Weapon : Weapons)
		{
			Character->AttachWeaponMeshes(Weapon);
		}
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
