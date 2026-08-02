// Breachpoint. One equipped weapon: the row it came from, and its ammunition.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/Object.h"

#include "BRWeaponInstance.generated.h"

struct FBRWeaponRow;

DECLARE_MULTICAST_DELEGATE_OneParam(FBROnWeaponAmmoChanged, class UBRWeaponInstance*);

UCLASS(BlueprintType)
class BREACHPOINT_API UBRWeaponInstance : public UObject
{
	GENERATED_BODY()

public:
	UBRWeaponInstance();

	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UWorld* GetWorld() const override;

	void InitializeFromRow(const FDataTableRowHandle& InWeaponRow);

	void OverrideAmmo(int32 InAmmoInMag, int32 InAmmoReserve);

	const FDataTableRowHandle& GetWeaponRowHandle() const { return WeaponRow; }

	FName GetWeaponRowName() const { return WeaponRow.RowName; }

	const FBRWeaponRow* GetRow() const;

	int32 GetAmmoInMag() const { return AmmoInMag; }
	int32 GetAmmoReserve() const { return AmmoReserve; }

	FBROnWeaponAmmoChanged OnAmmoChanged;

	bool ConsumeAmmoForShot();

	int32 CommitReload();

	static int32 CalcReloadTransfer(int32 InMagSize, int32 InAmmoInMag, int32 InAmmoReserve);

	static void CalcInitialAmmo(const FBRWeaponRow& Row, int32& OutAmmoInMag, int32& OutAmmoReserve);

	double GetLastFireServerTimeSeconds() const { return LastFireServerTimeSeconds; }
	void SetLastFireServerTimeSeconds(double InServerTimeSeconds);

protected:
	UFUNCTION()
	void OnRep_Ammo();

	bool HasAmmoWriteRights() const;

	AActor* GetOwningActor() const;

private:
	UPROPERTY(Replicated)
	FDataTableRowHandle WeaponRow;

	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 AmmoInMag = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 AmmoReserve = 0;

	double LastFireServerTimeSeconds = -1.0;
};
