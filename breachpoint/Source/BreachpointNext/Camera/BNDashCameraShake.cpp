#include "Camera/BNDashCameraShake.h"

UBNDashShakePattern::UBNDashShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// UCameraShakePattern's constructor takes an FObjectInitializer and nothing else here
	// needs doing — every value is a UPROPERTY default above.
}

void UBNDashShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
}

void UBNDashShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
}

void UBNDashShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const float T = Duration > 0.f ? FMath::Clamp(Elapsed / Duration, 0.f, 1.f) : 1.f;

	// A HALF SINE, not a decaying oscillation. The grapple wobbles because a hook yanks;
	// a dash is one push, so the camera goes out and comes back exactly once. An oscillation
	// here would read as a stumble — the body tripping rather than accelerating.
	const float Envelope = FMath::Sin(T * PI);

	OutResult.Location = FVector::ZeroVector;
	OutResult.Rotation = FRotator(
		PitchAmplitude * Envelope,                    // pitch: the weight of the shove
		0.f,                                          // yaw: none — a dash does not steer the view
		RollAmplitude * Envelope * FMath::Sign(RollSign == 0.f ? 0.f : RollSign));
	OutResult.FOV = FOVAmplitude * Envelope;
}

bool UBNDashShakePattern::IsFinishedImpl() const
{
	return Elapsed >= Duration;
}

UBNDashCameraShake::UBNDashCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The pattern IS the shake's content — set here rather than left to config, so the class
	// works the moment it is named and cannot be half-configured into silence.
	SetRootShakePattern(ObjectInitializer.CreateDefaultSubobject<UBNDashShakePattern>(this, TEXT("Pattern")));
}
