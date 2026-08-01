// Breachpoint. THE SEAM: everything that changes when the server stops being a listen host.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "BRServerLifecycle.generated.h"

class APlayerController;
class UGameInstance;
class UWorld;

/**
 * BRServerLifecycle — ARCHITECTURE §3.8 unit 2, `online-services.md` law 1 + seam shape v1.1,
 * `BREACHPOINT-GAMELIFT-PLAN.md` §4. Header-only on purpose: the seam has no behaviour, only
 * shape. Behaviour lives in implementations (`UBRListenServerLifecycle` today,
 * `UBRGameLiftLifecycle` in Phase 2 — §3.8's reserved fourth unit).
 *
 * WHAT THIS INTERFACE IS FOR (read before adding a method):
 *   The whole game asks exactly four questions of "the server as a hosted thing":
 *     1. Is it up and willing to take players?      InitializeHosting / NotifyServerReadyForPlayers
 *     2. May THIS player in?                        ValidateJoin      (per-player admission)
 *     3. The match is over / must stop — now what?  NotifyMatchComplete / RequestHostingEnd
 *     4. Something upstream is ending us.           OnHostingEnding   (outbound event)
 *   Anything that is not one of those four is not a lifecycle question. Match rules are the
 *   GameMode's; replicated match state is the GameState's; platform session state is
 *   `UBRSessionsSubsystem`'s. This interface knows nothing about scores, teams, or Steam.
 *
 * THE ONE RULE FOR CALLERS (`online-services.md` law 1): a caller may never learn which
 * implementation it holds. There is deliberately NO `IsListenServer()`, no `GetBackendName()`,
 * and no `HasLocalHostPlayer()` on this interface — each would be an invitation to branch on
 * the vendor, and a caller that branches on the vendor is a finding. Listen-server-only
 * behaviour lives INSIDE the listen implementation, behind these methods, marked as such.
 *
 * WHY A GAMELIFT IMPLEMENTATION SLOTS IN WITHOUT TOUCHING A CALLER — the three v1.1 gaps:
 *   - Admission is a per-player hook (`ValidateJoin`) that already runs on every join today,
 *     accepting always. GameLift's `AcceptPlayerSession(playerSessionId)` is a body swap; no
 *     new call site, no new ordering.
 *   - Endings are an outbound EVENT (`OnHostingEnding`), and every reason — host quit, match
 *     complete, GameLift `ProcessEnding`, spot interruption, scale-down — arrives on that ONE
 *     event with a reason code. Consumers already handle "we are going down" generically, so
 *     adding cloud reasons adds no consumer code.
 *   - The join target is a variant (`FBRJoinTarget`), so "session handle" (Steam) and
 *     "ip:port + player-session id" (GameLift) are the same type to every caller.
 *
 * NO TICK (law 4): nothing here polls. Implementations use timers, SDK callbacks and engine
 * delegates. `OnHostingHealthPulse` exists because GameLift's SDK pushes a health callback —
 * it is an inbound push, never a poll the game performs.
 */

// =============================================================================================
// Reasons, states, verdicts — the vocabulary both implementations share.
// =============================================================================================

/**
 * Why hosting is ending. EVERY ending in the game maps onto one of these and travels on the
 * single `OnHostingEnding` event (seam gap 2). Adding a reason is cheap; adding a second
 * ending EVENT would fork every consumer and is a finding.
 */
UENUM()
enum class EBRHostingEndReason : uint8
{
	/** Never broadcast; the default of an unset notice. */
	Unknown,

	/** The match reached a decided end and teardown was requested by the match frame. */
	MatchComplete,

	/**
	 * LISTEN: the hosting player deliberately left (quit to menu / closed the game).
	 * There is no host migration in Breachpoint — see the host-quit block in
	 * BRListenServerLifecycle.h. In Phase 2 no GameLift server can produce this reason,
	 * and that difference is invisible to consumers.
	 */
	HostQuit,

	/** LISTEN: the host process or its network died without announcing itself. Best-effort. */
	HostConnectionLost,

	/**
	 * The hosting platform asked us to stop: GameLift `ProcessEnding`, fleet scale-down, spot
	 * interruption. Phase 2 only; present now so consumers are written against it from day one.
	 */
	PlatformRequested,

