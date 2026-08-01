// Breachpoint. The slice's IBRServerLifecycle: one process, one host, no migration.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/TimerHandle.h"
#include "Online/BRServerLifecycle.h"

#include "BRListenServerLifecycle.generated.h"

class UGameInstance;

/**
 * UBRListenServerLifecycle — ARCHITECTURE §3.8 unit 3; implementation #1 of `IBRServerLifecycle`.
 * Implementation #2 (`UBRGameLiftLifecycle`, GL-1) replaces this class and nothing else.
 *
 * THIS IS THE ONLY FILE IN THE PROJECT ALLOWED TO ASSUME A LISTEN SERVER
 * (`online-services.md`: "nothing at this scope may assume the host has a local player *except*
 * explicitly-marked listen-server-only code behind the interface"). It assumes: the server
 * shares a process with a player, the host's exit is the server's exit, and there is no
 * placement service issuing per-player credentials. Every one of those assumptions is wrong
 * under GameLift, and every one of them is invisible above the interface.
 *
 * =============================================================================================
 * THE HOST-QUIT DECISION (BP11's "defined, not discovered" — `online-services.md` law 4)
 * =============================================================================================
 *
 * DECISION: **when the host leaves, the match ends for everyone. There is no host migration,
 * not now and not in the slice.** The eight other players are returned to the main menu with a
 * named reason, in bounded time, with their end-of-match scoreboard intact.
 *
 * Why not migration: migration on a listen topology means re-hosting a live simulation on a
 * machine that has none of the server's authoritative state (GAS prediction keys, damage
 * ledger, respawn timers). Every honest implementation restarts the match anyway. Phase 2's
 * answer to host-quit is a dedicated server, not a migration protocol (R16 gates that with the
 * abandonment telemetry this class stamps — see BRTelemetrySubsystem).
 *
 * THE SEQUENCE, host side (all four host-exit paths converge on RequestHostingEnd):
 *   1. `UBRSessionsSubsystem::HostQuitToMainMenu()`  — the front end's "Quit to menu" button.
 *   2. `NotifyMatchComplete()`                        — the normal end of a match.
 *   3. `FCoreDelegates::OnEnginePreExit`              — alt-F4 / `quit`, BEST EFFORT ONLY.
 *   4. Any future path                                — there is one funnel; use it.
 *   => RequestHostingEnd(Reason)
 *      a. latch (idempotent — a second call logs and returns)
 *      b. state Hosting -> Ending; IsAcceptingPlayers() goes false, so a join racing the quit
 *         is REJECTED with a defined reason instead of landing in a dying world.
 *      c. broadcast OnHostingEnding synchronously — telemetry folds and stamps the match
 *         outcome (abandoned vs completed) HERE, while the world still exists.
 *      d. send every remote primary PlayerController
 *         `ClientReturnToMainMenuWithTextReason(<reason text>)` — a reliable RPC, so remotes
 *         get a clean, immediate, explained disconnect instead of a 30-second timeout.
 *      e. wait `HostingEndGraceSeconds` (default 2 s, clamped [0, 5]) on the GAME INSTANCE
 *         timer manager — which survives the level teardown that a world timer would not —
 *         so step (d)'s RPCs actually flush.
 *      f. state Ending -> Ended. `UBRSessionsSubsystem` observes that transition and does the
 *         platform work (destroy session, return the host to the front end). This class never
 *         touches the OSS: session state is the sessions subsystem's, per law 6.
 *
 * THE SEQUENCE, remote side (see UBRSessionsSubsystem's disconnect classifier):
 *   - Graceful (steps d-f above): the client is returned to the main menu with no network
 *     failure, and the sessions subsystem classifies it `EBRDisconnectReason::HostEndedSession`.
 *   - Ungraceful (host alt-F4 mid-frame, power loss, cable pull): the client sees
 *     `ENetworkFailure::ConnectionLost/ConnectionTimeout` and the sessions subsystem classifies
 *     it `EBRDisconnectReason::ConnectionLost`.
 *   Both broadcast the SAME delegate with a defined FText. There is no third outcome, and in
 *   particular there is no "sit in a dead world hoping it comes back" state.
 *
 * KNOWN LIMIT, STATED OUT LOUD: path 3 (alt-F4) reaches us at `OnEnginePreExit`, which is far
 * too late to flush a reliable RPC. The host-quit path is therefore correct-and-explained for
 * paths 1, 2, 4 and best-effort for path 3, where remotes fall back to the ConnectionLost
 * classification. That fallback is why both reasons land on one delegate: the UI has one state
 * to render either way and no undefined case to discover in a playtest.
 *
 * NO TICK (law 4): four timers' worth of nothing. The grace window is one one-shot timer; the
 * health pulse is off by default and is a diagnostic, not gameplay.
 */
