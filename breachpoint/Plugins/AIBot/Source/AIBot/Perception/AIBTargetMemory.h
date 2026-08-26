#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * Last-known position with decay (FAIRPLAY F5). Worldless, single-focus — an
 * anti-omniscience choice, not a host fact: one remembered contact is the most a fair
 * memory grants, and F5's decay bounds how long even that survives.
 *
 * THE API EXPOSES POSITION AND AGE, NEVER THE ACTOR (W-REVIEW F2-A): a live actor
 * handle out of a "memory" is a wallhack — hear someone once, track their true position
 * forever. The weak pointer stays private, used only for the liveness check.
 *
 * Freshness windows come from the caller (tier data) and are clamped at ONE site to
 * AIB::MaxMemorySeconds, so FLT_MAX cannot lawfully compile into infinite memory (F5-A).
 */
class AIBOT_API FAIBTargetMemory
{
public:
	/** A null Who is refused: a memory of nobody must not read as fresh (F-2.1). */
	void Remember(AActor* Who, const FVector& Where, double EventSeconds)
	{
		if (!Who)
		{
			return;
		}
		RememberedActor = Who;
		LastKnownLocation = Where;
		RememberedAtSeconds = EventSeconds;
	}

	/** True (and the position) only while the memory is younger than the window —
	 *  clamped to the module ceiling — and the remembered actor still exists. */
	bool GetFresh(double NowSeconds, float FreshWindowSeconds, FVector& OutWhere) const;

	/** Seconds since remembered; NEGATIVE when empty OR when the remembered actor is
	 *  gone — both readers now agree that a dead memory is no memory (F5-D). */
	float AgeSeconds(double NowSeconds) const
	{
		if (RememberedAtSeconds < 0.0 || !RememberedActor.IsValid())
		{
			return -1.f;
		}
		return static_cast<float>(NowSeconds - RememberedAtSeconds);
	}

	/** Identity comparison only — never a way to reach the live actor. */
	bool Remembers(const AActor* Who) const { return RememberedActor.Get() == Who; }

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