	/** An unrecoverable server-side error. Consumers treat it exactly like PlatformRequested. */
	Fault
};

/** Where the hosted server is in its own life. Monotonic: it never moves backwards. */
UENUM()
enum class EBRHostingState : uint8
{
	/** Constructed, nothing claimed. */
	Uninitialized,
	/** InitializeHosting() done; not yet accepting players. GameLift: after InitSDK. */
	Initializing,
	/** Accepting players. GameLift: after ProcessReady + OnStartGameSession. */
	Hosting,
	/** OnHostingEnding has fired; the grace window is running. Joins are refused from here on. */
	Ending,
	/** Hosting is over. Terminal. */
	Ended
};

/** The only two answers admission may give. There is no "maybe" and no "retry later". */
UENUM()
enum class EBRJoinAdmission : uint8
{
	Accepted,
	Rejected
};

/** Which shape of join target we are holding (seam gap 3). */
UENUM()
enum class EBRJoinTargetKind : uint8
{
	/** Nothing resolved. Passing this to a travel call is a programming error. */
	Invalid,

	/**
	 * Platform session handle — Steam today, any handle-shaped backend later. The handle is an
	 * OPAQUE COOKIE (`SessionHandleId`) into `UBRSessionsSubsystem`'s resolved-result table, so
	 * no `FOnlineSessionSearchResult` ever appears in a public type and no consumer can be
	 * tempted to read Steam fields off it.
	 */
	SessionHandle,

	/**
	 * A direct connect string ("ip:port"), the shape GameLift placement returns. Present from
	 * day one so the Phase-2 swap does not add a case to anyone's switch.
	 */
	Address
};

/**
 * FBRJoinCredential — the per-player admission token, opaque to everything but the lifecycle
 * implementation that issued it.
 *
 * TRUST (services law 2 / netcode fill-in): this is an INPUT. It proves the bearer was placed
 * into this session by the backend and NOTHING else. It never grants gameplay authority, never
 * skips an in-game check, and never selects a loadout, team, or spawn. On the listen server it
 * is empty and unused; the listen implementation accepts regardless of its contents, which is
 * the correct behaviour for a topology with no placement service — not a stub to "finish later".
 *
 * It is a string because the issuer decides its shape (GameLift: `playerSessionId`; a future
 * Lambda: a short-lived signed token). Parsing it anywhere outside an `IBRServerLifecycle`
 * implementation is a finding.
 */
USTRUCT()
struct FBRJoinCredential
{
	GENERATED_BODY()

	/** Opaque. Empty is legal and means "this backend does not issue credentials". */
	UPROPERTY()
	FString Token;

	bool IsEmpty() const { return Token.IsEmpty(); }

	/**
	 * The URL option this credential travels in, e.g. "BRJoin=abc123". Appended to the client
	 * travel URL by the sessions subsystem and read back out of `Options` on the server.
	 * Returns an empty string when there is no credential, so callers can append unconditionally.
	 */
	FString ToUrlOption() const
	{
		return Token.IsEmpty() ? FString() : FString::Printf(TEXT("%s=%s"), UrlOptionKey, *Token);
	}

	/** The option key. One spelling, one place — the server parses `Options` for exactly this. */
	static constexpr const TCHAR* UrlOptionKey = TEXT("BRJoin");
};

/**
 * FBRJoinTarget — seam gap 3: "the join target is a variant, not a handle."
 *
 * CALLER LAW: a caller may pass this to the travel/connect layer
 * (`UBRSessionsSubsystem::JoinResolvedTarget`) and may read `DisplayLabel` for UI. Anything
 * else — switching on `Kind`, reading `ConnectAddress`, inspecting `Credential` — is a finding.
 * The variant exists precisely so the Steam→GameLift swap never leaks upward; the ONE switch
 * on `Kind` in the whole codebase lives in `UBRSessionsSubsystem::JoinResolvedTarget` and is
 * labelled there.
 */
USTRUCT()
struct FBRJoinTarget
{
	GENERATED_BODY()

	UPROPERTY()
	EBRJoinTargetKind Kind = EBRJoinTargetKind::Invalid;

	/** SessionHandle variant: opaque cookie into the sessions subsystem's resolved-result table. */
	UPROPERTY()
	int32 SessionHandleId = INDEX_NONE;

