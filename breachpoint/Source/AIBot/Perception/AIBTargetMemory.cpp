#include "Perception/AIBTargetMemory.h"

#include "Core/AIBTypes.h"

bool FAIBTargetMemory::GetFresh(double NowSeconds, float FreshWindowSeconds, FVector& OutWhere) const
{
	if (RememberedAtSeconds < 0.0 || !RememberedActor.IsValid())
	{
		return false;
	}

	// F5's one clamp site: no caller window may exceed the module ceiling.
	const float Window = FMath::Min(FreshWindowSeconds, AIB::MaxMemorySeconds);
	if (NowSeconds - RememberedAtSeconds >= Window)
	{
		return false;
	}

	OutWhere = LastKnownLocation;
	return true;
}
