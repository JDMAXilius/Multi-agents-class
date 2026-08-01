// Breachpoint. The CMC subclass: sprint intent, grapple intent, the saved move that replays both,
// the speed rule, and the grapple pull's root-motion source.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "BRCharacterMovementComponent.generated.h"

/**
 * FSavedMove_BR — one frame of Breachpoint intent, replayable.
 *
 * =========================================================================================
 * RULING (BP02, 1 Aug 2026) — COMPRESSED FLAGS, not the structured move-data container.
 * =========================================================================================
 * The `cmc-prediction` skill left this open and asked the first packet that needed it to decide,
 * on the grounds that "switching later touches every movement state at once". That premise is
 * false in the direction we care about, and that is why the decision is cheap:
 *
 *   `FCharacterNetworkMoveData` — the structured path — CARRIES `CompressedMoveFlags` as one of
 *   its own fields. Adopting the container later is ADDITIVE: every boolean already riding a
 *   custom bit keeps riding it, untouched, and the container adds room for typed fields beside
 *   them. The painful migration is the other direction (typed fields down into 4 bits), and
 *   nobody performs that one.
 *
 * So: boolean movement INTENT rides the custom bits, starting now. The container arrives, in its
 * own packet, the first time a movement state needs a field that is not a boolean — BP06's
 * grapple is the likely trigger (a target point or a surface class will not fit in a bit) — and
 * that packet does not have to touch sprint.
 *
 * THE BIT BUDGET, recorded here because there are only four and running out silently is the
 * failure mode:
 *   FLAG_Custom_0  sprint   (BP02, this file)
 *   FLAG_Custom_1  grapple  (BP06, this file) — TAKEN 1 Aug 2026; the reservation is spent
 *   FLAG_Custom_2  free
 *   FLAG_Custom_3  free
 *
 * TWO BITS LEFT, and BP06 did NOT need the structured move-data container after all. The BP02
 * ruling above guessed grapple would be the trigger ("a target point or a surface class will not
 * fit in a bit"). It guessed wrong, and the reason is worth recording because it is the whole
 * design: the pull TARGET never travels in the move at all. It travels once, inside a root motion
 * source, and the engine already saves and restores the entire `FRootMotionSourceGroup` with every
 * saved move (`SavedRootMotion`). So the target replays for free through machinery we did not
 * write, and the only thing left for a custom bit is the boolean it always was — *is this move a
 * pull move*. The container's trigger is still out there; it is not this packet.
 *
 * ALL FIVE HOOKS ARE IMPLEMENTED BELOW and none is optional. (The line above used to say FOUR
 * while listing five — corrected here rather than left as a trap.) Skipping `Clear()` reuses a
 * pooled move with a stale flag (a phantom sprint three moves later); skipping `CanCombineWith`
 * lets two moves that disagree merge and a sprint frame vanish on correction. Both bugs only
 * appear under loss, which is to say: not in the editor.
 */
struct FSavedMove_BR : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

	/** Sprint intent as it was at the START of this move. Set by BRGA_Sprint, replayed from here. */
	uint8 bSavedWantsToSprint : 1;

	/**
	 * Grapple intent as it was at the START of this move. Set by BRGA_Grapple, replayed from here.
	 *
	 * WHY THIS BIT EARNS ITS PLACE when the root motion source already replays itself. A replay
	 * re-simulates N moves back-to-back in one frame, and the detach rules below (arrival radius,
	 * jump-cancel) are evaluated inside that re-simulation. If grapple intent were a plain component
	 * member, all N replayed moves would be evaluated against the intent the player has NOW —
	 * so a player who released the grapple after move 6 would have moves 1..6 replayed as
	 * "released" and detach at move 1. The replayed path would then diverge from the path the
	 * client originally predicted, the server correction would not converge, and the character
	 * would oscillate. Carrying the intent per-move is what makes the replay reproduce itself.
	 */
	uint8 bSavedWantsToGrapple : 1;

	FSavedMove_BR()
		: bSavedWantsToSprint(0)
		, bSavedWantsToGrapple(0)
	{
	}

	/** Moves are POOLED. Every custom field resets here or it leaks into an unrelated frame. */
	virtual void Clear() override;

	/** Capture the component's current intent into this move (client, before the move simulates). */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;

	/** Push this move's intent back into the component before a replay. */
	virtual void PrepMoveFor(ACharacter* C) override;

	/** Refuse to combine moves whose intent differs — a merged disagreement is a lost frame. */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	/** Pack the intent into the custom bits for the wire. */
	virtual uint8 GetCompressedFlags() const override;
};