	/** Address variant: "ip:port". Empty for the handle variant. */
	UPROPERTY()
	FString ConnectAddress;

	/** Per-player admission token, if the backend issues one. Empty on Steam/listen. */
	UPROPERTY()
	FBRJoinCredential Credential;

	/**
	 * Human-readable label for the front end ("Blake's game — Arena01, 5/8"). UI ONLY.
	 * It can contain a platform persona name, so it is PII-adjacent: it must never be written
	 * into telemetry, a log line shipped off the machine, or replicated state.
	 */
	UPROPERTY()
	FText DisplayLabel;

	/** Open player slots at resolve time; -1 when the backend did not say. UI/logging only. */
	UPROPERTY()
	int32 OpenSlots = -1;

	/** Round-trip estimate in ms at resolve time; -1 when unknown. UI/logging only. */
	UPROPERTY()
	int32 PingMs = -1;

	bool IsValid() const
	{
		switch (Kind)
		{
		case EBRJoinTargetKind::SessionHandle:	return SessionHandleId != INDEX_NONE;
		case EBRJoinTargetKind::Address:		return !ConnectAddress.IsEmpty();
		default:								return false;
		}
	}

	/** Non-PII, credential-free one-liner for LogBROnline. Never prints the token or the label. */
	FString ToLogString() const
	{
		switch (Kind)
		{
		case EBRJoinTargetKind::SessionHandle:
			return FString::Printf(TEXT("JoinTarget(handle #%d, slots=%d, ping=%d, cred=%s)"),
				SessionHandleId, OpenSlots, PingMs, Credential.IsEmpty() ? TEXT("none") : TEXT("present"));
		case EBRJoinTargetKind::Address:
			// The address is a game server endpoint, not a player datum; the token never prints.
			return FString::Printf(TEXT("JoinTarget(address %s, slots=%d, ping=%d, cred=%s)"),
				*ConnectAddress, OpenSlots, PingMs, Credential.IsEmpty() ? TEXT("none") : TEXT("present"));
		default:
			return TEXT("JoinTarget(invalid)");
		}
	}
};

/**
 * FBRJoinRequest — everything admission is allowed to look at, and nothing more.
 *
 * Deliberately NOT a PlayerController: the real admission moment is `PreLogin`, before any
 * controller exists. Phase 2's `AcceptPlayerSession` runs at exactly that moment too, which is
 * why the hook is shaped this way now rather than "once we have a pawn".
 */
USTRUCT()
struct FBRJoinRequest
{
	GENERATED_BODY()

	/**
	 * The travel URL's option string as the server received it. The credential is extracted
	 * from here by the sessions subsystem; implementations read `Credential`, not this.
	 * Kept for diagnostics only, and never logged verbatim (it carries the token).
	 */
	UPROPERTY()
	FString Options;

	/** Remote endpoint as the net driver saw it. Diagnostics only; never a trust input. */
	UPROPERTY()
	FString RemoteAddress;

	/**
	 * Platform identity as a STRING, already stringified by the caller.
	 * Identity, and identity only (services law 2): it may be used to reject a duplicate or a
	 * ban-listed player and for NOTHING else. It never selects team, loadout, or authority, and
	 * it must never reach telemetry (see BRTelemetrySubsystem's pseudonymous keys).
	 */
	UPROPERTY()
	FString PlatformIdString;

	/** Extracted admission token, or empty. */
	UPROPERTY()
	FBRJoinCredential Credential;

	/** True when this join arrives after the match has started — the join-in-progress case. */
	UPROPERTY()
	bool bJoinInProgress = false;
};

/**
 * FBRJoinVerdict — admission's answer. A rejection ALWAYS carries a player-facing reason
 * (services law 7: failure is a state, not a silent hang) and a non-PII code for logs.
 */
USTRUCT()
struct FBRJoinVerdict
{
	GENERATED_BODY()

	UPROPERTY()
	EBRJoinAdmission Admission = EBRJoinAdmission::Accepted;

	/** Shown to the rejected player. Required when rejecting; ignored when accepting. */
	UPROPERTY()
	FText DenialReason;

	/** Short, stable, non-PII. Goes into logs and telemetry; the FText does not. */
	UPROPERTY()
	FString DenialCode;

