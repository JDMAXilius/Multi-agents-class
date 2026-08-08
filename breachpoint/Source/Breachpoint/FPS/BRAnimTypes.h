#pragma once

#include "CoreMinimal.h"

#include "BRAnimTypes.generated.h"

/**
 * Vocabulary for the animation spine. Types only -- no numbers.
 *
 * Every tuning value the FPS template kept on a class (`cardinalDirectionDeadZone` 10,
 * `rootYawOffsetAngleClamp` {-120,100}, `defaultWalkSpeed` 500) is a NUMBER, and law 3 puts
 * numbers in the `Content/Data` CSVs, not in a header. What lives here is the shape those numbers
 * are read into, plus the one enum the graph switches on.
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
 * Everything the worker thread needs, copied on the game thread.
 *
 * THIS STRUCT IS THE THREAD-SAFETY BOUNDARY and it is the whole reason the spine can obey
 * `animation.md` law 1 ("no game-thread reads outside the proxy's cached data"). The pawn, the
 * controller and the movement component are UObjects; touching any of them from
 * `NativeThreadSafeUpdateAnimation` is the hitch that has not happened yet. So the game-thread
 * pass reads them ONCE into these plain fields, and the worker pass computes from nothing else.
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
	 * `ACharacter::bIsCrouched`, read directly rather than through a tag.
	 *
	 * Worth naming because it is the one state in this spine that needs NO `contract_gap`:
	 * the engine already replicates crouch on the character, so a `State.Movement.Crouching`
	 * tag would be a second source of truth for something UE is already authoritative about.
	 */
	bool bIsCrouched = false;

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

	/** Critically-damped-ish step. Stiffness and damping come from a row, never from here. */
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
