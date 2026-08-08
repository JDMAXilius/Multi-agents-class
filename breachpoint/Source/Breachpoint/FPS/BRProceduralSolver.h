#pragma once

#include "CoreMinimal.h"

#include "FPS/BRAimAndLeanTypes.h"
#include "FPS/BRAnimTypes.h"
#include "FPS/BRPoseOffsetTypes.h"
#include "FPS/BRProceduralTypes.h"
#include "FPS/BRRecoilTypes.h"
#include "FPS/BRSwayAndLagTypes.h"

/**
 * The procedural maths, as free functions over plain data.
 *
 * WHY THIS IS NOT A COMPONENT, which is the one design decision in this file. The template puts
 * every one of these on a `UActorComponent` and runs it from `Tick` — five components, on the
 * pawn, ticking on dedicated servers that render nothing. That shape costs an object, a tick
 * registration and a replication surface per concern, and it puts animation maths on the game
 * thread, which `animation.md` law 1 forbids outright.
 *
 * As free functions over structs, the same maths is:
 *   · **worker-thread safe** — no UObject is reachable from any signature here, so law 1 holds by
 *     construction rather than by review;
 *   · **headless-testable** — every function is pure input → output with its mutable state passed
 *     in by reference, so a spec can drive a hundred frames of recoil with no engine at all;
 *   · **free on the server** — nothing to tick, because there is nothing to register.
 *
 * ALL STATE IS EXPLICIT. Springs and force lists are passed in as references rather than hidden
 * in a class, so a caller can hold two independent sets — which is exactly what the first- and
 * third-person spines need from the same camera motion.
 */
namespace BRProcedural
{
	/**
	 * Stiffness per unit of `SwayInterpSpeed`, and the damping ratio held across the whole range.
	 *
	 * Named constants rather than literals in the formula because their RELATIONSHIP is the design:
	 * zeta is what decides whether a weapon settles crisply or rings, and it must not drift when a
	 * designer turns the speed knob. 0.74 sits just under critical -- one small overshoot, which
	 * reads as weight; a perfectly critical spring reads as lifeless.
	 */
	static constexpr float SwayStiffnessPerSpeed = 9.f;
	static constexpr float SwayDampingRatio = 0.74f;

	/** Below this speed (cm/s) there is no lag direction. Matches the spine's own "is moving" test. */
	static constexpr float BR_MinLagSpeed = 1.f;

	/**
	 * A weight sum below this fraction of the largest single weight is treated as no distribution.
	 * Catches mixed-sign tables, where the sum is small but the shares are enormous.
	 */
	static constexpr float BR_MinWeightSumRatio = 0.01f;
	/**
	 * Advance a spring one step and return its value.
	 *
	 * `Step` is expected to be **already clamped** by the caller. Semi-implicit Euler at the
	 * stiffness/damping this system uses (90/14) sign-flips above ~0.071 s and diverges above
	 * ~0.106 s, so an unclamped frame spike does not degrade the motion — it inverts it.
	 */
	BREACHPOINT_API float StepSpring(FBRSpring1D& Spring, float Target, const FBRSpringSetting& Setting, float Step);

	/**
	 * Rotational sway: the weapon lagging behind a turn and settling.
	 *
	 * Driven by the **rate** of view rotation, never the angle — a constant angle is a constant
	 * spring target, which settles to a constant offset and never returns. The source computes it
	 * the same way (`swayPrevControlRotation` → `swayControlRotDelta`), which is the second
	 * independent confirmation of that after `ABP_Mannequin_Base`'s `camRotRate`.
	 */
	BREACHPOINT_API FRotator SolveSway(
		const FBRSwayAndLagInfo& Info,
		float YawRateDegrees,
		float PitchRateDegrees,
		bool bIsADS,
		float MaxAngle,
		float Step,
		FBRSpring1D& YawSpring,
		FBRSpring1D& PitchSpring);

	/**
	 * Positional lag: the weapon trailing in space as the body moves, then catching up.
	 *
	 * Separate from sway because they are separate intuitions — sway is the weight of the gun
	 * resisting a turn, lag is the body carrying it. The airborne multiplier exists because a
	 * falling character's vertical velocity would otherwise drag the weapon off-screen; it is the
	 * one field the pack's own author documented.
	 */
	BREACHPOINT_API FVector SolveLag(
		const FBRSwayAndLagInfo& Info,
		const FVector& LocalVelocity,
		bool bIsADS,
		bool bIsFalling,
		float Step,
		FVector& LagOffset);

	/**
	 * Sum the live recoil forces and drop the expired ones.
	 *
	 * Additive on purpose: several shots fired quickly stack into one kick that decays as a whole,
	 * rather than each shot restarting the motion. A weapon whose recoil resets per shot feels
	 * lighter the faster you fire it, which is backwards.
	 *
	 * `NowSeconds` is passed in rather than read from a world, because a world is a UObject and
	 * this function runs on the worker thread.
	 */
	BREACHPOINT_API void AccumulateForces(
		TArray<FBRProceduralForce>& ActiveForces,
		float NowSeconds,
		FVector& OutLocation,
		FRotator& OutRotation);

	/**
	 * Build one kick from an envelope, given a caller-supplied roll in 0..1.
	 *
	 * **The roll is an argument, deliberately.** Recoil is randomised between a min and a max, and
	 * a function that rolled its own would produce a different kick on the client and the server
	 * for the same shot — not as a bug, as arithmetic. Whoever owns the shot owns the seed. See
	 * `animation.md` A.6: camera recoil is server-validated and belongs to the fire path.
	 */
	BREACHPOINT_API FBRProceduralForce MakeRecoilForce(
		const FBRRecoilImpulse& Impulse,
		float Alpha,
		float NowSeconds);

	/**
	 * Share one rotation out along a spine, weighted per bone.
	 *
	 * The whole reason an aim offset reads as *aiming* rather than as bending: the distribution
	 * decides whether the head glances or the spine arches. The pack's own measurement is
	 * neck-heavy (`neck_02` 0.2 against `head` 0.1).
	 *
	 * **Weights are normalised**, so a table that does not sum to 1 still produces exactly the
	 * requested total rotation. Without that, editing one bone's weight silently rescales the
	 * whole offset — a table that is wrong in a way that looks like a tuning change.
	 */
	BREACHPOINT_API void DistributeSpineRotation(
		const FBRSpineWeights& Weights,
		const FRotator& TotalRotation,
		TMap<FName, FRotator>& OutPerBone);

	/**
	 * Pick the ADS offset matching a fitted optic and stance.
	 *
	 * Falls back to the idle item rather than to identity when nothing matches: identity means
	 * "the weapon is where the base pose put it", which for an ADS pose is the sight nowhere near
	 * the camera. A wrong-but-plausible pose is easier to notice and fix than a silent identity.
	 */
	BREACHPOINT_API const FBRPoseOffsetItem& SelectPoseOffset(
		const FBRPoseOffsetInfo& Info,
		FName ScopeType,
		bool bIsADS,
		bool bIsCrouched);
}
