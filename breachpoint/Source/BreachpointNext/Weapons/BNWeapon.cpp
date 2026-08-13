#include "Weapons/BNWeapon.h"

#include "BreachpointNext.h"
#include "Data/BNDataRows.h"
#include "Data/BNGameData.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
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
	SetRootComponent(WeaponMesh);
}

void ABNWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABNWeapon, RowName);
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
	}

	CachedAnimLayerClass = Row->AnimLayerClass.IsNull() ? nullptr : Row->AnimLayerClass.LoadSynchronous();
}

FTransform ABNWeapon::GetMuzzleTransform() const
{
	const FBNWeaponRow* Row = GetRow();
	const FName Socket = Row ? Row->MuzzleSocketName : NAME_None;
	if (WeaponMesh && !Socket.IsNone() && WeaponMesh->DoesSocketExist(Socket))
	{
		return WeaponMesh->GetSocketTransform(Socket);
	}
	return GetActorTransform();
}
