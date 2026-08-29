#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "BNDashCameraShake.generated.h"

/**
 * THE DASH'S CAMERA, and the one piece of polish that does the most work.
 *
 * A dash reads as speed through the CAMERA, not through the character mesh — the player
 * cannot see their own third-person body. So this leans on the three things a first-person
 * view can say: a ROLL toward the direction travelled, a small FOV widen, and a settle.
 *
 * ROLL IS THE SIGNATURE, and it is why this is not UBNGrappleShakePattern with new numbers.
 * The grapple is a PULL from a fixed point: it wobbles and narrows. A dash is a LATERAL
 * BURST, and a body thrown sideways banks into it — so the camera tips toward the dash and
 * comes back level. Dodge left, the world tips left. That single degree of roll is what
 * separates "the character moved" from "I moved".
 *
 * Directional by construction: the ability hands in a roll SIGN, so a left dash and a right
 * dash are mirror images rather than the same generic shake played twice.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNDashShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	UBNDashShakePattern(const FObjectInitializer& ObjectInitializer);

	/** Seconds. Short and asymmetric in feel: the kick is immediate, the settle is the rest. */
	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float Duration = 0.38f;

	/** Degrees of roll at the peak. Signed by the ability: negative rolls the other way.
	 *  Small on purpose — 3 degrees reads as a lean, 10 reads as a bug. */
	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float RollAmplitude = 3.2f;

	/** Degrees of FOV WIDEN at the peak. Positive, unlike the grapple's narrow: a dash is
	 *  acceleration away from where you stood, and widening sells that. */
	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float FOVAmplitude = 2.4f;

	/** A single downward pitch dip — the weight of the push — rather than a wobble. */
	UPROPERTY(EditAnywhere, Category = "BN|Shake")
	float PitchAmplitude = -1.1f;

	/** Set by the ability before the shake starts: +1 dashing right, -1 left, 0 straight
	 *  ahead or back (where a roll would read as a stumble rather than a bank). */
	UPROPERTY(BlueprintReadWrite, Category = "BN|Shake")
	float RollSign = 1.f;

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

	float Elapsed = 0.f;
};

/** The shake the ability instantiates. Exists so DefaultGame.ini can name ONE class — a
 *  UCameraShakePattern is CONTENT, not a shake, and StartCameraShake takes a UCameraShakeBase.
 *  Naming the pattern directly in config resolves to nothing and fails silently, which is
 *  exactly what it did on the first pass here. */
UCLASS()
class BREACHPOINTNEXT_API UBNDashCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UBNDashCameraShake(const FObjectInitializer& ObjectInitializer);
};
