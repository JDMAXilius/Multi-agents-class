// Breachpoint. The CMC subclass: sprint intent, grapple intent, the saved move that replays both,
// the speed rule, and the grapple pull's root-motion source.

#include "Character/BRCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/RootMotionSource.h"

#include "AbilitySystem/BRCombatCurves.h"
#include "Core/BRCore.h"

namespace
{
	/**
	 * THE GRAPPLE'S CT_Combat KEYS.
	 *
	 * ------------------------------------------------------------------------------------------
	 * CONTRACT_GAP, declared where it bites rather than buried in the ticket.
	 *
	 * These belong in `BRCombatCurves::Names` beside `MovementSprintSpeedMultiplier`. That header's
	 * whole premise is "one reader, one spelling of each curve name" — and it lives in
	 * `Source/Breachpoint/AbilitySystem/`, which is not this packet's owner path (BP06 step 1 owns
	 * `Source/Breachpoint/Character/`). Law 5 says a blocked packet files the gap and does not
	 * reach into someone else's file to unblock itself, so the names sit here, in one translation
	 * unit, with one spelling each, and the gap is reported: **whoever next owns
	 * `AbilitySystem/BRCombatCurves.h` should move these three constants into `Names` and this
	 * block should be deleted in the same commit.** Until then there are two homes for curve keys,
	 * and that is a real, named cost, not an oversight.
	 *
	 * NONE OF THE THREE ROWS EXISTS IN `Content/Data/CT_Combat.csv` AS OF THIS COMMIT. The table is
	 * `Content/`, also not this packet's path. Until a designer adds them the grapple applies no
	 * source at all and logs once — see `GetGrappleTuning()` for why that is the correct failure and
	 * not a placeholder.
	 */
	namespace BRGrappleCurveNames
	{
		/** Speed of the pull along the rope, cm/s. Distance / this = the source's Duration. */
		const FName GrapplePullSpeed = FName(TEXT("Movement.Grapple.PullSpeed"));

		/** Detach rule 1: how close to the target counts as arrived, cm. */
		const FName GrappleArrivalRadius = FName(TEXT("Movement.Grapple.ArrivalRadius"));

		/** Detach rule 3: hard ceiling on a pull's Duration, seconds. */
		const FName GrappleMaxPullSeconds = FName(TEXT("Movement.Grapple.MaxPullSeconds"));
	}

	/**
	 * The pull source's name and priority.
	 *
	 * NOT TUNING and not a law-3 violation: a priority is a RANK among concurrent root motion
	 * sources, with no unit and no balance meaning — moving it from 5 to 6 changes nothing a
	 * designer can perceive unless a second source exists to be ranked against. It is recorded
	 * here so the next source added (knockback, launcher, BP06's enemy-reel) picks its rank
	 * deliberately and in one place, instead of two systems both defaulting to 0 and fighting.
	 *
	 * The InstanceName is how a human finds this source in `showdebug characters` output. It is
	 * also NOT how detach identifies it — detach uses the retained ID, because two pulls could in
	 * principle share a name and only one of them is ours.
	 */
	const FName GrapplePullSourceName = FName(TEXT("BRGrapplePull"));
	constexpr uint16 GrapplePullSourcePriority = 5;
}

// ===========================================================================================
// FSavedMove_BR — the five hooks, none optional
// ===========================================================================================

void FSavedMove_BR::Clear()
{
	Super::Clear();

	// THE CLASSIC BUG these two lines prevent: moves are pooled and reused, so a flag left over
	// from three moves ago replays as a phantom input the player never gave. Adding a field to this
	// struct and not to this function is the same bug with a new name — under packet loss only.
	bSavedWantsToSprint = 0;
	bSavedWantsToGrapple = 0;
}

void FSavedMove_BR::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UBRCharacterMovementComponent* Movement = C ? Cast<UBRCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		bSavedWantsToSprint = Movement->bWantsToSprint;
		bSavedWantsToGrapple = Movement->bWantsToGrapple;
	}

	// NOT CAPTURED HERE, deliberately: the pull's root motion source. `Super::SetMoveFor` already
	// copied the entire `CurrentRootMotion` group into `SavedRootMotion` — that is engine
	// behaviour we are relying on, not overlooking, and duplicating it would give the replay two
	// sources of truth about the same pull.
}

