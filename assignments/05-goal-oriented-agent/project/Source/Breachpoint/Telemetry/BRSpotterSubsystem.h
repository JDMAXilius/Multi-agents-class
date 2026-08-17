#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"

#include "BRSpotterSubsystem.generated.h"

class APlayerState;
class UBRTelemetrySubsystem;
class UDataTable;
enum class EBRSpotterAudience : uint8;
struct FBRKillFeedEntry;
struct FBRMatchTelemetryRecord;
struct FBRPlayerMatchTelemetry;
struct FBRSpotterLineRow;

/** A resolved line ready for display. `Subject` is who the line is about/for; may be null for a whole-lobby line. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FBRSpotterLineReadySignature, const APlayerState* /*Subject*/, EBRSpotterAudience, const FText& /*Line*/);

/**
 * `UBRSpotterSubsystem` -- the runtime Spotter agent (GDD §3.3). THE ONLY MODEL CALL IN THE
 * SHIPPED GAME, AND NEVER LOAD-BEARING: everything this class produces is flavor appended on top
 * of a match that already works without it -- medals and killfeed are canned and instant
 * elsewhere (`ABRGameState`), not here.
 *
 * TWO OUTPUTS, ONE PIPELINE. A per-event quip (`RequestEventLine`, ≤ 18 words, killfeed-adjacent)
 * and a per-player, telemetry-grounded coach line (`HandleMatchTelemetryFinalized`, ≤ 30 words,
 * fired once at match end). Both go through the same `ResolveAndDispatch`: try the model, budget
 * and timeout permitting; otherwise select a canned line from `DT_SpotterLines`. NO CONNECTIVITY
 * ⇒ THE GAME IS IDENTICAL MINUS FLAVOR -- the canned path is not a degraded mode, it is what
 * ships today, and the model path is a strictly additive upgrade to it.
 *
 * THE MODEL CALL DOES NOT EXIST YET. There is no HTTP/model client anywhere in this codebase.
 * `TryRequestModelLine` is an honest stub that always fails, gated behind `bEnableModelSpotter`
 * (defaulted false) so the failure path -- the canned fallback -- is the one every caller already
 * exercises today. `MaxModelCallsPerMatch` and `ModelRequestTimeoutSeconds` are reserved for that
 * future implementation, not consumed by the stub. Wiring a real client in only ever needs to
 * change `TryRequestModelLine`'s body; nothing else in this class assumes a particular transport.
 * §5.3 cut-order item 4 ("Spotter agent → canned coach lines only") IS `bEnableModelSpotter =
 * false` -- the cut is a config flip because the fallback already ships.
 *
 * IDENTITY WITHOUT A NEW LOOKUP TABLE. The finalized telemetry record only carries anonymized
 * `PlayerKey` integers (see `UBRTelemetrySubsystem`), not live `APlayerState*` pointers -- and it
 * should not gain a reverse-lookup just for this. Instead `HandleMatchTelemetryFinalized` walks
 * the live `PlayerArray` and calls the ALREADY-PUBLIC `UBRTelemetrySubsystem::GetPlayerKey`, which
 * returns the same key it handed out during the match for the same object identity. That is
 * enough to join a live player back to their stat row without inventing new API surface.
 *
 * NOTABLE-EVENT CLASSIFICATION IS NOT THIS CLASS'S JOB. Deciding a kill was a spree, a
 * multi-kill, or a grapple finish needs streak/grouping state that belongs to the medal system
 * (`FBRMedalRow` in `Data/BRDataRows.h`), which is not built in C++ yet. `HandleKillFeedEntryAdded`
 * is wired up as the documented landing point, not a guess at that logic -- see the TODO there.
 *
 * HOST-AUTHORITATIVE, LIKE `UBRTelemetrySubsystem`: every action is gated on `HasSpotterAuthority`
 * (net mode, not subsystem creation) so a client never selects, requests, or broadcasts a line --
 * it only ever receives one over the `OnSpotterLineReady` delegate from whatever replicates it.
 */
