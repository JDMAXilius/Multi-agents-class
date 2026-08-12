#include "Animations/BRProceduralSolver.h"

namespace BRProcedural
{

float StepSpring(FBRSpring1D& Spring, float Target, const FBRSpringSetting& Setting, float Step)
{
	Spring.Step(Target, Setting.Stiffness, Setting.Damping, Step);
	return Spring.Value;
}

FRotator SolveSway(
	const FBRSwayAndLagInfo& Info,
	const FRotator& ControlRotationDelta,
	float ApplySwayAlpha,
	bool bIsADS,
	float DeltaSeconds,
	FRotator& SwayState)
{
	const float Multiply = (bIsADS ? Info.SwayMultiplyADS : Info.SwayMultiplyDefault) * ApplySwayAlpha;
	const float Limit = Info.SwayMaxDeltaDegrees;

	// Pitch negated, yaw not. Asymmetric in the source and it is not a typo there: looking UP
	// should drop the muzzle, while turning right should trail the weapon right. Negating both
	// -- which the first version did -- makes the weapon lead the turn instead of following it.
	const FRotator Target(
		FMath::Clamp(-ControlRotationDelta.Pitch, -Limit, Limit) * Multiply,
		FMath::Clamp(ControlRotationDelta.Yaw, -Limit, Limit) * Multiply,
		0.f);

	// RInterpTo, NOT a spring. `SwayInterpSpeed` is an interpolation speed; the previous version
	// fed it to a spring as stiffness and derived damping from it, which is a category error --
	// and it is why that code needed a timestep clamp and an output clamp to stay stable.
	// RInterpTo clamps its own alpha, so it is unconditionally stable at any delta.
	SwayState = FMath::RInterpTo(SwayState, Target, DeltaSeconds, Info.SwayInterpSpeed);
	return SwayState;
}

FVector SolveLag(
	const FBRSwayAndLagInfo& Info,
	const FVector& WorldVelocity,
	const FVector& ActorForward,
	const FVector& ActorRight,
	const FVector& ActorUp,
	float MaxWalkSpeed,
	float JumpZVelocity,
	bool bIsADS,
	float DeltaSeconds,
	FVector& LagState)
{
	const float Multiply = bIsADS ? Info.LagMultiplyADS : Info.LagMultiplyDefault;

	// Each component divided by the maximum that is RELEVANT to it. Strafe and forward against
	// walk speed; vertical against jump velocity, because falling speed has nothing to do with
	// how fast you walk and dividing it by walk speed sends the weapon off-screen in a long drop.
	const float SafeWalk = FMath::Max(MaxWalkSpeed, 1.f);
	const float SafeJump = FMath::Max(FMath::Abs(JumpZVelocity), 1.f);

	const FVector Target(
		(FVector::DotProduct(WorldVelocity, ActorRight) / SafeWalk) * Info.LagStrafeScale,
		(FVector::DotProduct(WorldVelocity, ActorForward) / -SafeWalk) * Info.LagForwardScale,
		(FVector::DotProduct(WorldVelocity, ActorUp) / -SafeJump) * Info.LagMultiplyAir);

	// Clamped as a VECTOR, not per axis: clamping components independently would let a diagonal
	// sprint travel further than either axis allows, which is the classic square-instead-of-circle
	// bug and shows up as the weapon drifting further on diagonals.
	const FVector Clamped = (Target * Multiply).GetClampedToMaxSize(Info.LagDistance);

	// VInterpTo clamps its own alpha, so any positive speed is stable at any timestep.
	LagState = FMath::VInterpTo(LagState, Clamped, DeltaSeconds, Info.LagInterpSpeed);
	return LagState;
}

void AccumulateForces(
	TArray<FBRProceduralForce>& ActiveForces,
	float NowSeconds,
	FVector& OutLocation,
	FRotator& OutRotation)
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;