void FSavedMove_BR::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UBRCharacterMovementComponent* Movement = C ? Cast<UBRCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		// The replay path. The component is rewound to the intent this move was RECORDED with, not
		// the intent the player has now — which is the whole point of a saved move, and for grapple
		// it is the difference between a replay that reproduces the original path and one that
		// detaches at move 1 because the player has since let go.
		Movement->bWantsToSprint = bSavedWantsToSprint;
		Movement->bWantsToGrapple = bSavedWantsToGrapple;
	}
}

bool FSavedMove_BR::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_BR* Other = static_cast<const FSavedMove_BR*>(NewMove.Get());
	if (Other && Other->bSavedWantsToSprint != bSavedWantsToSprint)
	{
		// Combining moves that disagree about intent is how the frame you started sprinting
		// silently disappears on a correction. Refuse before asking the base anything else.
		return false;
	}

	if (Other && Other->bSavedWantsToGrapple != bSavedWantsToGrapple)
	{
		// Same rule, and it matters more here: the move where grapple intent changes is the move
		// that starts or ends a pull. Merging it into its neighbour moves the pull's boundary by a
		// frame on one machine and not the other, which is a correction at both ends of every
		// grapple. (The engine independently refuses to combine moves with differing root motion,
		// which would catch most of these — but "most" is not a netcode argument, and this check
		// also covers the boundary frames where the source is not applied yet.)
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

uint8 FSavedMove_BR::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bSavedWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}

	if (bSavedWantsToGrapple)
	{
		Result |= FLAG_Custom_1;
	}

	return Result;
}

FSavedMovePtr FNetworkPredictionData_Client_BR::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_BR());
}

// ===========================================================================================
// UBRCharacterMovementComponent
// ===========================================================================================

UBRCharacterMovementComponent::UBRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bWantsToSprint(0)
	, bWantsToGrapple(0)
	, bGrappleIntentObserved(0)
{
	// Halo-feel movement numbers (walk/air speed, air control, jump velocity, gravity scale) are
	// §3.4 "config on CMC defaults, not code" — this constructor still sets NONE of them, and the
	// sprint multiplier is a CT_Combat curve rather than a member. A number typed here would be a
	// law-3 violation waiting to be found.
}

void UBRCharacterMovementComponent::SetSprintIntent(bool bNewWantsToSprint)
{
	const uint8 NewValue = bNewWantsToSprint ? 1 : 0;
	if (bWantsToSprint == NewValue)
	{
		return;
	}

	bWantsToSprint = NewValue;

	UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacterMovementComponent '%s': sprint intent -> %s."),
		*GetNameSafe(GetOwner()), bNewWantsToSprint ? TEXT("ON") : TEXT("OFF"));
}

bool UBRCharacterMovementComponent::IsSprintIntentValid() const
{
	// Airborne sprint would let a bunny-hopper carry sprint speed through the air, which is a
	// movement design nobody asked for. IsMovingOnGround() covers MOVE_Walking and MOVE_NavWalking
	// (bots) in one call. This is a rule, not a tuning value, so it lives in code.
	return bWantsToSprint && IsMovingOnGround();
}

float UBRCharacterMovementComponent::GetSprintSpeedMultiplier() const
{
	if (CachedSprintSpeedMultiplier >= 0.f)
	{
		return CachedSprintSpeedMultiplier;
	}

	float Multiplier = 0.f;
	if (BRCombatCurves::Evaluate(BRCombatCurves::Names::MovementSprintSpeedMultiplier, Multiplier) && Multiplier > 0.f)
	{
		CachedSprintSpeedMultiplier = Multiplier;
		return CachedSprintSpeedMultiplier;
	}

	// 1.0 is the IDENTITY, not a guess at a sprint speed: with no curve the player walks while
	// holding sprint, which is visible in the first ten seconds of a playtest. Substituting a
	// plausible 1.4 here would hide a missing CSV row behind a number nobody chose.
	UE_LOG(LogBRCombat, Error, TEXT("BRCharacterMovementComponent: CT_Combat has no usable '%s' curve. Sprint applies NO speed multiplier."),
		*BRCombatCurves::Names::MovementSprintSpeedMultiplier.ToString());

	CachedSprintSpeedMultiplier = 1.f;
	return CachedSprintSpeedMultiplier;
}

