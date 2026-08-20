#include "Animation/BNADSCameraBlend.h"

#include "Camera/CameraComponent.h"

void FBNADSCameraBlend::Update(UCameraComponent* Camera, bool bADS, bool bOwnerFirstPerson, float DeltaSeconds)
{
	if (!Camera || !bOwnerFirstPerson)
	{
		return;
	}

	// Captured, not assumed: whatever the camera was authored at IS the hip FOV, and ADS returns
	// to exactly that. Capturing once also means a mid-blend re-entry does not latch a partially
	// zoomed value as the new default.
	if (DefaultFOV <= 0.f)
	{
		DefaultFOV = Camera->FieldOfView;
	}

	const float TargetFOV = bADS ? ADSFOV : DefaultFOV;
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, InterpSpeed));
}
