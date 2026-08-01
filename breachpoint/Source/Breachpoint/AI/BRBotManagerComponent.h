// Breachpoint. Match glue: fill the roster with bots, backfill leavers, seed them all.

#pragma once

#include "AI/BRBotBrain.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "BRBotManagerComponent.generated.h"

class ABRBotController;
class AController;
class UDataTable;

/**
 * UBRBotManagerComponent — the only place bots come into existence (ARCHITECTURE §3.7).
 *
 * Lives on ABRGameMode, which means: SERVER ONLY, always. Roster size and spawn authority
 * are the GameMode's domain, so this component ASKS rather than acts — it spawns a
 * controller and calls the GameMode's own RestartPlayer, exactly as the login path does for
 * a human. There is no bot-specific spawn, no bot-specific PlayerState, and no bot-specific
 * possession: a client receives a replicated fighter and cannot tell from the network which
 * ones are driven by a person.
 *
 * SEEDING IS THE POINT. One match seed enters here and is combined with each bot's SLOT
 * INDEX to make that bot's stream. Consequences that the soak depends on:
 *   - the same match seed reproduces the same match, bot for bot;
 *   - two bots in one match never draw the same sequence (they would think in lockstep);
 *   - a backfilled bot's seed is a function of its slot, not of when it arrived — so a
 *     replay does not depend on the wall-clock moment a human rage-quit.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class BREACHPOINT_API UBRBotManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBRBotManagerComponent();

	// -- Configuration (called by the GameMode from FBRMatchRules; all numbers are data) ---

	/**
	 * @param InMatchSeed          the match's deterministic seed (logged with every soak run).
	 * @param InRosterSize         combatants the match wants, humans included. From match rules.
	 * @param InBackfillDelay_s    seconds after a departure before the slot is refilled. From
	 *                             match rules — BP08's "10 s backfill" is a CSV value, not a
	 *                             literal in this file.
	 */
	void ConfigureRoster(int32 InMatchSeed, int32 InRosterSize, float InBackfillDelay_s);

	/**
	 * Read the tier scalars and the ambition table once, for every bot to share.
	 *
	 * Returns false (loudly) today: both row structs are still missing from BRDataRows.h —
	 * BP08 contract_gap 1. When it fails, no bots are spawned at all, which is the correct
	 * behaviour: an unconfigured bot in a shipped match is worse than a missing one.
	 */
	bool LoadBotTables(const UDataTable* TuningTable, const UDataTable* AmbitionsTable, const TArray<FName>& TierPerSlot);

	// -- Roster ---------------------------------------------------------------------------

	/** Spawn bots until the roster is full. Called from HandleMatchHasStarted. */
	void FillToRoster();

	/**
	 * A combatant left. Starts the backfill timer for that slot (never fills immediately —
	 * a reconnecting player should get their slot back, and a match that instantly papers
	 * over a departure hides a networking problem from the soak report).
	 */
	void NotifyCombatantLeft(AController* Leaver);

	/** A human joined. If a bot occupies a slot, the LAST-spawned bot yields it. */
	void NotifyHumanJoined();

	/** Every bot this component currently owns. */
	const TArray<TObjectPtr<ABRBotController>>& GetBots() const { return Bots; }

	/** Bots currently alive in the roster. */
	int32 GetBotCount() const { return Bots.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Spawn, configure and restart one bot into the given slot. */
	ABRBotController* SpawnBotForSlot(int32 SlotIndex);

	/** Destroy a bot and free its slot. */
	void RemoveBot(ABRBotController* Bot);

	/** Timer callback: the backfill window elapsed. */
	void HandleBackfillElapsed();

	/**
	 * The bot's seed: match seed combined with its SLOT, never with time or spawn order.
	 * Pure and reproducible — this one line is what makes "seeds[] reproduce" a checkable
	 * claim rather than a hope.
	 */
	int32 MakeSeedForSlot(int32 SlotIndex) const;

	/** Humans currently connected (PlayerControllers). Bots are AIControllers and are not counted. */
	int32 CountHumans() const;

private:
	UPROPERTY()
	TArray<TObjectPtr<ABRBotController>> Bots;

	/** Tier row name per slot, from data. Slot i gets TierPerSlot[i % Num]. */
	UPROPERTY()
	TArray<FName> TierRoster;

	/** One resolved tier-scalar set per distinct tier name. */
	UPROPERTY()
	TMap<FName, FBRBotTierScalars> TierScalarsByName;

	/** The ambition table, read once, shared by every brain (they never mutate it). */
	UPROPERTY()
	TArray<FBRBotAmbitionDef> AmbitionDefs;

	int32 MatchSeed = 0;
	int32 RosterSize = 0;
	float BackfillDelay_s = 0.f;
	bool bTablesLoaded = false;

	FTimerHandle BackfillTimer;
};