float UBRCharacterMovementComponent::GetMaxSpeed() const
{
	const float BaseMaxSpeed = Super::GetMaxSpeed();

	if (!IsSprintIntentValid())
	{
		return BaseMaxSpeed;
	}

	// Multiplying the engine's answer (rather than returning a sprint speed) means crouch, water
	// and every future movement mode keep their own ceilings and sprint scales whatever is current.
	return BaseMaxSpeed * GetSprintSpeedMultiplier();
}

void UBRCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// Symmetric with FSavedMove_BR::GetCompressedFlags. Asymmetry here is the bug that makes a
	// client sprint and a server not, and it is invisible in single-process PIE. Read the two
	// functions side by side when adding a third bit — that pairing IS the test, until rung 4.
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0 ? 1 : 0;
	bWantsToGrapple = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0 ? 1 : 0;

	// The rising edge of the ordering guard. Set here rather than in StartGrapplePull because the
	// question this answers is "has the CLIENT's intent stream ever confirmed the pull", and on the
	// server StartGrapplePull is driven by the ability RPC, which is the other channel entirely.
	if (bWantsToGrapple)
	{
		bGrappleIntentObserved = 1;
	}
}

FNetworkPredictionData_Client* UBRCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		// The ONE reason this override exists: hand out FSavedMove_BR instead of
		// FSavedMove_Character. Everything else (smoothing distances, timestamps) is initialised by
		// FNetworkPredictionData_Client_Character's constructor and is deliberately not restated
		// here — a "restated engine default" is a second source of truth that silently diverges the
		// next time Epic changes theirs.
		UBRCharacterMovementComponent* MutableThis = const_cast<UBRCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_BR(*this);
	}

	return ClientPredictionData;
}

// ===========================================================================================
// The grapple's MOTION half — the root motion source, and the three rules that end it
// ===========================================================================================

const FBRGrappleTuning& UBRCharacterMovementComponent::GetGrappleTuning() const
{
	if (bGrappleTuningResolved)
	{
		return CachedGrappleTuning;
	}

	bGrappleTuningResolved = true;

	float PullSpeed = 0.f;
	float ArrivalRadius = 0.f;
	float MaxSeconds = 0.f;

	const bool bHavePullSpeed = BRCombatCurves::Evaluate(BRGrappleCurveNames::GrapplePullSpeed, PullSpeed) && PullSpeed > 0.f;
	const bool bHaveArrival = BRCombatCurves::Evaluate(BRGrappleCurveNames::GrappleArrivalRadius, ArrivalRadius) && ArrivalRadius > 0.f;
	const bool bHaveMaxSeconds = BRCombatCurves::Evaluate(BRGrappleCurveNames::GrappleMaxPullSeconds, MaxSeconds) && MaxSeconds > 0.f;

	if (bHavePullSpeed && bHaveArrival && bHaveMaxSeconds)
	{
		CachedGrappleTuning.PullSpeedCmPerSecond = PullSpeed;
		CachedGrappleTuning.ArrivalRadiusCm = ArrivalRadius;
		CachedGrappleTuning.MaxPullSeconds = MaxSeconds;
		CachedGrappleTuning.bValid = true;
		return CachedGrappleTuning;
	}

	// THE SPRINT PRECEDENT, APPLIED HONESTLY RATHER THAN LITERALLY.
	//
	// Sprint's missing-curve answer is the IDENTITY (multiplier 1.0 = you walk), chosen because
	// walking while holding sprint is visible in ten seconds and a substituted 1.4 would hide a
	// missing CSV row behind a number nobody chose. There is no identity for a pull speed, an
	// arrival radius or a timeout — every candidate value IS a design decision — so the same
	// principle produces a different answer: the identity here is NO PULL. The player fires the
	// grapple, nothing moves, the log says exactly which rows are missing, and a designer fixes a
	// CSV. Inventing 2000 cm/s would ship a grapple whose feel nobody authored, and it would
	// survive playtest as "a bit floaty" instead of failing as "not wired up".
	//
	// It is also the zero-residue answer: no source is applied, so there is no position to roll
	// back on either machine.
	UE_LOG(LogBRCombat, Error,
		TEXT("BRCharacterMovementComponent '%s': grapple DISABLED — CT_Combat is missing usable rows. '%s'=%s '%s'=%s '%s'=%s. No pull will be applied."),
		*GetNameSafe(GetOwner()),
		*BRGrappleCurveNames::GrapplePullSpeed.ToString(), bHavePullSpeed ? TEXT("ok") : TEXT("MISSING"),
		*BRGrappleCurveNames::GrappleArrivalRadius.ToString(), bHaveArrival ? TEXT("ok") : TEXT("MISSING"),
		*BRGrappleCurveNames::GrappleMaxPullSeconds.ToString(), bHaveMaxSeconds ? TEXT("ok") : TEXT("MISSING"));

	CachedGrappleTuning = FBRGrappleTuning();
	return CachedGrappleTuning;
}

