// Breachpoint. Platform session state: host, find, join, invite, leave — and nothing else.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ScriptInterface.h"
#include "Online/BRServerLifecycle.h"

#include "BRSessionsSubsystem.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class UNetDriver;
class UWorld;
struct FUniqueNetIdRepl;

/**
 * BRSessionsSubsystem — ARCHITECTURE §3.8 unit 1, `online-services.md` (whole file),
 * ticket BP11 step 1.
 *
 * WHAT IT OWNS (law 6, so the lobby boundary is named and not ambient): **platform session
 * state**. Create/find/join/invite/leave, plus the classification of how a client left. It does
 * NOT own the roster (that assembles in-match during Warmup, on PlayerState), it does NOT own
 * match rules (GameMode), and it does NOT own loadout/ready (that is `BRLobbySubsystem`, Phase
 * 2 — a feature that starts blurring the two files a contract_gap FIRST).
 *
 * WHAT IT HIDES (law 1): the backend. Callers say "host", "find and join the best session",
 * "invite", "leave". They never see `IOnlineSubsystem`, an `FOnlineSessionSearchResult`, a
 * Steam id, or a connect string — the whole OSS surface is private below. Swapping Steam for
 * FlexMatch replaces the bodies of `HostListenSession`/`FindAndJoinBestSession` and nothing
 * above them. A caller that asks "is this Steam?" is a finding.
 *
 * WHAT IT NEVER TRUSTS (law 2): the platform proves **identity and session membership**. That
 * is the whole grant. No session datum selects a team, a loadout, a spawn, or any gameplay
 * authority; admission (`ValidateIncomingJoin` -> `IBRServerLifecycle::ValidateJoin`) may only
 * say yes or no to entering the match.
 *
 * ==============================================================================================
 * DELEGATE SURFACE AND FIRING ORDER — this is API. Consumers bind; nobody polls (law 5).
 * ==============================================================================================
 *
 * `OnSessionStateChanged` fires FIRST for every transition, before the specific delegate that
 * describes the same step. So a handler of any delegate below may call `GetSessionState()` and
 * will always see the NEW state. Every operation ends in exactly one specific delegate,
 * success or failure — there is no path that ends in silence (law 7).
 *
 * HOST (local player is the host; no travel-in, the world becomes the server):
 *   HostListenSession()
 *     -> OnSessionStateChanged(Idle -> Creating)
 *     -> [OSS CreateSession completes]
 *     -> OnSessionStateChanged(Creating -> Hosting)        (success)
 *        OnHostSessionCreated(result.bSucceeded = true)
 *        [travel to the arena as a listen server]
 *        -> OnSessionStateChanged stays Hosting; when the map is up:
 *           IBRServerLifecycle::NotifyServerReadyForPlayers()
 *     -> OnSessionStateChanged(Creating -> Idle)           (failure)
 *        OnHostSessionCreated(result.bSucceeded = false)
 *        OnSessionFailure(same result)                     <- always AFTER the specific one
 *
 * FIND + JOIN (remote client; ends in a client travel):
 *   FindAndJoinBestSession()
 *     -> OnSessionStateChanged(Idle -> Searching)
 *     -> [OSS FindSessions completes]
 *        OnSessionSearchCompleted(NumResults, result)
 *     -> (no results) OnSessionStateChanged(Searching -> Idle) + OnSessionFailure(NoSessionsFound)
 *     -> (a winner)   OnJoinTargetResolved(target)         <- the variant surfaces HERE
 *                     OnSessionStateChanged(Searching -> Joining)
 *     -> [OSS JoinSession completes]
 *        (success) OnSessionJoined(result)
 *                  OnSessionStateChanged(Joining -> Travelling)
 *                  OnClientTravelStarting(target)
 *                  [map loads] -> OnSessionStateChanged(Travelling -> InSession)
 *        (failure) OnSessionJoined(result.bSucceeded = false)
 *                  OnSessionStateChanged(Joining -> Idle)
 *                  OnSessionFailure(same result)
 *
 * INVITE (Steam only; see the OSS Null note below):
 *   ShowInviteUI()            -> platform overlay; no delegate of ours (the overlay is the UX)
 *   [friend accepts]          -> OnInviteAccepted(target) -> then the JOIN sequence above from
 *                                OnSessionStateChanged(Idle -> Joining) onward.
 *
 * LEAVING, ALL FOUR WAYS — every one of them ends in `OnDisconnected` with a defined reason:
 *   LeaveSessionAndReturnToMainMenu()  -> reason LocalPlayerLeft
 *   host quit / match end (remote view)-> reason HostEndedSession
 *   host or network died (remote view) -> reason ConnectionLost
 *   server refused/kicked us           -> reason Kicked / JoinRejected / VersionMismatch
 *
 * ==============================================================================================
 * JOIN-IN-PROGRESS IS FIRST CLASS (law 3)
 * ==============================================================================================
 * The session is created with `bAllowJoinInProgress = true` and is NOT closed when the match
 * goes Live: this subsystem watches the GameMode's match state and calls StartSession/EndSession
 * so the platform's view stays honest without ever refusing a late joiner. Every consumer of
 * these delegates must survive arriving mid-match: `GetActiveJoinTarget()` may be default,
 * PlayerState may be null for the first frames, and the killfeed/scoreboard arrive already
 * populated. "Worked from map start" is half a test.
 *
 * ==============================================================================================
 * WHAT THIS FILE CANNOT PROVE WITHOUT STEAM (honesty law)
 * ==============================================================================================
 * Under `OSS Null` (PIE, and any build without `[OnlineSubsystem] DefaultPlatformService=Steam`)
 * create/find/join work over LAN and every state machine, delegate order, admission call and
 * host-quit path is exercisable. INVITES ARE NOT: `ShowInviteUI` has no overlay to open and the
 * invite-accepted callback never fires. Any claim about invites names the rung — "works (Steam,
 * 2 machines)" — or it is an opinion.
 *
 * NO TICK (law 4): every path here is an OSS completion delegate, an engine delegate, or a
 * one-shot timeout timer.
 */