	bool IsAccepted() const { return Admission == EBRJoinAdmission::Accepted; }

	static FBRJoinVerdict Accept()
	{
		return FBRJoinVerdict();
	}

	static FBRJoinVerdict Reject(const FText& Reason, const FString& Code)
	{
		FBRJoinVerdict Verdict;
		Verdict.Admission = EBRJoinAdmission::Rejected;
		Verdict.DenialReason = Reason;
		Verdict.DenialCode = Code;
		return Verdict;
	}
};

/**
 * FBRHostingEndNotice — the payload of the one outbound ending event.
 *
 * `GraceSeconds` is the window consumers have before the world goes away: telemetry gets one
 * synchronous chance to fold and hand off its record, and the match frame gets one chance to
 * freeze. It is BOUNDED and always elapses — a consumer that needs "just a bit longer" does not
 * get it, because a hang here is nine players staring at a frozen scoreboard.
 */
USTRUCT()
struct FBRHostingEndNotice
{
	GENERATED_BODY()

	UPROPERTY()
	EBRHostingEndReason Reason = EBRHostingEndReason::Unknown;

	/** Seconds between this event and the actual teardown. May be 0 (die-now paths). */
	UPROPERTY()
	float GraceSeconds = 0.f;

	/** What remote players are told. Defined for every reason; never empty in practice. */
	UPROPERTY()
	FText RemoteMessage;

	/** Stable non-PII code for logs/telemetry, e.g. "host_quit", "match_complete". */
	UPROPERTY()
	FString DiagnosticCode;
};

/**
 * FBRMatchResultSummary — the minimum the match frame hands the lifecycle when a match ends.
 * Deliberately tiny: the lifecycle is not a scoreboard. GameLift's implementation forwards this
 * to a session-results endpoint; the listen implementation logs it and stamps telemetry.
 */
USTRUCT()
struct FBRMatchResultSummary
{
	GENERATED_BODY()

	/** Winning team id, or 255 for a draw (matches `BRMatch::InvalidTeamId` without including it). */
	UPROPERTY()
	uint8 WinningTeamId = 255;

	/** Seconds of scoring play. */
	UPROPERTY()
	float MatchDurationSeconds = 0.f;

	/** Humans present at the final whistle. Bots are not the lifecycle's business. */
	UPROPERTY()
	int32 HumanPlayerCount = 0;
};

// =============================================================================================
// Outbound event signatures (server -> game). Native multicast: no Blueprint surface (R18).
// =============================================================================================

/**
 * Hosting is ending. Fires EXACTLY ONCE per server lifetime, before teardown begins, on the
 * server only. Every reason funnels here (seam gap 2). Handlers must be non-blocking and must
 * tolerate running during a level teardown: no travel, no new actors, no latent work.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRHostingEndingSignature, const FBRHostingEndNotice& /*Notice*/);

/** Hosting state moved. Fires on every transition, before any other event for that transition. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRHostingStateChangedSignature, EBRHostingState /*Old*/, EBRHostingState /*New*/);

/**
 * The hosting platform's health callback arrived (GameLift `OnHealthCheck`). Inbound push, never
 * a poll. The listen implementation only fires this when a diagnostic interval is explicitly
 * enabled, so consumers can be tested without a cloud fleet.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRHostingHealthPulseSignature, double /*ServerTimeSeconds*/);

// =============================================================================================
// The interface.
// =============================================================================================

UINTERFACE(MinimalAPI, NotBlueprintable, meta = (CannotImplementInterfaceInBlueprint))
class UBRServerLifecycle : public UInterface
{
	GENERATED_BODY()
};

