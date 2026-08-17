#pragma once

#include "CoreMinimal.h"

#include "Animation/BRAnimInstance.h"

#include "BRAnimInstance3P.generated.h"

/**
 * The third-person body spine — the one everyone ELSE sees, and the one that speaks for the pawn.
 *
 * `animation.md` law 2, the other half of the pair. Everything shared is in `UBRAnimInstance`;
 * what is here is the handful of things that are only true of a body being observed.
 *
 * THE ASYMMETRY WORTH KNOWING. This class carries more than the 1P one, and that is not an
 * accident of effort — it is law 7. The third-person body is the mesh a **simulated proxy** view
 * is made of, so it is the one whose state arrives at the net update rate rather than per frame,
 * and the one whose errors are invisible to the player causing them. Turn-in-place, lean and the
 * aim offset all exist to make a body that is being watched read correctly, and all three are
 * fields the first-person arms deliberately zero.
 */
UCLASS(Config = Game)
class BREACHPOINT_API UBRAnimInstance3P : public UBRAnimInstance
{
	GENERATED_BODY()

public:

	UBRAnimInstance3P();

protected:

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/**
	 * Aim pitch, compressed toward the horizon before it reaches the spine.
	 *
	 * A player looking straight down is a legal camera pose and an illegal SPINE pose — the body
	 * folds in half. The camera can go where the skeleton cannot, so the observed body applies a
	 * fraction of it and the aim-offset asset covers the rest.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float AimPitchScale = 0.8f;

	/** Below this speed the body is "standing" for turn-in-place, even if it is drifting slightly. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Tuning")
	float StandingSpeedThreshold = 10.f;
};