/** The factory that hands out FSavedMove_BR instead of FSavedMove_Character. Nothing else. */
class FNetworkPredictionData_Client_BR : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_BR(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override;
};

/**
 * FBRGrappleTuning — the grapple's three CT_Combat values, resolved together or not at all.
 *
 * ALL THREE OR NONE. `bValid` is false unless every row resolved, and `StartGrapplePull` then
 * refuses outright. Half a tuning set would mean pulling at a real speed toward a target we cannot
 * recognise arriving at — i.e. flying past the ledge until the timeout. See `GetGrappleTuning()`
 * for why "refuse" is the correct missing-data answer here even though sprint's is "substitute the
 * identity": there is no identity pull speed, and every candidate number is a design decision.
 *
 * File scope rather than nested in the component, for the same reason `FSavedMove_BR` is: UHT parses
 * the UCLASS body, and a plain struct declared inside it is a needless question to make it answer.
 */
struct FBRGrappleTuning
{
	/** `Movement.Grapple.PullSpeed` — cm/s along the pull. Sets the source's Duration via distance/speed. */
	float PullSpeedCmPerSecond = 0.f;

	/** `Movement.Grapple.ArrivalRadius` — cm. Inside this of the target, detach rule 1 fires. */
	float ArrivalRadiusCm = 0.f;

	/** `Movement.Grapple.MaxPullSeconds` — the ceiling on the source's Duration, i.e. detach rule 3. */
	float MaxPullSeconds = 0.f;

	bool bValid = false;
};

