#include "Weapons/BNWeapon.h"

#include "BreachpointNext.h"
#include "Data/BNDataRows.h"
#include "Data/BNGameData.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ABNWeapon::ABNWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Carried, never simulated: no collision, no overlaps. The attachment reaches clients
	// through the engine's AttachmentReplication because this actor replicates and the
	// equipment component attaches it on the authority.
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionProfileName(TEXT("NoCollision"));
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetOwnerNoSee(true);
	WeaponMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	SetRootComponent(WeaponMesh);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(WeaponMesh);
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetCollisionProfileName(TEXT("NoCollision"));
	FirstPersonMesh->SetGenerateOverlapEvents(false);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bCastDynamicShadow = false;
}

void ABNWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABNWeapon, RowName);
	DOREPLIFETIME(ABNWeapon, CurrentAmmo);
	DOREPLIFETIME(ABNWeapon, AmmoReserve);
}

void ABNWeapon::SetRowName(FName InRowName)
{
	if (!HasAuthority())
	{
		return;
	}

	RowName = InRowName;
	ApplyRow();
}

void ABNWeapon::OnRep_RowName()
{
	ApplyRow();
}

const FBNWeaponRow* ABNWeapon::GetRow() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UBNGameData* GameData = GameInstance ? GameInstance->GetSubsystem<UBNGameData>() : nullptr;
	return GameData ? GameData->FindWeaponRow(RowName) : nullptr;
}

void ABNWeapon::ApplyRow()
{
	const FBNWeaponRow* Row = GetRow();
	if (!Row)
	{
		UE_LOG(LogBN, Error, TEXT("BNWeapon: row '%s' is not in the weapon table — this weapon has no mesh, no anim layer and no abilities."), *RowName.ToString());
		return;
	}

	// Synchronous by choice: this runs once per weapon, on the authority at possession and on
	// a client the moment the identity replicates. The startup set is a handful of meshes, and
	// an async load would leave an empty hand for a frame AND race the visibility and
	// layer-link pass that immediately follows it.
	if (USkeletalMesh* Mesh = Row->WeaponMesh.LoadSynchronous())
	{
		WeaponMesh->SetSkeletalMeshAsset(Mesh);
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetSkeletalMeshAsset(Mesh);
		}
	}

	CachedAnimLayerClass = Row->AnimLayerClass.IsNull() ? nullptr : Row->AnimLayerClass.LoadSynchronous();

	// The magazine starts full, on the authority, at the moment the identity is known — a weapon
	// never exists on the server holding zero rounds it was never meant to have.
	if (HasAuthority())
	{
		CurrentAmmo = Row->MagazineSize;
	AmmoReserve = Row->ReserveAmmo;
	}
}

int32 ABNWeapon::GetMagazineSize() const
{
	const FBNWeaponRow* Row = GetRow();
	return Row ? Row->MagazineSize : 0;
}

bool ABNWeapon::ConsumeAmmo(int32 Count)
{
	if (!HasAuthority() || !HasAmmo(Count))
	{
		return false;
	}

	CurrentAmmo -= Count;
	return true;
}

void ABNWeapon::Reload()
{
	// The transfer, not a refill: before AmmoReserve existed this set CurrentAmmo = MagazineSize
	// and ammo was silently infinite. An empty reserve now reloads nothing — the empty click.
	if (HasAuthority())
	{
		const int32 Moved = CalcReloadTransfer(GetMagazineSize(), CurrentAmmo, AmmoReserve);
		CurrentAmmo += Moved;
		AmmoReserve -= Moved;
	}
}

void ABNWeapon::OnRep_AmmoReserve()
{
	// Behaviourless like OnRep_CurrentAmmo, for the same reason. The HUD's reserve readout binds here.
}

void ABNWeapon::OnRep_CurrentAmmo()
{
	// The clients' observation point, and behaviourless on purpose this wave: nothing off the
	// authority writes ammo, so there is no local guess to reconcile. The ammo readout binds here.
}

FTransform ABNWeapon::GetMuzzleTransform() const
{
	const FBNWeaponRow* Row = GetRow();
	const FName Socket = Row ? Row->MuzzleSocketName : NAME_None;
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	USkeletalMeshComponent* ViewMesh = (PawnOwner && PawnOwner->IsLocallyControlled() && FirstPersonMesh)
		? FirstPersonMesh.Get()
		: WeaponMesh.Get();
	if (ViewMesh && !Socket.IsNone() && ViewMesh->DoesSocketExist(Socket))
	{
		return ViewMesh->GetSocketTransform(Socket);
	}
	return GetActorTransform();
}
