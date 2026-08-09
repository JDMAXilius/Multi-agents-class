#pragma once

#include "CoreMinimal.h"

#include "FPS/BRSwayAndLagTypes.h"

#include "BRAnimTypes.generated.h"

/**
 * Vocabulary for the animation spine. Types only -- no numbers.
 *
 * Types only, no numbers. What lives here is the shape values are read into, plus the one enum
 * the graph switches on.
 *
 * WHERE THE NUMBERS WENT, since the template kept them all on a class and law 3 forbids that.
 * The split is by what the number CHANGES, not by where it came from:
 *   - **Gameplay numbers** -> `Content/Data` CSV. `defaultWalkSpeed` 500 is one: it changes what
 *     the game does, so a designer must be able to retune it against a table.
 *   - **Presentation constants** -> `Config` on `UBRAnimInstance`. Sway stiffness, the cardinal
 *     dead zone, the root-yaw clamps. They change how it LOOKS and nothing else; a CSV row and a
 *     DataTable reimport for a spring constant is machinery with no reader.
 * Both are diffable by a critic, which is the property law 3 is actually protecting.
 */

/**
 * Which way the body is travelling relative to where it is looking.
 *
 * From `ABP_Mannequin_Base`'s `localVelocityDirection` / `cardinalDirectionFromAcceleration`
 * (inventory: both `string`, defaulting "Forward"). It is an enum here because a cardinal is a
 * closed set of four and a string comparison in an anim update is a per-frame allocation waiting
 * to happen.
 */
UENUM(BlueprintType)
enum class EBRAnimCardinal : uint8
{
	Forward,
	Backward,
	Left,
	Right
};

/**
 * Tag-driven state, as written by the ASC callback on the game thread.
 *
 * WHY THIS IS ITS OWN STRUCT and not nine loose bools on the AnimInstance. The ASC callback
 * fires from arbitrary game-thread code -- a GameplayEffect applied during some other actor's
 * tick -- while the parallel anim task may be in flight. As long as the worker only ever READ
 * these for the graph, a stale frame was the worst case and nobody cared.
 *
 * That stopped being true the moment the worker started COMPUTING from them (`bADSStateChanged`
 * is an edge, `UpperBodyAdditiveWeight` is a product). An edge computed against a value that
 * changes between the compare and the store is **lost permanently**, and a state machine waiting
 * on it never transitions. So the callback writes here, the game pass latches this into the
 * snapshot, and the worker reads only its own copy -- the same discipline every other field
 * already followed.
 */
struct FBRAnimTagState
{
	bool bSprinting = false;
	bool bReloading = false;
	bool bSwapping = false;
	bool bMeleeing = false;
	bool bGrappling = false;
	bool bThrowingGrenade = false;
	bool bDead = false;

	/** Bound to nothing until BP93 declares `State.Weapon.ADS` / `.Firing` (contract_gap BP82-2). */
	bool bADS = false;
	bool bFiring = false;
};

/**
 * Everything the worker thread needs, copied on the game thread.
 *
 * THIS STRUCT IS THE THREAD-SAFETY BOUNDARY and it is the whole reason the spine can obey
 * `animation.md` law 1 ("no game-thread reads outside the proxy's cached data"). The pawn, the
 * controller and the movement component are UObjects; touching any of them from
 * `NativeThreadSafeUpdateAnimation` is the hitch that has not happened yet. So the game-thread
 * pass reads them ONCE into these plain fields, and the worker pass computes from nothing else.
 *
 * "Nothing else" is meant literally, and it did not used to be. An earlier revision let the
 * worker compute from `LinkedLayerRow` and from the ASC-callback bools, which happened to be
 * ordering-safe but made this comment false -- and a boundary that is documented but not
 * enforced is one careless addition away from being neither. Everything the worker computes
 * from now arrives here, including the tag state and the linked layer.
 *
 * Deliberately not a `USTRUCT` with `UPROPERTY`s: nothing reflects it, nothing serialises it,
 * and no graph reads it. It is a snapshot, and giving it reflection would invite both.
 */
struct FBRAnimSnapshot
{
	FVector WorldLocation = FVector::ZeroVector;
	FRotator WorldRotation = FRotator::ZeroRotator;
	FVector WorldVelocity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;

