#pragma once

#include "CoreMinimal.h"

/**
 * Last-known position with decay (FAIRPLAY F5). Worldless, single-focus: BREACHPOINT
 * bots fight one enemy at a time, and a memory of the SECOND-to-last enemy is exactly
 * the omniscience F5 exists to ban. Freshness is judged at read time against a window
 * the caller supplies (tier data) — the memory itself stores only what and when.
 */
class AIBOT_API FAIBTargetMemory
{
public:
	void Remember(AActor* Who, const FVector& Where, double NowSeconds)
	{
		RememberedActor = Who;
		LastKnownLocation = Where;
		RememberedAtSeconds = NowSeconds;
	}

	/** True (and the position) only while the memory is younger than the window and the
	 *  remembered actor still exists — a despawned enemy is not worth searching for. */
	bool GetFresh(double NowSeconds, float FreshWindowSeconds, FVector& OutWhere) const
	{
		if (RememberedAtSeconds < 0.0 || !RememberedActor.IsValid())
		{
			return false;
		}
		if (NowSeconds - RememberedAtSeconds >= FreshWindowSeconds)
		{
			return false;
		}
		OutWhere = LastKnownLocation;
		return true;
	}

	/** Seconds since remembered; negative when empty. Feeds FAIBFacts::LastKnownAgeSeconds. */
	float AgeSeconds(double NowSeconds) const
	{
		return RememberedAtSeconds < 0.0 ? -1.f : static_cast<float>(NowSeconds - RememberedAtSeconds);
	}

	AActor* GetRememberedActor() const { return RememberedActor.Get(); }

	void Forget()
	{
		RememberedActor = nullptr;
		LastKnownLocation = FVector::ZeroVector;
		RememberedAtSeconds = -1.0;
	}

private:
	TWeakObjectPtr<AActor> RememberedActor;
	FVector LastKnownLocation = FVector::ZeroVector;
	double RememberedAtSeconds = -1.0;
};