/**
 * IBRServerLifecycle — the migration seam. Two implementations will ever exist:
 * `UBRListenServerLifecycle` (slice) and `UBRGameLiftLifecycle` (Phase 2, GL-1).
 *
 * Obtained ONLY through `UBRSessionsSubsystem::GetServerLifecycle()`, which hands back a
 * `TScriptInterface<IBRServerLifecycle>` — a GC-safe reference that reveals nothing about the
 * concrete class. Never `Cast<UBRListenServerLifecycle>` outside `Online/`.
 *
 * FIRING ORDER (this is API, not implementation detail):
 *   InitializeHosting  -> OnHostingStateChanged(Uninitialized -> Initializing)
 *   NotifyServerReadyForPlayers -> OnHostingStateChanged(Initializing -> Hosting)
 *   [per join]         ValidateJoin -> (accepted) NotifyPlayerJoined
 *   [per leave]        NotifyPlayerLeft
 *   NotifyMatchComplete -> RequestHostingEnd(MatchComplete)
 *   RequestHostingEnd  -> OnHostingStateChanged(Hosting -> Ending) THEN OnHostingEnding
 *                      -> [grace window] -> OnHostingStateChanged(Ending -> Ended)
 * The state change ALWAYS precedes the event for the same transition, so a handler of
 * OnHostingEnding that queries GetHostingState() always sees `Ending`, never `Hosting`.
 */
class IBRServerLifecycle
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------------------------
	// Game -> server. Every one of these is server-side; calling them on a client is a no-op
	// in every implementation (and logs), never a crash.
	// -----------------------------------------------------------------------------------------

	/**
	 * Claim the process for hosting. Listen: latch the game instance and bind engine hooks.
	 * GameLift: `InitSDK` + register `OnStartGameSession`/`OnProcessTerminate`/`OnHealthCheck`.
	 * Idempotent. Returns false only when hosting is impossible (already ending, no instance).
	 */
	virtual bool InitializeHosting(UGameInstance* GameInstance) = 0;

	/**
	 * We are on the map and willing to take players. Listen: called once the listen travel has
	 * completed. GameLift: `ProcessReady` / `ActivateGameSession`. Idempotent.
	 */
	virtual void NotifyServerReadyForPlayers() = 0;

	/**
	 * SEAM GAP 1 — per-player admission. Called on EVERY join, before the player enters the
	 * match, including join-in-progress and including the host's own local player.
	 * A join path that does not pass through here is a finding (`online-services.md` v1.1).
	 *
	 * Listen implementation: accept-always (with the ending-state refusal below, which is the
	 * host-quit rule, not an admission policy).
	 * GameLift implementation: `AcceptPlayerSession(Request.Credential.Token)`.
	 *
	 * Must be synchronous and cheap: it runs inside the login handshake.
	 */
	virtual FBRJoinVerdict ValidateJoin(const FBRJoinRequest& Request) = 0;

	/**
	 * An admitted player is now in. Listen: bookkeeping. GameLift: nothing (AcceptPlayerSession
	 * already did it) — which is exactly why this is separate from ValidateJoin.
	 */
	virtual void NotifyPlayerJoined(const FString& PlatformIdString) = 0;

	/** A player left, for any reason. GameLift: `RemovePlayerSession`. Listen: bookkeeping. */
	virtual void NotifyPlayerLeft(const FString& PlatformIdString) = 0;

	/**
	 * The match reached a decided end. The match frame's ONLY lifecycle call.
	 * Implementations route this into RequestHostingEnd(MatchComplete) — the match frame does
	 * not decide whether the server dies, only that its match is over.
	 */
	virtual void NotifyMatchComplete(const FBRMatchResultSummary& Summary) = 0;

	/**
	 * Stop hosting, for `Reason`. IDEMPOTENT AND LATCHED: the first call wins, later calls are
	 * ignored and logged. This is the single funnel every ending goes through — host quit,
	 * match complete, platform request, fault.
	 */
	virtual void RequestHostingEnd(EBRHostingEndReason Reason) = 0;

	// -----------------------------------------------------------------------------------------
	// Server -> game. Consumers BIND these; nobody polls (services law 5).
	// -----------------------------------------------------------------------------------------

	virtual FBRHostingEndingSignature& OnHostingEnding() = 0;
	virtual FBRHostingStateChangedSignature& OnHostingStateChanged() = 0;
	virtual FBRHostingHealthPulseSignature& OnHostingHealthPulse() = 0;

	// -----------------------------------------------------------------------------------------
	// Queries. Note what is NOT here: nothing that names the backend or the topology.
	// -----------------------------------------------------------------------------------------

	virtual EBRHostingState GetHostingState() const = 0;

	/** True while new players may be admitted. False from the instant an ending is requested. */
	virtual bool IsAcceptingPlayers() const = 0;
};
