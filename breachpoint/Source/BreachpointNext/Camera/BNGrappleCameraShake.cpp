#include "Camera/BNGrappleCameraShake.h"

UBNGrappleShakePattern::UBNGrappleShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// UCameraShakePattern has no default constructor, so the FObjectInitializer form is
	// mandatory — `= default` and a bare `()` both fail, the first by deleting itself and
	// the second by refusing to initialise the base.
}

void UBNGrappleShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(FMath::Max(0.01f, Duration));
}

void UBNGrappleShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
}

void UBNGrappleShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	const float Total = FMath::Max(0.01f, Duration);
	Elapsed = FMath::Min(Elapsed + Params.DeltaTime, Total);

	// Linear decay, deliberately: an exponential tail keeps the camera faintly alive for
	// long enough to read as drift on a 0.45s shake, and drift is what a player calls
	// "the camera feels loose".
	const float Decay = 1.f - (Elapsed / Total);
	const float Scale = Params.GetTotalScale() * Decay;

	OutResult.Rotation.Pitch = FMath::Sin(Elapsed * PitchFrequency) * PitchAmplitude * Scale;
	OutResult.Rotation.Yaw   = FMath::Sin(Elapsed * YawFrequency)   * YawAmplitude   * Scale;
	OutResult.FOV            = FMath::Sin(Elapsed * PitchFrequency * 0.5f) * FOVAmplitude * Scale;
}

bool UBNGrappleShakePattern::IsFinishedImpl() const
{
	return Elapsed >= FMath::Max(0.01f, Duration);
}

UBNGrappleCameraShake::UBNGrappleCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The pattern IS the shake's content. Set here rather than left for config, so the
	// class works the moment it is named and cannot be half-configured into silence.
	SetRootShakePattern(ObjectInitializer.CreateDefaultSubobject<UBNGrappleShakePattern>(this, TEXT("Pattern")));
}
