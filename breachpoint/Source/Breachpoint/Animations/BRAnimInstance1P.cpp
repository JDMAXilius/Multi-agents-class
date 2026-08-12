#include "Animations/BRAnimInstance1P.h"

UBRAnimInstance1P::UBRAnimInstance1P()
{
}

void UBRAnimInstance1P::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// WORKER THREAD still -- the base's law-1 discipline is inherited, not restated.

	// The camera IS the aim in first person. The arms are parented under it, so they have already
	// been rotated by the look input before a single anim node runs; applying the aim offset
	// again would pitch them twice as far as the player looked. The field stays published for the
	// graph to READ (an ADS pose can lean on it) -- it just must not drive the arms' rotation.
	AimPitch = 0.f;
	AimYaw = 0.f;

	// Lean is a third-person read. Arms that roll with the turn read as the whole screen tilting.
	LeanAngle = 0.f;

	SwayRotation = FRotator(
		SwayRotation.Pitch * FirstPersonSwayScale,
		SwayRotation.Yaw * FirstPersonSwayScale,
		0.f);
}
