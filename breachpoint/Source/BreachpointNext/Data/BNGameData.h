#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BNGameData.generated.h"

class UDataTable;
enum class EBNBotAmbition : uint8;
struct FBNBotAmbitionRow;
struct FBNWeaponRow;

// The one read path for table data. Everything it holds is read-only configuration loaded
// from the same assets on every machine, so nothing here replicates or needs to.
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNGameData : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const UDataTable* GetWeaponTable() const { return WeaponTable; }

	/** Null on a miss. A missing row is an answer, not a failure. */
	const FBNWeaponRow* FindWeaponRow(FName RowName) const;

	/** Null on a miss, same contract as FindWeaponRow — the caller falls back to
	 *  UBNBotBrain::DefaultRow. A missing TABLE warns once: the DT is the editor ticket. */
	const FBNBotAmbitionRow* FindBotAmbitionRow(EBNBotAmbition Ambition) const;

private:
	UPROPERTY()
	TObjectPtr<UDataTable> WeaponTable;

	UPROPERTY()
	TObjectPtr<UDataTable> BotAmbitionTable;

	/** Soft, set in DefaultGame.ini (R6 G3 3.2); a null resolve is the designed pre-ticket state. */
	UPROPERTY(Config)
	TSoftObjectPtr<UDataTable> BotAmbitionsTable;

	/** Once, not per lookup: bots run on C++ default rows until the table lands. */
	mutable bool bWarnedNoAmbitionTable = false;
};