// =============================================================================================
// Vocabulary
// =============================================================================================

namespace BRSessions
{
	/**
	 * The platform session name. Deliberately the same FName the engine's own `AGameSession`
	 * uses for player registration (`NAME_GameSession` == FName("GameSession")); spelled as a
	 * literal here so this header does not depend on which OSS header happens to export that
	 * symbol in a given engine version. Changing it would silently split our session from the
	 * one the engine registers players into.
	 */
	inline const FName SessionName = FName(TEXT("GameSession"));

	/**
	 * Search keyword, so `FindAndJoinBestSession` never returns another product's lobby that
	 * happens to share the Steam App ID during development.
	 */
	inline const FName KeywordValue = FName(TEXT("BREACHPOINT"));

	/** 4v4. The platform's slot count; bots fill empty slots in-game and are not session members. */
	inline constexpr int32 DefaultMaxPlayers = 8;
}

/** Where this machine is in the session flow. One value; every delegate below is derived from it. */
UENUM()
enum class EBRSessionState : uint8
{
	/** No platform session. The front end's resting state. */
	Idle,
	/** CreateSession in flight. */
	Creating,
	/** We own the session and are (or are becoming) the server. */
	Hosting,
	/** FindSessions in flight. */
	Searching,
	/** JoinSession in flight. */
	Joining,
	/** Joined at the platform level; the client travel is running. */
	Travelling,
	/** We are a client inside the host's match. */
	InSession,
	/** DestroySession in flight. */
	Leaving
};

/**
 * Why an operation failed. EVERY failure has a value here and a player-facing FText in
 * `FBRSessionOpResult::UserMessage` — services law 7: failure is a state, never a silent hang.
 */
UENUM()
enum class EBRSessionFailure : uint8
{
	None,
	/** No OSS at all (module missing/misconfigured). Config problem, not a player problem. */
	NoOnlineSubsystem,
	/** OSS present but exposes no session interface. */
	NoSessionInterface,
	/** No local player / no valid platform identity to act as. */
	NoLocalPlayer,
	/** Refused because we are already in or entering a session. */
	AlreadyInSession,
	/** The backend rejected CreateSession. */
	CreateFailed,
	/** The backend rejected or errored FindSessions. */
	SearchFailed,
	/** The search succeeded and returned nothing joinable. */
	NoSessionsFound,
	/** JoinSession failed for a reason without a more specific value. */
	JoinFailed,
	/** The chosen session filled up between search and join — the classic race. */
	SessionFull,
	/** The chosen session no longer exists (host quit between search and join). */
	SessionGone,
	/** Joined, but the backend could not produce a connect string. */
	AddressResolveFailed,
	/** The travel itself failed (map missing, version mismatch, pending connection failure). */
	TravelFailed,
	/** DestroySession failed; we return to the front end anyway. */
	DestroyFailed,
	/** The server's admission hook said no. */
	AdmissionRejected,
	/** The operation timed out on our side. */
	Timeout,
	/** The platform cannot do this here — e.g. invites under OSS Null. Not an error, a limit. */
	Unsupported
};