/**
 * UBRCharacterMovementComponent — our subclass of the engine CMC (§3.4, "subclass, NOT from
 * scratch": rewriting CMC means rewriting the most battle-tested networked prediction code in the
 * engine, and "100% our gameplay code" does not extend to re-implementing engine subsystems).
 *
 * WHAT IT OWNS TODAY: the sprint half of `BRGA_Sprint` — the intent flag, the saved move that
 * replays it, and the speed rule that consumes it — and the MOTION half of `BRGA_Grapple`: the
 * root motion source that performs the pull, and the three rules that end it. The DECISION to
 * sprint or to grapple is the ability's (gas-purity.md's movement exception: GAS decides, CMC
 * moves).
 *
 * EVERY NUMBER IS DATA. `GetMaxSpeed()` multiplies the engine's answer by
 * `CT_Combat["Movement.Sprint.SpeedMultiplier"]`; the pull's speed, arrival radius and timeout
 * ceiling come from three more `CT_Combat` curves. No gameplay number appears in this file, and a
 * balance change to either system is a CSV diff.
 *
 * ----------------------------------------------------------------------------------------------
 * THE GRAPPLE, AND WHY IT IS A ROOT MOTION SOURCE AND NOT A POSITION WRITE.
 *
 * A root motion source rides `FSavedMove_Character::SavedRootMotion`: the engine captures the whole
 * source group into every saved move and restores it before every replay. That means the pull
 * predicts on the client, reconciles against the server, and replays after a correction through
 * exactly the same machinery as walking — with no bespoke reconciliation code for us to get wrong.
 * A `SetActorLocation` or a velocity write would get none of that, and would fork position on the
 * first dropped packet. There is deliberately not one position write in this file.
 *
 * WHERE THE LINE SITS, and it is absolute:
 *
 *   `BRGA_Grapple` decides WHETHER — trace, classify the surface, server-validate range/LOS/rate,
 *   commit the cooldown GE, fire the cues. It never writes a position and never calls into the
 *   engine's movement state.
 *
 *   THIS COMPONENT executes MOTION — it applies the source, and it owns the three rules that end
 *   the pull (arrival radius, jump-cancel, timeout). It never asks whether the player may grapple,
 *   never reads a gameplay tag, never touches a cooldown, and never spawns an effect.
 *
 * REJECTION LEAVES ZERO POSITION RESIDUE — this file's third of BP06's Done-when.
 *
 * The rule that buys it: **this component's grapple bookkeeping is DERIVED, never authoritative.**
 * `ActiveGrapplePullSourceID` is a lookup key into `CurrentRootMotion`, which is the only truth
 * about whether a pull exists, and which the engine itself rewinds on a correction. So when the
 * server rejects the grapple, the server never applied a source; the correction the client receives
 * carries the server's position and the server's (empty) root motion group; the client's replay
 * therefore runs with no pull source, arrives at the server's position, and the very next movement
 * update finds `ActiveGrapplePullSourceID` dangling and clears the intent bit with it. There is no
 * second source of position in this file to disagree with the correction, and there is no timer,
 * no cached location and no accumulated float to survive it. The residue cannot exist because there
 * is nowhere for it to live.
 *
 * That is also why the TIMEOUT is the source's own `Duration` and not an `FTimerHandle`. A timer
 * runs on wall-clock time in one process; it does not replay, it does not rewind, and it would fire
 * at a different simulated moment on the client than on the server — a guaranteed correction at the
 * end of every single grapple. The source's `Duration` is simulated time carried inside the object
 * the engine already saves and restores. It was the only one of the three detach rules that needed
 * no code from us, and it is the only one a timer could not have done.
 *
 * NAMED, NOT HIDDEN — the trust properties of `FLAG_Custom_1`, which differ from the sprint bit's.
 *
 * A modified client asserting the grapple bit forever gains NOTHING: the bit does not create
 * motion, only a root motion source does, and a source only exists where `StartGrapplePull` was
 * called — on the server that is the server's own ability activation, after the server's own
 * validation. Arrival and timeout are then computed by the server from the server's own simulated
 * position and the server's own source `Duration`, so a client that refuses to drop the bit is
 * still detached on schedule. The worst a forged bit buys is declining to cancel your own pull.
 * This bit is, unlike the sprint bit, not an exploit surface — recorded because the sprint gap
 * above makes it reasonable to assume otherwise.
 *
 * The REAL hazard here is ordering, and it is the same one the sprint note describes from the other
 * end. The ability's activation RPC (PlayerState channel) and `ServerMove` (Character channel) are
 * unordered, so the server can apply the pull source and THEN process a `ServerMove` that the
 * client recorded before it pressed grapple — a move whose bit is 0. Taken naively that stale zero
 * reads as "the player let go" and kills the pull on its first frame, every time, on a laggy
 * connection. `bGrappleIntentObserved` below exists for exactly that: a falling edge only detaches
 * after a rising edge has been seen within the same pull. It is a small piece of state and it is
 * documented rather than clever.
 *
 * ----------------------------------------------------------------------------------------------
 * NAMED, NOT HIDDEN — the trust gap in the sprint bit.
 *
 * On the server, `UpdateFromCompressedFlags` accepts the client's sprint bit as intent, and
 * `GetMaxSpeed` acts on it without asking whether the client actually has an active sprint
 * ability. A modified client can therefore assert the bit and move at sprint speed without ever
 * activating `BRGA_Sprint`. This is the standard UE arrangement (`bWantsToCrouch` has exactly the
 * same property) and it is a real, if bounded, exploit: a flat movement-speed multiplier.
 *
 * The obvious closure — require `State.Movement.Sprinting` on the owner before applying the
 * multiplier — was tried on paper and REJECTED here, not overlooked: the ability-activation RPC
 * (PlayerState channel) and the ServerMove (Character channel) are not ordered relative to each
 * other, so the server would routinely process a sprint move before the activation that
 * authorises it and correct the client at the start of every sprint. Trading a guaranteed hitch on
 * every honest press for a cheat that costs a cheater a modified binary is the wrong trade to make
 * silently, so it is made loudly, here, and routed to netcode-builder: the gate belongs in
 * `IsSprintIntentValid()` below, which exists precisely so the closure is one function.
 */
