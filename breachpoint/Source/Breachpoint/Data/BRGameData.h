#pragma once

// BP91 step 4 — the ONE subsystem allowed to hold a table pointer or resolve a soft
// reference (rework §3.2). Pure lookup: no Tick, no timers, no gameplay state.

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"

#include "BRGameData.generated.h"

class UCurveTable;
class UDataTable;
struct FBRWeaponRow;

/**
 * Owns every UDataTable/UCurveTable handle in the module. Table asset paths come from
 * Config/DefaultGame.ini — this class hard-codes nothing:
 *
 *   [/Script/Breachpoint.BRGameData]
 *   WeaponTablePath=/Game/Data/DT_Weapons.DT_Weapons
 *   CombatCurveTablePath=/Game/Data/CT_Combat.CT_Combat
 *
 * This is the module's ONLY FStreamableManager::RequestAsyncLoad call site (BP91
 * done-box; pre-rework offenders are findings on the ticket, migrated by their own
 * packets). A missing table, row, or curve is a null/0 answer plus one LogBRCombat
 * warning — never a crash, never a default that silently plays.
 */
UCLASS(Config = Game)
class BREACHPOINT_API UBRGameData : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Null if the table is unset/unloaded or the row does not exist (warned, not defaulted). */
	const FBRWeaponRow* GetWeaponRow(FName RowName) const;

	/** 0.f if the curve table is unset/unloaded or the curve does not exist (warned, not defaulted). */
	float EvalCombatCurve(FName CurveName, float InTime) const;

private:
	void OnTablesLoaded();

	// Soft paths, filled by DefaultGame.ini (never by C++ — law 3).
	UPROPERTY(Config)
	FSoftObjectPath WeaponTablePath;

	UPROPERTY(Config)
	FSoftObjectPath CombatCurveTablePath;

	// Resolved on load-complete; UPROPERTY pins them against GC.
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> WeaponTable;

	UPROPERTY(Transient)
	TObjectPtr<UCurveTable> CombatCurveTable;

	TSharedPtr<FStreamableHandle> TableLoadHandle;
};