/** How a client's participation ended. Every departure produces exactly one of these. */
UENUM()
enum class EBRDisconnectReason : uint8
{
	None,
	/** This player chose to leave. */
	LocalPlayerLeft,
	/** The host ended the session gracefully: host quit, or the match completed. */
	HostEndedSession,
	/** The host or the network vanished. The ungraceful twin of HostEndedSession. */
	ConnectionLost,
	/** The server explicitly refused/removed us after we were connected. */
	Kicked,
	/** Admission said no. */
	JoinRejected,
	/** Client and server builds disagree. */
	VersionMismatch,
	/** Classified as nothing else. Still a defined UI state; never a hang. */
	Unknown
};

/** The result of one session operation. Carried by every op delegate, success or failure. */
USTRUCT()
struct FBRSessionOpResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSucceeded = false;

	UPROPERTY()
	EBRSessionFailure Failure = EBRSessionFailure::None;

	/** Player-facing, localizable. Non-empty for every failure. */
	UPROPERTY()
	FText UserMessage;

	/** Short, stable, non-PII code for logs and telemetry. Never contains an id or a token. */
	UPROPERTY()
	FString DiagnosticCode;

	static FBRSessionOpResult Success()
	{
		FBRSessionOpResult Result;
		Result.bSucceeded = true;
		return Result;
	}
};

/** How a client's session ended, in the shape the UI needs to render a defined state. */
USTRUCT()
struct FBRDisconnectNotice
{
	GENERATED_BODY()

	UPROPERTY()
	EBRDisconnectReason Reason = EBRDisconnectReason::None;

	/** Player-facing. Never empty except for LocalPlayerLeft (nothing to explain). */
	UPROPERTY()
	FText UserMessage;

	UPROPERTY()
	FString DiagnosticCode;

	/** True when this machine was the host. Host and remote render different end states. */
	UPROPERTY()
	bool bWasHost = false;
};

/** Everything `HostListenSession` needs. Defaults are the shipping slice's values. */
USTRUCT()
struct FBRHostSessionParams
{
	GENERATED_BODY()

	/** Platform slots. 4v4 => 8; bots are not session members. */
	UPROPERTY()
	int32 MaxPlayers = BRSessions::DefaultMaxPlayers;

	/**
	 * LAN session. TRUE is the only thing that works under OSS Null, which is why the front end
	 * must not hardcode false: PIE/LAN testing is a supported rung, not a hack.
	 */
	UPROPERTY()
	bool bIsLANMatch = false;

	/**
	 * Invite-first topology (`online-services.md`: quickmatch is cut). When true the session is
	 * still advertised, but only friends may join via presence.
	 */
	UPROPERTY()
	bool bFriendsOnly = true;

	/**
	 * Map to travel to as a listen server. A travel URL, not an asset reference — law 3's soft-ref
	 * rule is about UObject references in data, and this is neither. Overridable from config so a
	 * test map does not need a code change.
	 */
	UPROPERTY()
	FString MapPath;

	/** Extra travel options, appended after the map (e.g. "?game=..."). Rarely used. */
	UPROPERTY()
	FString ExtraTravelOptions;
};

/** Everything `FindAndJoinBestSession` needs. */
USTRUCT()
struct FBRSessionQuery
{
	GENERATED_BODY()

	UPROPERTY()
	int32 MaxResults = 25;

	/** Must match the host's `bIsLANMatch` or nothing is found. OSS Null: true. */
	UPROPERTY()
	bool bLanQuery = false;

	/** Reject sessions with fewer than this many open slots. */
	UPROPERTY()
	int32 MinOpenSlots = 1;

	/**
	 * Include matches already in progress. Defaults TRUE: join-in-progress is a first-class path
	 * (law 3), and a slice with 8 slots and no quickmatch cannot afford to hide half its sessions.
	 */
	UPROPERTY()
	bool bIncludeMatchesInProgress = true;