bool UBRCharacterMovementComponent::IsGrapplePullActive()
{
	if (ActiveGrapplePullSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return false;
	}

	// The ID is a KEY, never the answer. `CurrentRootMotion` is rewound by the engine on a server
	// correction and pruned when a source finishes, so asking it — rather than trusting a bool we
	// set ourselves — is what makes a rejected grapple stop existing without any rollback code of
	// our own. A cached bool here would be exactly the "position residue" BP06 forbids.
	return CurrentRootMotion.GetRootMotionSourceByID(ActiveGrapplePullSourceID).IsValid();
}

bool UBRCharacterMovementComponent::StartGrapplePull(const FVector& PullTargetWorld)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	// ROLE, not authority. This must run on the server AND on the predicting client — server-only
	// is a snap one round-trip after the button, client-only has no authority and dies at the first
	// correction. A simulated proxy is the one role that must NOT apply its own source: it receives
	// the motion through replication, and a locally-manufactured pull there would double the
	// displacement on every observing client.
	const ENetRole LocalRole = CharacterOwner->GetLocalRole();
	if (LocalRole != ROLE_Authority && LocalRole != ROLE_AutonomousProxy)
	{
		return false;
	}

	const FBRGrappleTuning& Tuning = GetGrappleTuning();
	if (!Tuning.bValid)
	{
		// Already logged loudly, once, by GetGrappleTuning. Nothing applied, nothing to undo.
		return false;
	}

	// Replacing a pull with a pull is legal (the ability may retarget); leaving the old source
	// running while the new one overrides it is not — two Override sources fight for the same
	// frame and the winner depends on insertion order. One pull, one source, always.
	StopGrapplePull();

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const float Distance = FVector::Dist(Start, PullTargetWorld);
	if (Distance <= Tuning.ArrivalRadiusCm)
	{
		// Already there. Applying a zero-length source would divide by nothing and then fire the
		// arrival rule on its first frame anyway; refusing is the same outcome with no source ever
		// existing, which is the cheaper thing to reconcile.
		return false;
	}

	// CONSTANT SPEED, DERIVED DURATION — the pull covers `Distance` at `PullSpeed`, so a short
	// grapple is quick and a long one is not slower per metre. A fixed duration would make near
	// targets feel sluggish and far ones feel like a teleport, and it would also put a second
	// gameplay number in the table for designers to keep consistent with the first.
	//
	// The MaxPullSeconds clamp IS detach rule 3. Clamping the duration BELOW the honest travel time
	// means a pull that would take too long simply stops short — which is what a timeout is. In
	// normal play it never binds, because the ability's own range validation bounds `Distance`; it
	// binds when the character is wedged on geometry and the source would otherwise grind against a
	// wall for as long as the player held the button.
	const float Duration = FMath::Min(Distance / Tuning.PullSpeedCmPerSecond, Tuning.MaxPullSeconds);

	// ---------------------------------------------------------------------------------------
	// WHY FRootMotionSource_MoveToForce AND NOT THE OTHER TWO.
	//
	// MoveToForce's output position is a pure function of (StartLocation, TargetLocation, Duration,
	// elapsed time). Nothing else. That is the property prediction actually needs: the client and
	// the server, handed the same saved move, compute the same position without exchanging
	// anything, so reconciliation is a no-op instead of a correction every frame. Its Duration also
	// lives INSIDE the source, which is what makes the timeout replay-correct for free (see the
	// class comment) — and it is the reason detach rule 3 has no code.
	//
	// FRootMotionSource_RadialForce was rejected: it contributes a FORCE, so the resulting position
	// depends on accumulated velocity, gravity and whatever the character brushed against. Two
	// machines diverge by construction, it does not guarantee arrival at all, and "pull until you
	// roughly get there" is a physics toy, not a Halo grapple.
	//
	// FRootMotionSource_MoveToDynamicForce was rejected FOR NOW, and named as the seam: its whole
	// point is a target that moves after apply time (`SetTargetLocation`), which is what BP06
	// step 2's enemy-reel would want if the reel ever becomes two-body. It cannot be used here
	// because a moving target is a target the server never validated — the server checked range and
	// line of sight against ONE point, and a per-frame target stream is a per-frame trust decision
	// nobody has designed. If step 2 wants it, it needs a validation story first, not a swap.
	// ---------------------------------------------------------------------------------------
	TSharedPtr<FRootMotionSource_MoveToForce> Pull = MakeShared<FRootMotionSource_MoveToForce>();
	Pull->InstanceName = GrapplePullSourceName;
	Pull->Priority = GrapplePullSourcePriority;
	Pull->AccumulateMode = ERootMotionAccumulateMode::Override;
	Pull->StartLocation = Start;
	Pull->TargetLocation = PullTargetWorld;
	Pull->Duration = Duration;

	// Restricting speed to expected clamps the character to the source's nominal velocity, which
	// fights the source the moment it grazes geometry and turns a clean pull into a stutter along
	// a wall. Off; the Override accumulate mode is already the speed authority for this frame.
	Pull->bRestrictSpeedToExpected = false;

	// PathOffsetCurve (the arc), FinishVelocity handling (the arrival pop) and any camera treatment
	// are STEP 3's feel pass with anim-builder, and their numbers land in CT_Combat there. Left at
	// engine defaults on purpose: a shape chosen here by a netcode packet is a shape nobody
	// authored, and step 3 would have to undo it before it could do its job.
	Pull->PathOffsetCurve = nullptr;

	// A pull that starts on the ground must leave it, or the walking mode constrains the motion to
	// the floor plane and a grapple to a ledge slides along the ground instead of rising. This is
	// MOTION, not a gameplay decision, and the movement mode is saved-move state
	// (`StartPackedMovementMode`), so it rewinds with everything else.
	if (IsMovingOnGround())
	{
		SetMovementMode(MOVE_Falling);
	}

	ActiveGrapplePullSourceID = ApplyRootMotionSource(Pull);
	if (ActiveGrapplePullSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRCharacterMovementComponent '%s': ApplyRootMotionSource refused the grapple pull."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	GrapplePullTarget = PullTargetWorld;
	bWantsToGrapple = 1;

	// The ordering guard starts CLOSED. On the server it opens the first time a ServerMove arrives
	// carrying FLAG_Custom_1; until then a zero in that bit is a move the client recorded before it
	// pressed grapple, not a release. On the client it opens on the very next move, since the line
	// above set the intent this move will capture.
	bGrappleIntentObserved = 0;

	UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacterMovementComponent '%s': grapple pull -> %s (dist %.0f cm, %.2f s, source %u, role %d)."),
		*GetNameSafe(GetOwner()), *PullTargetWorld.ToCompactString(), Distance, Duration, (uint32)ActiveGrapplePullSourceID, (int32)LocalRole);

	return true;
}

