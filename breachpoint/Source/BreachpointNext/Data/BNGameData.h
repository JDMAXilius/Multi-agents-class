#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BNGameData.generated.h"

class UDataTable;
enum class EBNBotAmbition : uint8;
struct FBNBotAmbitionRow;
struct FBNBotTuningRow;
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

	/** R10 — the difficulty tier's row, keyed by its own name (Recruit/Marine/ODST/Spartan).
	 *  Same contract again: null on a miss, and the caller falls back to the C++ tier. */
	const FBNBotTuningRow* FindBotTuningRow(FName TierName) const;

private:
	UPROPERTY()
	TObjectPtr<UDataTable> WeaponTable;

	UPROPERTY()
	TObjectPtr<UDataTable> BotAmbitionTable;

	UPROPERTY()
	TObjectPtr<UDataTable> BotTuningTable;

	/** Soft, set in DefaultGame.ini (R6 G3 3.2); a null resolve is the designed pre-ticket state. */
	UPROPERTY(Config)
	TSoftObjectPtr<UDataTable> BotAmbitionsTable;

	/** Soft, same as the ambitions table and just as optional: four tiers exist in C++ whether
	 *  or not this asset does. */
	UPROPERTY(Config)
	TSoftObjectPtr<UDataTable> BotTuningTablePath;

	/** Once, not per lookup: bots run on C++ default rows until the table lands. */
	mutable bool bWarnedNoAmbitionTable = false;
	mutable bool bWarnedNoTuningTable = false;
};