	/** Our own timeout, so a backend that never calls back still produces a defined failure. */
	UPROPERTY()
	float TimeoutSeconds = 20.f;
};

// =============================================================================================
// Delegates. Native multicast (no Blueprint surface — R18).
// =============================================================================================

/** Always fires FIRST for a transition, before the specific delegate for the same step. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRSessionStateChangedSignature, EBRSessionState /*Old*/, EBRSessionState /*New*/);
/** Host create finished. Fires exactly once per HostListenSession call, success or failure. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRSessionOpCompleteSignature, const FBRSessionOpResult& /*Result*/);
/** Search finished. NumResults is post-filter (how many were actually joinable). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRSessionSearchCompleteSignature, int32 /*NumJoinable*/, const FBRSessionOpResult& /*Result*/);
/** A join target was resolved (search winner, or an accepted invite). The variant surfaces here. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRJoinTargetSignature, const FBRJoinTarget& /*Target*/);
/** Catch-all failure, for UI that does not care WHICH operation failed. Fires after the specific one. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRSessionFailureSignature, const FBRSessionOpResult& /*Result*/);
/** This machine's participation ended. The one delegate that renders every leave/kick/host-quit. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRDisconnectedSignature, const FBRDisconnectNotice& /*Notice*/);
/**
 * PLATFORM membership changed (server only): how many players the session holds, of how many
 * slots. This is NOT the gameplay roster — the roster assembles in-match on PlayerState during
 * Warmup (law 6). A UI that renders a lobby list from this delegate is blurring the boundary.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRSessionMembershipChangedSignature, int32 /*NumRegistered*/, int32 /*MaxPlayers*/);

UCLASS(Config = Game)
class BREACHPOINT_API UBRSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	// =========================================================================================
	// THE PUBLIC SURFACE. Six verbs. None of them names a backend.
	// =========================================================================================

	/**
	 * Create a session and travel to the arena as a listen server.
	 * HOST BEHAVIOUR: no client travel; this world becomes the server and the local player is
	 * already in it. REMOTE BEHAVIOUR: none — remotes arrive via FindAndJoinBestSession/invite.
	 * Returns false only for an immediate refusal (already in a session, no OSS); asynchronous
	 * failures arrive on OnHostSessionCreated + OnSessionFailure.
	 */
	bool HostListenSession(const FBRHostSessionParams& Params);

	/**
	 * THE SEAM (law 1). Find the best session for this player and join it. Steam OSS today;
	 * FlexMatch/placement later, behind this exact signature. Callers never learn which.
	 * Ends in a client travel on success.
	 */
	bool FindAndJoinBestSession(const FBRSessionQuery& Query);

	/**
	 * Join a target that was already resolved (search winner, accepted invite, or — Phase 2 — a
	 * placement result). Public so the invite path and a future matchmaker share ONE join path.
	 */
	bool JoinResolvedTarget(const FBRJoinTarget& Target);

	/**
	 * Open the platform's invite overlay for our session. Returns false when the platform has no
	 * overlay (OSS Null) — an honest "unsupported here", not a silent no-op.
	 */
	bool ShowInviteUI();

	/**
	 * Leave voluntarily. Host and remote are DIFFERENT paths and this routes to the right one:
	 * a host's "leave" is a host-quit (which ends the match for everyone — see HostQuitToMainMenu);
	 * a remote's "leave" only removes that player. Produces OnDisconnected either way.
	 */
	void LeaveSessionAndReturnToMainMenu();

	/**
	 * THE HOST-QUIT ENTRY POINT. The front end's "Quit to menu" calls this, on host and remote
	 * alike. On a remote it is exactly LeaveSessionAndReturnToMainMenu(); on the host it hands
	 * over to `IBRServerLifecycle::RequestHostingEnd(HostQuit)` — which ends the match for all
	 * nine players with a defined, explained state. See BRListenServerLifecycle.h for the full
	 * decision and its sequence.
	 */
	void HostQuitToMainMenu();

	// =========================================================================================
	// Match-frame handoff. The GameMode's only two lifecycle calls.
	// =========================================================================================

	/**
	 * Server only. The match reached a decided end and its post-match window has elapsed.
	 * Routes to `IBRServerLifecycle::NotifyMatchComplete` -> RequestHostingEnd(MatchComplete).
	 * Intended caller: `ABRGameMode::RequestMatchTeardown()` (BP04 left that seam a log-only stub).
	 */
	void RequestMatchTeardown(const FBRMatchResultSummary& Summary);

	/**
	 * Server only. SEAM GAP 1's call site: run admission for one incoming player.
	 * Called automatically for every login through the engine's PreLogin event (see
	 * `OnPlayerPreLogin`), so no join path can bypass it. Public because the credential-
	 * carrying form needs `Options`/`Address`, which the engine event does not provide — a
	 * GameMode override forwards those here.
	 */
	FBRJoinVerdict ValidateIncomingJoin(const FBRJoinRequest& Request);

	// =========================================================================================
	// Access + state
	// =========================================================================================

	/**
	 * The lifecycle seam. Returns an interface, never a concrete class: `Cast` to a concrete
	 * lifecycle outside `Online/` is a finding. May be null before hosting is initialized.
	 */
	TScriptInterface<IBRServerLifecycle> GetServerLifecycle() const { return ServerLifecycle; }

	EBRSessionState GetSessionState() const { return SessionState; }

	/** True once we are actually in a match world (host or remote). */
	bool IsInSession() const { return SessionState == EBRSessionState::Hosting || SessionState == EBRSessionState::InSession; }

	/** True when THIS machine created the session. A role, not a backend — safe to branch on. */
	bool IsHosting() const { return bIsHost; }

	/** The target we joined (or resolved). Default-constructed on the host and in the front end. */
	const FBRJoinTarget& GetActiveJoinTarget() const { return ActiveJoinTarget; }

	/** Why we last left a session. Survives the return to the front end so the UI can render it. */
	const FBRDisconnectNotice& GetLastDisconnectNotice() const { return LastDisconnectNotice; }

	// =========================================================================================
	// Delegates — see the firing-order block at the top of this file.
	// =========================================================================================

	FBRSessionStateChangedSignature OnSessionStateChanged;
	FBRSessionOpCompleteSignature OnHostSessionCreated;
	FBRSessionSearchCompleteSignature OnSessionSearchCompleted;
	FBRJoinTargetSignature OnJoinTargetResolved;
	FBRJoinTargetSignature OnInviteAccepted;
	FBRSessionOpCompleteSignature OnSessionJoined;
	FBRJoinTargetSignature OnClientTravelStarting;
	FBRSessionFailureSignature OnSessionFailure;
	FBRDisconnectedSignature OnDisconnected;
	FBRSessionMembershipChangedSignature OnSessionMembershipChanged;

