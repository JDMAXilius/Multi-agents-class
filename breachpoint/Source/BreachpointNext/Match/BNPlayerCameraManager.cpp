#include "Match/BNPlayerCameraManager.h"

ABNPlayerCameraManager::ABNPlayerCameraManager()
{
	// Epic's FP template numbers. Wider than this and the spine weights cannot accumulate to the
	// look; narrower and ADS on high/low angles feels clipped.
	ViewPitchMin = -70.f;
	ViewPitchMax = 80.f;
}
