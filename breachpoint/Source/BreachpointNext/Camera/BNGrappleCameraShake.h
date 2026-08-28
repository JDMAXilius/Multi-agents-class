#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "BNGrappleCameraShake.generated.h"

/**
 * BN23 — the grapple's camera kick, IN CODE.
 *
 * Written as a pattern rather than configured as an asset for one reason: everything else
 * this packet added to the grapple (rope, sounds, montage, rumble) is a soft pointer at
 * Tier-4 content that does not exist yet, so the whole feature is invisible until an
 * artist authors it. A shake needs no asset, so this is the one piece of the polish that
 * can actually be FELT today — and the founder should not have to take "the plumbing is
 * correct" on faith with nothing on screen.
 *
 * Engine's own oscillator patterns (WaveOscillator, PerlinNoise) live in the
 * GameplayCameras PLUGIN, which this module does not depend on. UCameraShakePattern itself
 * is Engine, so the pattern is implemented here instead of taking a plugin dependency for
 * forty lines of sine.
 *
 * The shape is a recoil, not a rumble: a hard pitch punch that decays, with a small yaw
 * wobble so it does not read as a purely vertical machine motion, and a brief FOV pull
 * that sells the yank. Amplitude decays linearly over Duration; nothing loops.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGrappleShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	UBNGrappleShakePattern(const FObjectInitializer& ObjectInitializer);

	/** Seconds. Short — this is the launch, not the flight. */
	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float Duration = 0.45f;

	/** Degrees of upward pitch at t=0, decaying to zero. */
	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float PitchAmplitude = 2.2f;

	/** Degrees of yaw wobble, deliberately smaller and at a different rate than pitch. */
	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float YawAmplitude = 0.9f;

	/** Degrees of FOV pull-in at t=0. Negative narrows the view, which reads as speed. */
	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float FOVAmplitude = -1.5f;

	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float PitchFrequency = 22.f;

	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float YawFrequency = 14.f;

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

	float Elapsed = 0.f;
};

/** The shake the cue instantiates. Exists so DefaultGame.ini can name ONE class. */
UCLASS()
class BREACHPOINTNEXT_API UBNGrappleCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UBNGrappleCameraShake(const FObjectInitializer& ObjectInitializer);
};