	// Reverse iteration so RemoveAtSwap cannot skip an element -- the classic filter-in-place bug,
	// and one that would show as recoil that occasionally never decays.
	for (int32 Index = ActiveForces.Num() - 1; Index >= 0; --Index)
	{
		const FBRProceduralForce& Force = ActiveForces[Index];

		if (Force.EndTime <= NowSeconds)
		{
			ActiveForces.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		OutLocation += Force.Location;
		OutRotation += Force.Rotation;
	}
}

FBRProceduralForce MakeRecoilForce(const FBRRecoilImpulse& Impulse, float Alpha, float NowSeconds)
{
	const float T = FMath::Clamp(Alpha, 0.f, 1.f);

	FBRProceduralForce Force;
	Force.Location = FMath::Lerp(Impulse.MinForceLocation, Impulse.MaxForceLocation, T);
	Force.Rotation = FMath::Lerp(Impulse.MinForceRotation, Impulse.MaxForceRotation, T);
	Force.EndTime = NowSeconds + FMath::Lerp(Impulse.MinForceDuration, Impulse.MaxForceDuration, T);
	return Force;
}

void DistributeSpineRotation(
	const FBRSpineWeights& Weights,
	const FRotator& TotalRotation,
	TMap<FName, FRotator>& OutPerBone)
{
	OutPerBone.Reset();

	const float Total = Weights.TotalWeight();

	// `IsNearlyZero` alone is not enough, and the gap is not academic. Weights of opposite sign
	// -- one typed minus in a bone table -- can sum to a small NON-zero total while the individual
	// magnitudes stay large: {head: 1.0, neck_01: -0.999} totals 0.001, so the shares become
	// +1000 and -999 and a 30-degree aim pitch becomes 30,000 degrees on one bone. That is not a
	// wrong pose, it is destroyed geometry. Guarding on the total against the largest magnitude
	// catches the sign case, which a magnitude-free epsilon cannot.
	float MaxMagnitude = 0.f;
	for (const TPair<FName, float>& Pair : Weights.BoneWeights)
	{
		MaxMagnitude = FMath::Max(MaxMagnitude, FMath::Abs(Pair.Value));
	}

	if (FMath::IsNearlyZero(Total) || FMath::Abs(Total) < MaxMagnitude * BR_MinWeightSumRatio)
	{
		// An empty or all-zero table means "no distribution authored", which is a legitimate
		// answer (use the spine's default), not an error. Returning an empty map says exactly
		// that; spreading the rotation evenly would invent a pose nobody authored.
		return;
	}

	OutPerBone.Reserve(Weights.BoneWeights.Num());
	for (const TPair<FName, float>& Pair : Weights.BoneWeights)
	{
		// Normalised, so a table summing to 0.9 or 1.2 still yields exactly TotalRotation.
		// Without this, editing one bone silently rescales the whole offset -- a table that is
		// wrong in a way that looks like a tuning change.
		const float Share = Pair.Value / Total;
		OutPerBone.Add(Pair.Key, TotalRotation * Share);
	}
}

const FBRPoseOffsetItem& SelectPoseOffset(
	const FBRPoseOffsetInfo& Info,
	FName ScopeType,
	bool bIsADS,
	bool bIsCrouched)
{
	if (bIsADS)
	{
		// Crouched ADS is its OWN array, not the crouch offset plus the ADS offset. A lowered
		// stance changes the shoulder line, so the two corrections do not compose.
		const TArray<FBRADSPoseOffsetItem>& Items = bIsCrouched ? Info.ADSCrouchItems : Info.ADSItems;

		for (const FBRADSPoseOffsetItem& Item : Items)
		{
			if (Item.ScopeType == ScopeType)
			{
				return Item.Item;
			}
		}

		// Nothing matched the fitted optic. Falling back to the stance pose is wrong-but-visible;
		// falling back to identity would put the sight nowhere near the camera and read as a
		// broken weapon rather than a missing table row.
	}

	return bIsCrouched ? Info.Crouch : Info.Idle;
}

} // namespace BRProcedural