UCLASS(meta = (DisplayName = "BR Character Movement Component"))
class BREACHPOINT_API UBRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UBRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set (or clear) sprint intent. THE ONLY caller is `UBRGA_Sprint` — on activation and on every
	 * end path. Not a gameplay decision: by the time this is called the decision is made, blocked
	 * tags were checked, and the ability is running.
	 */
	void SetSprintIntent(bool bNewWantsToSprint);

	/** Raw intent, as carried by the saved move. */
	bool WantsToSprint() const { return bWantsToSprint; }

	// ------------------------------------------------------------------------------------------
	// The grapple's MOTION half. THE ONLY caller is `UBRGA_Grapple`. See the class comment for the
	// division of labour; nothing below asks whether the grapple is allowed.
	// ------------------------------------------------------------------------------------------

	/**
	 * Begin pulling the owner toward `PullTargetWorld` with a root motion source.
	 *
	 * MUST BE CALLED ON BOTH the predicting client and the server — which is what happens naturally
	 * when a `LocalPredicted` ability calls it from `ActivateAbility`, since that body runs on both.
	 * Client-only means the pull has no authority and dies at the first correction; server-only
	 * means no prediction and a visible snap one round-trip after the button. There is no authority
	 * check in here for that reason, only a role check that rejects simulated proxies (they receive
	 * the motion through replication and must not manufacture their own).
	 *
	 * The target is captured ONCE, here, and never updated — the server validated exactly this point
	 * for range and line of sight, and a target that moved afterwards would be a point the server
	 * never agreed to.
	 *
	 * @return true if a source was applied. FALSE, WITH NO SIDE EFFECT AT ALL, when the CT_Combat
	 *         tuning is missing or the role is wrong. The ability must treat false as "the pull did
	 *         not happen" — false here is the zero-residue path, not a warning.
	 */
	bool StartGrapplePull(const FVector& PullTargetWorld);

	/**
	 * End the pull now and remove exactly the source this component applied.
	 *
	 * Idempotent, and safe to call when no pull is active — which is what lets `BRGA_Grapple` call
	 * it unconditionally from `EndAbility`, the way `BRGA_Sprint` clears sprint intent from its one
	 * exit. Removal is BY ID: a blanket `CurrentRootMotion` clear would also delete an unrelated
	 * knockback or launch source that happened to be running, and that bug would surface the first
	 * time someone grapples out of an explosion.
	 */
	void StopGrapplePull();

	/** Raw grapple intent for the move being simulated, as carried by the saved move. */
	bool WantsToGrapple() const { return bWantsToGrapple; }

	/**
	 * Is a pull source live right now? Asks `CurrentRootMotion`, not a cached bool — see the class
	 * comment.
	 *
	 * NON-CONST, and it should offend you slightly. `FRootMotionSourceGroup::GetRootMotionSourceByID`
	 * is a non-const engine accessor (checked against the 5.6 headers on this machine; 5.8 is
	 * assumed unchanged and is UNVERIFIED), and the group has no const equivalent. The alternatives
	 * were a `const_cast` inside a const function — a signature that lies — or caching a bool, which
	 * is the exact thing the zero-residue rule forbids. An honest non-const query is the least bad
	 * of the three, and callers hold a mutable component anyway.
	 */
	bool IsGrapplePullActive();

	/**
	 * Does the current sprint intent actually earn the multiplier?
	 *
	 * Today: intent + on the ground. THE SEAM for the server-side trust gate described in the class
	 * comment — one function, one place, no movement state moves when it changes.
	 */
	virtual bool IsSprintIntentValid() const;

	/** Walking speed x the CT_Combat sprint multiplier while sprint intent is valid. */
	virtual float GetMaxSpeed() const override;

	/** Unpack the custom bits on the server / on replay. Symmetric with GetCompressedFlags(). */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	/**
	 * Detach rule 2 — JUMP-CANCEL, evaluated here because here is before the move simulates.
	 *
	 * NOT A TICK, and law 4 is not bent by this. `UpdateCharacterStateBeforeMovement` is the engine's
	 * own per-move state hook — it is where `bWantsToCrouch` becomes a crouch — and it is called from
	 * `PerformMovement`, which means it is ALSO called for every replayed move after a correction.
	 * That is precisely why the detach rules live here and not on a timer or an actor tick: a rule
	 * evaluated inside the simulation replays with the simulation. A rule evaluated outside it does
	 * not, and desyncs on the first correction.
	 *
	 * Jump specifically: `bPressedJump` is saved-move state (`FSavedMove_Character` records it and
	 * `PrepMoveFor` restores it), so reading it here is replay-correct for free. Cancelling before the
	 * move simulates is what makes the jump frame a jump frame rather than a pull frame with a jump
	 * queued behind it.
	 */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	/**
	 * Detach rules 1 and 3 — ARRIVAL RADIUS and TIMEOUT — plus the reconciliation of our bookkeeping
	 * against `CurrentRootMotion`.
	 *
	 * After, not before, because both are questions about the position the move just produced.
	 * Arrival is a distance test against the captured target: a pure function of simulated position,
	 * so client and server reach it on the same simulated frame and the replay reaches it again.
	 * Timeout is not tested at all — it is the source's `Duration` expiring, observed here as the
	 * handle no longer resolving. Same observation covers a server correction having wiped the
	 * source, which is the rejection path, and the two need no separate code because at this level
	 * they are the same event: the pull is gone, so stop believing in it.
	 */
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

