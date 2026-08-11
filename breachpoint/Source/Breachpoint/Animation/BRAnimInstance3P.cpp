#include "Animation/BRAnimInstance3P.h"

UBRAnimInstance3P::UBRAnimInstance3P()
{
}

void UBRAnimInstance3P::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// WORKER THREAD still -- law 1 inherited from the base, not restated.

	// The camera can look somewhere the skeleton cannot follow. Straight down is a legal view and
	// a body folded in half; scaling before the graph sees it keeps the aim-offset asset working
	// inside the range it was actually authored for.
	AimPitch *= AimPitchScale;

	// "Standing" for turn-in-place is not the same question as "has any velocity at all". A body
	// drifting at 3 cm/s on a slope, or a simulated proxy being nudged by correction, is standing
	// as far as a player watching it is concerned -- and the base's turn-in-place branch keys off
	// `bHasVelocity`, which such a body technically has.
	bHasVelocity = bHasVelocity && GroundSpeed > StandingSpeedThreshold;
}