protected:
	/**
	 * The arena the host travels to. Config so a test map is an ini line, not a code change.
	 * NOTE (BP11): `/Game/Maps/BR_Arena01` does not exist yet — `Content/Maps/` is empty at the
	 * time of writing. Hosting fails with a defined TravelFailed until the blockout lands.
	 */
	UPROPERTY(Config)
	FString DefaultArenaMapPath = TEXT("/Game/Maps/BR_Arena01");

	/**
	 * Seconds to wait for an OSS callback before declaring Timeout ourselves. A backend that
	 * never calls back must still produce a defined UI state (law 7).
	 */
	UPROPERTY(Config)
	float OperationTimeoutSeconds = 30.f;

private:
	// --- state machine -----------------------------------------------------------------------
	void SetSessionState(EBRSessionState NewState);

	/**
	 * THE ONE FAILURE PATH, so the documented order cannot be got wrong at a call site:
	 *   1. state moves to `RecoveryState`   (OnSessionStateChanged)
	 *   2. `SpecificDelegate` fires with the failed result, if the operation has one
	 *   3. `OnSessionFailure` fires with the same result
	 * Pass nullptr for step 2 when the operation's specific delegate has a different signature
	 * (the search delegate) and must be fired by the caller between 1 and 3.
	 */
	FBRSessionOpResult FailOperation(EBRSessionFailure Failure, const FText& UserMessage, const FString& Code,
		EBRSessionState RecoveryState, FBRSessionOpCompleteSignature* SpecificDelegate);

	// --- OSS completion handlers ---------------------------------------------------------------
	// NOTE THE PARAMETER TYPES: not one of them is an OSS type. The OSS delegates are bound in
	// the .cpp with CreateWeakLambda, which converts at the boundary (an `EOnJoinSessionComplete
	// Result::Type` becomes an int32 here). That is what keeps this header — and therefore every
	// consumer — free of `IOnlineSubsystem`, and what makes the FlexMatch swap a .cpp-only edit.
	void OnCreateSessionFinished(bool bWasSuccessful);
	void OnStartSessionFinished(bool bWasSuccessful);
	void OnFindSessionsFinished(bool bWasSuccessful);
	void OnJoinSessionFinished(int32 RawJoinResult);
	void OnDestroySessionFinished(bool bWasSuccessful);
	/** `ResolvedIndex` addresses the search-state table; INDEX_NONE means the invite was unusable. */
	void OnInviteAcceptedInternal(bool bWasSuccessful, int32 ResolvedIndex);

	void ClearAllOssDelegates();
	void StartOperationTimeout(EBRSessionFailure OnTimeoutFailure, const FString& Code);
	void ClearOperationTimeout();
	void HandleOperationTimeout();

	// --- engine hooks (zero-intrusion: no other packet's file is touched) ---------------------
	// Bound with weak lambdas in the .cpp for the same reason as above; these take only plain types.
	void OnPlayerPreLogin(AGameModeBase* GameMode, const FString& PlatformIdString, FString& OutErrorMessage);
	void OnPlayerPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void OnPlayerLogout(AGameModeBase* GameMode, AController* Exiting);
	void OnMatchStateSet(FName MatchState);
	/** `RawFailureType` is `ENetworkFailure::Type` widened at the binding site. */
	void OnNetworkFailed(UWorld* FailedWorld, int32 RawFailureType, const FString& ErrorString);
	/** `RawFailureType` is `ETravelFailure::Type` widened at the binding site. */
	void OnTravelFailed(UWorld* FailedWorld, int32 RawFailureType, const FString& ErrorString);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandlePreLoadMap(const FString& MapName);
	void HandleHostingStateChanged(EBRHostingState OldState, EBRHostingState NewState);

	/** True when `OtherWorld` belongs to OUR game instance. PIE runs several in one process. */
	bool IsOurWorld(const UWorld* OtherWorld) const;

	// --- helpers ------------------------------------------------------------------------------
	void EnsureServerLifecycle();
	void BroadcastDisconnect(EBRDisconnectReason Reason, const FText& Message, const FString& Code);
	void DestroySessionAndReturnToFrontEnd(EBRDisconnectReason Reason, const FText& Message, const FString& Code);
	APlayerController* GetLocalPlayerController() const;
	int32 GetLocalControllerId() const;

	/** Picks the best search result; returns INDEX_NONE when nothing is joinable. */
	int32 SelectBestSearchResult(const FBRSessionQuery& Query) const;

	// --- data ---------------------------------------------------------------------------------

	/**
	 * The lifecycle implementation. Held as an interface reference so nothing here — or above —
	 * can accidentally depend on the listen implementation. Phase 2 changes the ONE NewObject
	 * call in EnsureServerLifecycle() and nothing else in this file.
	 */
	UPROPERTY()
	TScriptInterface<IBRServerLifecycle> ServerLifecycle;

	EBRSessionState SessionState = EBRSessionState::Idle;

	bool bIsHost = false;

	/** Set by LeaveSessionAndReturnToMainMenu so the disconnect classifier can tell intent apart. */
	bool bLocalLeaveRequested = false;

	/** Latched so a graceful host-end and a following network failure produce ONE notice. */
	bool bDisconnectNoticeLatched = false;

	FBRJoinTarget ActiveJoinTarget;
	FBRDisconnectNotice LastDisconnectNotice;
	FBRHostSessionParams PendingHostParams;
	FBRSessionQuery PendingQuery;

	/** The map we asked to travel to; used to recognise our own arrival in HandlePostLoadMap. */
	FString PendingTravelMapPath;

	/**
	 * Resolved search results, addressed by the opaque `FBRJoinTarget::SessionHandleId`.
	 * PIMPL'd behind a shared pointer so no OSS type appears in this header.
	 */
	TSharedPtr<class FBRSessionSearchState> SearchState;

	FTimerHandle OperationTimeoutHandle;
	EBRSessionFailure TimeoutFailure = EBRSessionFailure::Timeout;
	FString TimeoutCode;

	FDelegateHandle PreLoginHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	FDelegateHandle MatchStateHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle HostingStateHandle;
};