protected:
	/**
	 * Sprint intent for the move currently being simulated.
	 *
	 * NOT gameplay state and never read as such: the replicated state is
	 * `State.Movement.Sprinting`, granted by the ability. This is the per-move input bit that makes
	 * the ability's decision survive a correction, and it is exactly as authoritative as
	 * `bWantsToCrouch` — which is to say, it is the client's claim about its own intent.
	 */
	uint8 bWantsToSprint : 1;

	/**
	 * Grapple intent for the move currently being simulated. Same status as `bWantsToSprint`: the
	 * client's claim about its own input, replayed per-move by `FSavedMove_BR`. The replicated
	 * gameplay state is `State.Movement.Grappling`, which the ability grants and this file never
	 * reads.
	 */
	uint8 bWantsToGrapple : 1;

private:
	/**
	 * The sprint speed multiplier from `CT_Combat`, resolved once per process.
	 *
	 * Cached because `GetMaxSpeed()` is called several times per simulated frame per character and
	 * a curve evaluation per call is waste, not because the value is expected to change: it cannot,
	 * within a process, and caching a value that could change would be a determinism bug. A missing
	 * curve resolves to 1.0 (no sprint) and says so once, loudly — walking when you pressed sprint
	 * is a visible failure; guessing a multiplier would not be.
	 */
	float GetSprintSpeedMultiplier() const;

	mutable float CachedSprintSpeedMultiplier = -1.f;

	// ------------------------------------------------------------------------------------------
	// Grapple internals
	// ------------------------------------------------------------------------------------------

	/**
	 * Resolve (once) and return the grapple tuning. `bValid == false` means: do not grapple.
	 *
	 * Cached for the same reason `CachedSprintSpeedMultiplier` is: the arrival radius is consulted
	 * every simulated frame of every pull, a curve evaluation per frame is waste, and a value that
	 * could change within a process would be a determinism bug rather than a feature.
	 */
	const FBRGrappleTuning& GetGrappleTuning() const;

	mutable FBRGrappleTuning CachedGrappleTuning;
	mutable bool bGrappleTuningResolved = false;

	/**
	 * The ID of the pull source THIS component applied, or `ERootMotionSourceID::Invalid`.
	 *
	 * A lookup key, not a state flag, and never trusted on its own — `IsGrapplePullActive()` resolves
	 * it against `CurrentRootMotion` every time. Retained so detach removes exactly this source and
	 * leaves a concurrent knockback or launch alone.
	 */
	uint16 ActiveGrapplePullSourceID = 0; // 0 == (uint16)ERootMotionSourceID::Invalid; spelled out in the .cpp.

	/**
	 * The point the server validated and both sides pull toward. Read only by the arrival test, and
	 * meaningless unless `IsGrapplePullActive()`. Not saved-move state and does not need to be: it is
	 * constant for the whole pull, so a replay cannot want a different value.
	 */
	FVector GrapplePullTarget = FVector::ZeroVector;

	/**
	 * Has grapple intent been seen SET since this pull began?
	 *
	 * The ordering guard described in the class comment. Until this is true, a zero in
	 * `FLAG_Custom_1` is treated as "a move recorded before the player pressed grapple", not as a
	 * release — because on the server that is exactly what it usually is.
	 */
	uint8 bGrappleIntentObserved : 1;

	/** The single place a pull's bookkeeping is torn down. Every detach path routes through it. */
	void ClearGrapplePullState();

	friend struct FSavedMove_BR;
};
