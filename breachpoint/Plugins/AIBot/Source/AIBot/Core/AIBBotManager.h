#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIBBotManager.generated.h"

class AAIBBotController;
class IAIBAmbitionProvider;
class IAIBWorldQuery;

/**
 * PHASE 6 — the provider door's hinge (founder ruling, 26 Aug 2026, over the Phase-6
 * W-AUDIT: the manager owns PROVIDER RESOLUTION; spawning stays the host game mode's —
 * ARCHITECTURE law 3 carries the dated amendment).
 *
 * The host PUSHES its providers in once (RegisterProviders, authority only, from its
 * own lifecycle — this module never searches the world, which is what keeps the
 * boundary grep meaningful); controllers PULL at possession, so a respawn re-resolves
 * for free and a provider that died yields null, never a dangling call. Both handles
 * are weak: a mode object ending mid-match degrades bots to core ambitions, loudly at
 * the controller, never a crash.
 *
 * PHASE 14 — THE MATCH SEED and the STABLE BOT INDEX. Every per-life draw hashes
 * (MatchSeed, BotIndex, LifeIndex): the seed is `-AIBSeed=N` on the command line, else
 * the host's SetMatchSeed, else a hashed clock — resolved ONCE at first use and logged
 * once. GetUniqueID is not stable across runs, so the manager hands each controller its
 * spawn slot (first-possession order, never reused) — without it the seed is theatre.
 */
UCLASS()
class AIBOT_API UAIBBotManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The host's one call, on the authority. Either may be null (a Slayer host has no
	 *  ambition provider and that is correct); a client-world call is refused loudly. */
	void RegisterProviders(UObject* AmbitionProvider, UObject* WorldQuery);

	IAIBAmbitionProvider* GetAmbitionProvider() const;
	IAIBWorldQuery* GetWorldQuery() const;
	UObject* GetAmbitionProviderObject() const { return AmbitionProviderObject.Get(); }
	UObject* GetWorldQueryObject() const { return WorldQueryObject.Get(); }

	/** The host's seed (its game mode, before the first bot possesses). Overridden by
	 *  `-AIBSeed=N`; refused loudly once a bot has already drawn from the resolved seed. */
	void SetMatchSeed(int32 Seed);

	/** Resolves on first call (cmdline > host > hashed clock) and logs the one line:
	 *  `AIBot: match seed N (source=cmdline|host|clock).` */
	int32 GetMatchSeed();

	/** The controller's stable slot for this world: first-possession order, never reused. */
	int32 AssignBotIndex(const AAIBBotController& Bot);

private:
	TWeakObjectPtr<UObject> AmbitionProviderObject;
	TWeakObjectPtr<UObject> WorldQueryObject;

	int32 MatchSeed = 0;
	bool bMatchSeedResolved = false;
	bool bSeedFromCommandLine = false;
	bool bSeedFromHost = false;
	TArray<TWeakObjectPtr<const AAIBBotController>> BotSlots;
};