	/** Where the pawn is AIMING, which is not where its body faces -- the difference is the aim offset. */
	FRotator BaseAimRotation = FRotator::ZeroRotator;

	float MaxSpeed = 0.f;

	bool bIsOnGround = false;
	bool bIsFalling = false;

	/**
	 * Gravity, copied rather than assumed.
	 *
	 * `TimeToJumpApex` is `-VelocityZ / GravityZ`, and a hardcoded -980 would be wrong for any
	 * pawn with a gravity scale -- which is every pawn the moment someone tunes jump feel. It is
	 * read from the movement component on the game thread because that is where the component is.
	 */
	float GravityZ = -980.f;

	/** Needed to normalise vertical weapon lag; falling speed is unrelated to walk speed. */
	float JumpZVelocity = 420.f;

	/** Per-frame delta of the CONTROL rotation -- what the source drives sway from. */
	FRotator ControlRotationDelta = FRotator::ZeroRotator;

	/** Actor basis, so the worker can project velocity without touching the pawn. */
	FVector ActorForward = FVector::ForwardVector;
	FVector ActorRight = FVector::RightVector;
	FVector ActorUp = FVector::UpVector;

	/**
	 * `ACharacter::bIsCrouched`, read directly rather than through a tag.
	 *
	 * Worth naming because it is the one state in this spine that needs NO `contract_gap`:
	 * the engine already replicates crouch on the character, so a `State.Movement.Crouching`
	 * tag would be a second source of truth for something UE is already authoritative about.
	 */
	bool bIsCrouched = false;

	/** Latched from the ASC callback's own copy once per game-thread pass. See `FBRAnimTagState`. */
	FBRAnimTagState Tags;

	/**
	 * Did the actor's rotation actually change since the last GAME-THREAD pass?
	 *
	 * Not derivable on the worker thread, which is the whole point. For a simulated proxy the
	 * actor rotation is a step function refreshed at the net update rate, so a per-frame yaw
	 * delta reads exactly zero on most frames while the player is genuinely mid-turn. Anything
	 * that means "the turn has stopped" must ask whether NEW DATA ARRIVED, and only the game
	 * pass knows that.
	 */
	bool bRotationChanged = false;

	/** Which weapon row the linked layer serves, asked through `IBRAnimLayer` on the game thread. */
	FName LayerRow;

	/**
	 * The linked layer's sway/lag profile, latched with everything else.
	 *
	 * Crosses the boundary as data rather than being asked for on the worker thread, because
	 * asking means calling through a UObject interface — which law 1 forbids from that pass.
	 */
	FBRSwayAndLagInfo LayerSwayAndLag;
	bool bLayerOverridesHandPose = false;

	/** False until the pawn is real. Every consumer checks it; a zeroed snapshot is not a pose. */
	bool bValid = false;
};

/**
 * Spring state for one axis of weapon sway.
 *
 * Amendment A puts "sway / bob / recoil / spring damping" in C++ and calls it the place we go
 * further than Lyra. This is that state, and it is per-instance rather than global because two
 * meshes (1P arms, 3P body) sway independently off the same camera motion.
 */
struct FBRSpring1D
{
	float Value = 0.f;
	float Velocity = 0.f;

	/**
	 * Critically-damped-ish step. Stiffness and damping are passed in, never stored here.
	 *
	 * They live as `Config` properties on `UBRAnimInstance` -- NOT in a data row. Corrected from
	 * an earlier comment that said "a row": these are presentation constants that change nothing
	 * about what the game does, so law 3's CSV requirement (which is about *gameplay* numbers)
	 * does not reach them, and `DefaultGame.ini` is the smaller mechanism. The distinction is
	 * whether a critic can diff the value, and for a config property they can.
	 */
	void Step(float Target, float Stiffness, float Damping, float DeltaSeconds)
	{
		if (DeltaSeconds <= 0.f)
		{
			return;
		}

		// Semi-implicit Euler. Explicit Euler blows up at the frame rates a spike produces, and
		// an anim spring that explodes puts the weapon behind the camera for one visible frame.
		const float Accel = (Target - Value) * Stiffness - Velocity * Damping;
		Velocity += Accel * DeltaSeconds;
		Value += Velocity * DeltaSeconds;
	}

	void Reset()
	{
		Value = 0.f;
		Velocity = 0.f;
	}
};
