#pragma once

#include "AI/BRBotBrain.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "BRBotManagerComponent.generated.h"

class ABRBotController;
class AController;
class UDataTable;

UCLASS(meta = (BlueprintSpawnableComponent))
class BREACHPOINT_API UBRBotManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBRBotManagerComponent();

	void ConfigureRoster(int32 InMatchSeed, int32 InRosterSize, float InBackfillDelay_s);

	bool LoadBotTables(const UDataTable* TuningTable, const UDataTable* AmbitionsTable, const TArray<FName>& TierPerSlot);

	void FillToRoster();

	void NotifyCombatantLeft(AController* Leaver);

	void NotifyHumanJoined();

	const TArray<TObjectPtr<ABRBotController>>& GetBots() const { return Bots; }

	int32 GetBotCount() const { return Bots.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	ABRBotController* SpawnBotForSlot(int32 SlotIndex);

	void RemoveBot(ABRBotController* Bot);

	void HandleBackfillElapsed();

	int32 MakeSeedForSlot(int32 SlotIndex) const;

	int32 CountHumans() const;

private:
	UPROPERTY()
	TArray<TObjectPtr<ABRBotController>> Bots;

	UPROPERTY()
	TArray<FName> TierRoster;

	UPROPERTY()
	TMap<FName, FBRBotTierScalars> TierScalarsByName;

	UPROPERTY()
	TArray<FBRBotAmbitionDef> AmbitionDefs;

	int32 MatchSeed = 0;
	int32 RosterSize = 0;
	float BackfillDelay_s = 0.f;
	bool bTablesLoaded = false;

	FTimerHandle BackfillTimer;
};
