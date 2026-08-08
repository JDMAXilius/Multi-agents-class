#include "FPS/BRProceduralSolver.h"

namespace BRProcedural
{

float StepSpring(FBRSpring1D& Spring, float Target, const FBRSpringSetting& Setting, float Step)
{
	Spring.Step(Target, Setting.Stiffness, Setting.Damping, Step);
	return Spring.Value;
}

FRotator SolveSway(
	const FBRSwayAndLagInfo& Info,
	float YawRateDegrees,
	float PitchRateDegrees,
	bool bIsADS,
	float MaxAngle,
	float Step,
	FBRSpring1D& YawSpring,
	FBRSpring1D& PitchSpring)
{
	const float Multiply = bIsADS ? Info.SwayMultiplyADS : Info.SwayMultiplyDefault;

	// Negated: the weapon lags OPPOSITE the turn. Turning right leaves it trailing to the left,
	// which is the whole read -- a weapon that swings the way you turn looks like it is leading
	// the player rather than being carried by them.
	const float YawTarget = FMath::Clamp(-YawRateDegrees * Multiply, -MaxAngle, MaxAngle);
	const float PitchTarget = FMath::Clamp(-PitchRateDegrees * Multiply, -MaxAngle, MaxAngle);

	// DAMPING IS DERIVED FOR A CONSTANT DAMPING RATIO, not a constant ratio to stiffness. That
	// distinction is the whole fix for a defect this function shipped with.
	//
	// It used to be `{s * 9, s * 1.4}`, chosen so zeta = c / (2*sqrt(k)) landed near 0.74 -- at
	// s = 10, which was a MIS-TRANSCRIBED default. The source's real `sway_InterpSpeed` is 3
	// (all nine weapons), and at s = 3 that formula gives zeta = 0.2333*sqrt(3) = 0.40, i.e. a
	// 25% overshoot: every weapon visibly wobbles past its rest position on every turn. The
	// constant was calibrated against a number the pack does not contain, so loading the pack's
	// real values -- the entire point of the struct -- would have introduced the ring.
	//
	// zeta is now held fixed and `c` solved from `k`: c = 2*zeta*sqrt(k). The knob stays a knob
	// across its whole range instead of silently changing the feel as it is turned.
	const float Stiffness = FMath::Max(Info.SwayInterpSpeed, 0.01f) * SwayStiffnessPerSpeed;
	const FBRSpringSetting Setting{ Stiffness, 2.f * SwayDampingRatio * FMath::Sqrt(Stiffness) };

	// The TARGET was clamped; the spring's own value was not. At a high enough stiffness the
	// integrator overshoots past the rail even from a clamped target, so the output is clamped
	// too -- the same unbounded-output failure the timestep clamp closed, reachable through the
	// stiffness knob instead, from an unvalidated per-weapon table value.
	const float Pitch = FMath::Clamp(StepSpring(PitchSpring, PitchTarget, Setting, Step), -MaxAngle, MaxAngle);
	const float Yaw = FMath::Clamp(StepSpring(YawSpring, YawTarget, Setting, Step), -MaxAngle, MaxAngle);

	return FRotator(Pitch, Yaw, 0.f);
}

FVector SolveLag(
	const FBRSwayAndLagInfo& Info,
	const FVector& LocalVelocity,
	bool bIsADS,
	bool bIsFalling,
	float Step,
	FVector& LagOffset)
{
	float Multiply = bIsADS ? Info.LagMultiplyADS : Info.LagMultiplyDefault;
	if (bIsFalling)
	{
		// The pack documents this one in the asset itself. Falling velocity is large and vertical;
		// without its own multiplier it drags the weapon clean off the bottom of the screen.
		Multiply *= Info.LagMultiplyAir;
	}

	// A MINIMUM SPEED, because `GetSafeNormal`'s tolerance is ~1e-8 on the squared length: without
	// this, 5 cm/s of friction settle or network micro-jitter displaces the weapon exactly as far
	// as an 800 cm/s sprint. The threshold matches the spine's own "is moving" test (1 cm/s), so
	// there is no band where the spine reports stationary while the weapon wanders at full
	// amplitude -- which is precisely what the mismatch produced.
	const FVector Direction = LocalVelocity.SizeSquared() > (BR_MinLagSpeed * BR_MinLagSpeed)
		? LocalVelocity.GetSafeNormal()
		: FVector::ZeroVector;
	const FVector Target = -Direction * Info.LagDistance * Multiply;

	// Interp rather than spring: lag should trail and catch up, never overshoot past the hands.
	LagOffset = FMath::VInterpTo(LagOffset, Target, Step, Info.LagInterpSpeed);
	return LagOffset;
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