UCLASS(Config = Game)
class BREACHPOINT_API UBRListenServerLifecycle : public UObject, public IBRServerLifecycle
{
	GENERATED_BODY()

public:
	//~ IBRServerLifecycle
	virtual bool InitializeHosting(UGameInstance* InGameInstance) override;
	virtual void NotifyServerReadyForPlayers() override;
	virtual FBRJoinVerdict ValidateJoin(const FBRJoinRequest& Request) override;
	virtual void NotifyPlayerJoined(const FString& PlatformIdString) override;
	virtual void NotifyPlayerLeft(const FString& PlatformIdString) override;
	virtual void NotifyMatchComplete(const FBRMatchResultSummary& Summary) override;
	virtual void RequestHostingEnd(EBRHostingEndReason Reason) override;

	virtual FBRHostingEndingSignature& OnHostingEnding() override { return HostingEndingEvent; }
	virtual FBRHostingStateChangedSignature& OnHostingStateChanged() override { return HostingStateChangedEvent; }
	virtual FBRHostingHealthPulseSignature& OnHostingHealthPulse() override { return HostingHealthPulseEvent; }

	virtual EBRHostingState GetHostingState() const override { return HostingState; }
	virtual bool IsAcceptingPlayers() const override { return HostingState == EBRHostingState::Hosting; }
	//~ End IBRServerLifecycle

	//~ UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	/** Humans currently admitted. Diagnostics and the match-result summary; never a gameplay input. */
	int32 GetAdmittedPlayerCount() const { return AdmittedPlayerIds.Num(); }

	/** The last summary handed over by the match frame, if any. Read by telemetry at ending time. */
	const FBRMatchResultSummary& GetLastMatchSummary() const { return LastMatchSummary; }

protected:
	/**
	 * Seconds between `OnHostingEnding` and teardown. Long enough for the remote kick RPCs to
	 * flush; short enough that nobody stares at a frozen host. Clamped to [0, 5] on use — a
	 * config typo may not turn this into a hang (services law 7).
	 */
	UPROPERTY(Config)
	float HostingEndGraceSeconds = 2.f;

	/**
	 * DIAGNOSTIC ONLY, DEFAULT OFF. Seconds between synthetic `OnHostingHealthPulse` firings so
	 * consumers of the health event can be tested without a cloud fleet. 0 disables it, which is
	 * the shipping value: a listen server answers to no external health authority.
	 */
	UPROPERTY(Config)
	float DiagnosticHealthPulseIntervalSeconds = 0.f;

	/** State transition, with the event. The ONLY writer of HostingState. */
	void SetHostingState(EBRHostingState NewState);

	/** Step (d): the reliable, explained kick. Listen-only by definition. */
	void ReturnRemotePlayersToMainMenu(const FText& ReasonText);

	/** Step (f): fired by the grace timer, or immediately when the grace window is zero. */
	void CompleteHostingEnd();

	/** Path 3: the process is going down without anyone telling us first. */
	void HandleEnginePreExit();

	void HandleDiagnosticHealthPulse();

	/** The player-facing text for each ending reason. Every reason has one; none is empty. */
	static FText MakeRemoteMessage(EBRHostingEndReason Reason);

	/** The stable non-PII code for logs and telemetry. */
	static FString MakeDiagnosticCode(EBRHostingEndReason Reason);

	/** Null-safe world lookup through the game instance; null once the world is gone. */
	UWorld* GetHostWorld() const;

private:
	UPROPERTY()
	TWeakObjectPtr<UGameInstance> GameInstance;

	EBRHostingState HostingState = EBRHostingState::Uninitialized;

	/** The latch that makes RequestHostingEnd idempotent. */
	bool bHostingEndLatched = false;

	EBRHostingEndReason EndReason = EBRHostingEndReason::Unknown;

	FBRMatchResultSummary LastMatchSummary;

	/** Platform id strings of admitted players. Identity only; never leaves this class. */
	TSet<FString> AdmittedPlayerIds;

	FTimerHandle GraceTimerHandle;
	FTimerHandle HealthPulseTimerHandle;

	FDelegateHandle EnginePreExitHandle;

	FBRHostingEndingSignature HostingEndingEvent;
	FBRHostingStateChangedSignature HostingStateChangedEvent;
	FBRHostingHealthPulseSignature HostingHealthPulseEvent;
};
