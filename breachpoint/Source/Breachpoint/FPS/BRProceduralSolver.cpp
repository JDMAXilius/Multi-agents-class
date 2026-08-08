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

	// InterpSpeed is the source's tuning knob and maps to stiffness; damping is derived to sit
	// slightly under critical (zeta ~0.74 at the defaults) so the weapon settles with one small
	// overshoot rather than creeping in. A perfectly critical spring reads as damped and lifeless.
	const FBRSpringSetting Setting{ Info.SwayInterpSpeed * 9.f, Info.SwayInterpSpeed * 1.4f };

	return FRotator(
		StepSpring(PitchSpring, PitchTarget, Setting, Step),
		StepSpring(YawSpring, YawTarget, Setting, Step),
		0.f);
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

	// Normalised then scaled, NOT scaled raw. Raw velocity means lag grows without bound with
	// speed, so a sprint or a launch pad throws the weapon out of frame. Direction is the signal;
	// LagDistance is the entire budget.
	const FVector Direction = LocalVelocity.GetSafeNormal();
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
	if (FMath::IsNearlyZero(Total))
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
