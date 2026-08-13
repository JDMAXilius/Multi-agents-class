#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BNGameData.generated.h"

class UDataTable;
struct FBNWeaponRow;

// The one read path for table data. Everything it holds is read-only configuration loaded
// from the same assets on every machine, so nothing here replicates or needs to.
UCLASS()
class BREACHPOINTNEXT_API UBNGameData : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const UDataTable* GetWeaponTable() const { return WeaponTable; }

	/** Null on a miss. A missing row is an answer, not a failure. */
	const FBNWeaponRow* FindWeaponRow(FName RowName) const;

private:
	UPROPERTY()
	TObjectPtr<UDataTable> WeaponTable;
};