void UBRCharacterMovementComponent::StopGrapplePull()
{
	if (ActiveGrapplePullSourceID != static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		// BY ID, never a blanket clear. `CurrentRootMotion.Clear()` would also delete a knockback,
		// a launcher or an animation's own root motion that happened to be running — a bug that
		// would first appear the day someone grapples out of an explosion, and would read as "the
		// explosion didn't knock me back" rather than as a grapple bug.
		RemoveRootMotionSourceByID(ActiveGrapplePullSourceID);
	}

	ClearGrapplePullState();
}

void UBRCharacterMovementComponent::ClearGrapplePullState()
{
	const bool bWasSomething = ActiveGrapplePullSourceID != static_cast<uint16>(ERootMotionSourceID::Invalid) || bWantsToGrapple;

	ActiveGrapplePullSourceID = static_cast<uint16>(ERootMotionSourceID::Invalid);
	GrapplePullTarget = FVector::ZeroVector;
	bWantsToGrapple = 0;
	bGrappleIntentObserved = 0;

	// THE COMPLETE LIST of this file's grapple state is the four lines above. That is the point:
	// there is no accumulated timer, no cached start location, no "was grappling last frame" bool,
	// and no position anywhere. A rejected grapple has nothing here to leave behind, which is how
	// the position third of BP06's zero-residue Done-when is met — by having nowhere to store
	// residue, not by remembering to erase it.
	//
	// The character's movement mode is deliberately NOT restored here. Whatever mode the pull left
	// the character in is the mode the simulation produced, it is saved-move state, and the server
	// and the client agree on it. Forcing MOVE_Walking on detach would be this file inventing a
	// position outcome, which is the one thing it must never do.

	if (bWasSomething)
	{
		UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacterMovementComponent '%s': grapple pull cleared."), *GetNameSafe(GetOwner()));
	}
}

void UBRCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// DETACH RULE 2 — JUMP-CANCEL. Before the base call, and before the move simulates, so this
	// frame is a jump frame and not a pull frame with a jump queued behind it.
	//
	// `bPressedJump` is saved-move state, restored by PrepMoveFor, so this test is replay-correct
	// with no work from us. Note what is NOT here: no check of whether jumping is *allowed*, no
	// stamina, no tag. "The player asked to jump" is the whole condition; whether the jump then
	// happens is the engine's business and whether the player may jump at all is the ability
	// layer's. Cancelling on the ASK rather than on the successful jump is deliberate — a grapple
	// you cannot escape because you were out of jumps would be the worse failure.
	if (CharacterOwner && CharacterOwner->bPressedJump && IsGrapplePullActive())
	{
		UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacterMovementComponent '%s': grapple detach — jump-cancel."), *GetNameSafe(GetOwner()));
		StopGrapplePull();
	}

	// DETACH — INTENT RELEASED. The falling edge of FLAG_Custom_1, gated by the ordering guard so a
	// ServerMove recorded before the grapple started cannot end it on frame one. See the class
	// comment: this is the unordered-RPC hazard, and this `if` is the whole mitigation.
	//
	// ASYMMETRIC ON PURPOSE, and worth knowing before you debug it. `bGrappleIntentObserved` is only
	// ever set by `UpdateFromCompressedFlags`, which the owning client does NOT call during ordinary
	// prediction — only the server calls it per ServerMove, and the client calls it while replaying.
	// So in normal client play this branch never fires, and it does not need to: on the client the
	// release comes from `BRGA_Grapple::EndAbility` calling StopGrapplePull directly. This branch is
	// how that release reaches the SERVER without a second RPC, and how a client REPLAY reproduces
	// the release at the same move it originally happened. On a simulated proxy every test here is
	// false because that role never holds a source ID.
	if (!bWantsToGrapple && bGrappleIntentObserved && IsGrapplePullActive())
	{
		UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacterMovementComponent '%s': grapple detach — intent released."), *GetNameSafe(GetOwner()));
		StopGrapplePull();
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UBRCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);

	if (ActiveGrapplePullSourceID == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return;
	}

	// DETACH RULE 3 — TIMEOUT, and simultaneously the REJECTION path, and they need no separate
	// code because at this level they are the same event: the source we applied is no longer in
	// `CurrentRootMotion`. Either its Duration expired and the engine pruned it, or a server
	// correction rewound the group to a state that never contained it. Both mean the same thing —
	// there is no pull — and the response to both is to stop believing there is one.
	//
	// This is the line that makes a rejected grapple leave nothing: the client's replay runs
	// without the source, ends at the server's position, and this call clears the last trace of
	// the pull from this component.
	if (!IsGrapplePullActive())
	{
		UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacterMovementComponent '%s': grapple detach — source gone (timeout, or corrected away)."),
			*GetNameSafe(GetOwner()));
		ClearGrapplePullState();
		return;
	}

	// DETACH RULE 1 — ARRIVAL RADIUS. After the move, because the question is about the position
	// the move just produced. A distance test on simulated position is a pure function of the
	// simulation, so the client and the server cross the radius on the same simulated frame and a
	// replay crosses it again at the same move — which is exactly the property a timer would not
	// have had.
	const FBRGrappleTuning& Tuning = GetGrappleTuning();
	if (Tuning.bValid && UpdatedComponent)
	{
		const float DistanceToTarget = FVector::Dist(UpdatedComponent->GetComponentLocation(), GrapplePullTarget);
		if (DistanceToTarget <= Tuning.ArrivalRadiusCm)
		{
			UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacterMovementComponent '%s': grapple detach — arrived (%.0f cm)."),
				*GetNameSafe(GetOwner()), DistanceToTarget);
			StopGrapplePull();
		}
	}
}