UCLASS(Config = Game)
class BREACHPOINT_API UBRSpotterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/**
	 * Ask for optional color commentary on a notable event (spree, rocket multi-kill, grapple
	 * kill, ...). `TriggerId` must match a `TriggerId` column in `DT_SpotterLines`. Silent (no
	 * broadcast) if every matching line is on cooldown or none is authored -- that is the
	 * `spotter_line | null` contract from the GDD, not a failure.
	 */
	void RequestEventLine(FName TriggerId, APlayerState* Subject);

	int32 GetModelCallsUsedThisMatch() const { return ModelCallsUsedThisMatch; }

	bool IsModelSpotterEnabled() const { return bEnableModelSpotter; }

	/** Fires once a line has been resolved and is ready to show. UI/HUD binds this; nothing here assumes a listener exists. */
	FBRSpotterLineReadySignature OnSpotterLineReady;

protected:
	/** DT_SpotterLines. Soft (law 3): the loading screen sequence must never pull match-flavor text into the cook root. */
	UPROPERTY(Config)
	TSoftObjectPtr<UDataTable> SpotterLinesTable;

	/**
	 * Off by default. The shipped build runs canned-only until a real model client exists; this
	 * is also the exact knob §5.3's cut order flips to drop the model path under schedule
	 * pressure.
	 */
	UPROPERTY(Config)
	bool bEnableModelSpotter = false;

	/** ≤ 12 calls/match per the GDD. Reserved for the real model path; the stub never consumes this. */
	UPROPERTY(Config)
	int32 MaxModelCallsPerMatch = 12;

	/** 3 s per the GDD. Reserved for the real model path; the stub never consumes this. */
	UPROPERTY(Config)
	float ModelRequestTimeoutSeconds = 3.f;

private:
	bool HasSpotterAuthority() const;

	void TryBindMatchSources();

	void HandleKillFeedEntryAdded(const FBRKillFeedEntry& Entry);
	void HandleMatchTelemetryFinalized(const FBRMatchTelemetryRecord& Record);

	/** One coach line for one human player, using their real finalized stats. */
	void EmitCoachLine(APlayerState* Subject, const FBRPlayerMatchTelemetry& Stats, uint8 WinningTeamId);

	/** The one place both outputs meet: model attempt (if enabled and budget allows), else canned lookup, else silence. */
	void ResolveAndDispatch(FName TriggerId, APlayerState* Subject);

	/**
	 * TODO(spotter-model-integration): no HTTP/model client exists in this codebase yet. When one
	 * lands, this must become an ASYNC request resolved through a callback/delegate -- this call
	 * happens on the game thread and must not block it -- armed with a `ModelRequestTimeoutSeconds`
	 * timer that falls through to the canned line on expiry, and its output must be moderated
	 * before `OnSpotterLineReady` broadcasts it (free-text model output, unlike the pre-reviewed
	 * rows in `DT_SpotterLines`). Until then this always fails, which is the correct behavior for
	 * a build that has not integrated a client: every caller already exercises the fallback.
	 */
	bool TryRequestModelLine(FName TriggerId, const APlayerState* Subject, FText& OutLine);

	/** Weighted-random pick among DT_SpotterLines rows matching `TriggerId` that are off cooldown. False = no legal candidate. */
	bool TrySelectCannedLine(FName TriggerId, FName& OutRowName, FBRSpotterLineRow& OutRow);

	void BroadcastLine(const APlayerState* Subject, EBRSpotterAudience Audience, const FText& Line);

	/** Resolve and cache the lines table. Null (logged once) means Spotter is silently a no-op -- legal, matches the loading-screen convention for an unconfigured soft asset. */
	UDataTable* GetOrLoadLinesTable();

	float GetServerTime() const;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedLinesTable;

	TWeakObjectPtr<UBRTelemetrySubsystem> BoundTelemetry;

	/** RowName -> the server time it was last selected, for `RepeatCooldown_s`. */
	TMap<FName, float> LineLastUsedServerTime;

	int32 ModelCallsUsedThisMatch = 0;

	bool bMatchSourcesBound = false;

	FDelegateHandle KillFeedHandle;
	FDelegateHandle TelemetryFinalizedHandle;
};
