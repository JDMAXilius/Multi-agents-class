#include "Weapons/BRWeaponInstance.h"

#include "Core/BRCore.h"
#include "Data/BRDataRows.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UBRWeaponInstance::UBRWeaponInstance()
{
}

void UBRWeaponInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBRWeaponInstance, WeaponRow);

	DOREPLIFETIME_CONDITION(UBRWeaponInstance, AmmoInMag, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBRWeaponInstance, AmmoReserve, COND_OwnerOnly);
}

UWorld* UBRWeaponInstance::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (const AActor* Owner = GetOwningActor())
	{
		return Owner->GetWorld();
	}
	return nullptr;
}

AActor* UBRWeaponInstance::GetOwningActor() const
{
	return GetTypedOuter<AActor>();
}

bool UBRWeaponInstance::HasAmmoWriteRights() const
{
	const AActor* Owner = GetOwningActor();
	if (!Owner)
	{
		return false;
	}

	const ENetRole LocalRole = Owner->GetLocalRole();
	return LocalRole == ROLE_Authority || LocalRole == ROLE_AutonomousProxy;
}

void UBRWeaponInstance::InitializeFromRow(const FDataTableRowHandle& InWeaponRow)
{
	const AActor* Owner = GetOwningActor();
	if (!ensureMsgf(Owner && Owner->HasAuthority(),
			TEXT("BRWeaponInstance::InitializeFromRow is server-only; a client-built weapon is not a weapon.")))
	{
		return;
	}

	WeaponRow = InWeaponRow;

	const FBRWeaponRow* Row = GetRow();
	if (!Row)
	{
		AmmoInMag = 0;
		AmmoReserve = 0;
		OnAmmoChanged.Broadcast(this);
		return;
	}

	CalcInitialAmmo(*Row, AmmoInMag, AmmoReserve);
	OnAmmoChanged.Broadcast(this);
}

void UBRWeaponInstance::OverrideAmmo(int32 InAmmoInMag, int32 InAmmoReserve)
{
	const AActor* Owner = GetOwningActor();
	if (!ensureMsgf(Owner && Owner->HasAuthority(),
			TEXT("BRWeaponInstance::OverrideAmmo is server-only.")))
	{
		return;
	}

	const FBRWeaponRow* Row = GetRow();
	const int32 MagCeiling = Row ? Row->MagSize : 0;

	AmmoInMag = FMath::Clamp(InAmmoInMag, 0, MagCeiling);
	AmmoReserve = FMath::Max(0, InAmmoReserve);
	OnAmmoChanged.Broadcast(this);
}

const FBRWeaponRow* UBRWeaponInstance::GetRow() const
{
	if (!WeaponRow.DataTable || WeaponRow.RowName.IsNone())
	{
		return nullptr;
	}

	return WeaponRow.DataTable->FindRow<FBRWeaponRow>(WeaponRow.RowName, TEXT("BRWeaponInstance"), false);
}

bool UBRWeaponInstance::ConsumeAmmoForShot()
{
	if (!ensureMsgf(HasAmmoWriteRights(),
			TEXT("BRWeaponInstance::ConsumeAmmoForShot called without authority or ownership.")))
	{
		return false;
	}

	if (AmmoInMag <= 0)
	{
		return false;
	}

	--AmmoInMag;
	OnAmmoChanged.Broadcast(this);
	return true;
}

int32 UBRWeaponInstance::CommitReload()
{
	if (!ensureMsgf(HasAmmoWriteRights(),
			TEXT("BRWeaponInstance::CommitReload called without authority or ownership.")))
	{
		return 0;
	}

	const FBRWeaponRow* Row = GetRow();
	if (!Row)
	{
		return 0;
	}

	const int32 Moved = CalcReloadTransfer(Row->MagSize, AmmoInMag, AmmoReserve);
	if (Moved <= 0)
	{
		return 0;
	}

	AmmoInMag += Moved;
	AmmoReserve -= Moved;
	OnAmmoChanged.Broadcast(this);
	return Moved;
}

int32 UBRWeaponInstance::CalcReloadTransfer(int32 InMagSize, int32 InAmmoInMag, int32 InAmmoReserve)
{
	if (InMagSize <= 0 || InAmmoInMag < 0 || InAmmoReserve <= 0)
	{
		return 0;
	}

	const int32 FreeSpace = InMagSize - InAmmoInMag;
	if (FreeSpace <= 0)
	{
		return 0;
	}

	return FMath::Min(FreeSpace, InAmmoReserve);
}

void UBRWeaponInstance::CalcInitialAmmo(const FBRWeaponRow& Row, int32& OutAmmoInMag, int32& OutAmmoReserve)
{
	OutAmmoInMag = FMath::Max(0, Row.MagSize);
	OutAmmoReserve = FMath::Max(0, Row.GetStartingReserveAmmo());
}

void UBRWeaponInstance::SetLastFireServerTimeSeconds(double InServerTimeSeconds)
{
	const AActor* Owner = GetOwningActor();
	if (!ensureMsgf(Owner && Owner->HasAuthority(),
			TEXT("The rate-gate timestamp is server state; a client stamp would BE the exploit.")))
	{
		return;
	}

	LastFireServerTimeSeconds = InServerTimeSeconds;
}

void UBRWeaponInstance::OnRep_Ammo()
{
	OnAmmoChanged.Broadcast(this);
}
