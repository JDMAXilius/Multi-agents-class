#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIBBotManager.generated.h"

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
 */
UCLASS()
class AIBOT_API UAIBBotManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** The host's one call, on the authority. Either may be null (a Slayer host has no
	 *  ambition provider and that is correct); a client-world call is refused loudly. */
	void RegisterProviders(UObject* AmbitionProvider, UObject* WorldQuery);

	IAIBAmbitionProvider* GetAmbitionProvider() const;
	IAIBWorldQuery* GetWorldQuery() const;
	UObject* GetAmbitionProviderObject() const { return AmbitionProviderObject.Get(); }
	UObject* GetWorldQueryObject() const { return WorldQueryObject.Get(); }

private:
	TWeakObjectPtr<UObject> AmbitionProviderObject;
	TWeakObjectPtr<UObject> WorldQueryObject;
};
