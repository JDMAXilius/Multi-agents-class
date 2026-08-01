// Breachpoint. One equipped weapon: the row it came from, and its ammunition.

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

	// Public: everybody needs to know WHICH weapon is in the hands they can see.
	DOREPLIFETIME(UBRWeaponInstance, WeaponRow);

	// Private: THE anti-leak line. netcode.md law 5 — hidden state stays hidden, and it stays
	// hidden at replication, not at render. With COND_OwnerOnly these two properties are not
	// part of the replication layout sent to a non-owning connection at all: an enemy client
	// has no bytes to read, no OnRep to hook, and nothing to recover by patching its own
	// binary. Widening either of these to COND_None is a `high` finding, not a convenience.
	DOREPLIFETIME_CONDITION(UBRWeaponInstance, AmmoInMag, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBRWeaponInstance, AmmoReserve, COND_OwnerOnly);
}

UWorld* UBRWeaponInstance::GetWorld() const
{
	// The CDO has no world; answering otherwise breaks editor construction.
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

	// Authority = truth. AutonomousProxy = the owning client predicting its own trigger pull,
	// which gas-purity.md's ammo ledger explicitly allows because COND_OwnerOnly replication
	// is the correction path. A SimulatedProxy writing ammo would be inventing state about
	// somebody else's gun.
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
		// Refuse loudly rather than hand back a zero-ammo, zero-damage weapon that looks
		// equipped and does nothing. A missing row is a data defect and must read as one.
		UE_LOG(LogBRCombat, Error,
			TEXT("BRWeaponInstance: row '%s' not found in the supplied DataTable; weapon left unarmed."),
			*InWeaponRow.RowName.ToString());
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

	// Clamp rather than trust: this value arrives from a pickup that a client asked for, and
	// the magazine ceiling is a rule, not a suggestion.
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

	// FindRow with bWarnIfRowMissing=false: the caller decides how loud a missing row is.
	// GetRow<>() would log for us at a severity we do not control.
	return WeaponRow.DataTable->FindRow<FBRWeaponRow>(WeaponRow.RowName, TEXT("BRWeaponInstance"), /*bWarnIfRowMissing*/ false);
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
		// The "fire with 0 ammo" cheat (BP03 step 3) dies here on the server, whatever the
		// client believed. Returning false is not a failure mode to route around.
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
	// Refuse nonsense inputs instead of extrapolating from them. Every branch below is a
	// decision, not a guard against "probably never".
	if (InMagSize <= 0 || InAmmoInMag < 0 || InAmmoReserve <= 0)
	{
		return 0;
	}

	const int32 FreeSpace = InMagSize - InAmmoInMag;
	if (FreeSpace <= 0)
	{
		// Full magazine, or an over-full one (a data reimport that shrank MagSize). Either
		// way a reload moves nothing; it never SPILLS rounds back into reserve.
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
	// Cosmetic reaction only (netcode.md law 3): this is the HUD's cue and the correction of
	// a mispredicted decrement. Removing this body must not change a gameplay outcome.
	OnAmmoChanged.Broadcast(this);
}
