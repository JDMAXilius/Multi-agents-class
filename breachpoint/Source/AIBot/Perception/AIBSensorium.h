#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Perception/AIBReactionClock.h"
#include "Perception/AIBTargetMemory.h"

/**
 * The fair envelope's brain-facing half (FAIRPLAY F2/F3/F5), worldless. The controller
 * owns the engine's UAIPerceptionComponent and forwards its events here as Note*() calls;
 * this class runs the clock, matures stimuli on Pump(), and holds the ONLY awareness the
 * brain may read. Nothing else in the module touches raw perception.
 *
 * Latency: one draw per stimulus at Note-time, uniform in the tier's [Min,Max], from an
 * owned FRandomStream (seedable, so specs are deterministic). The clock clamps the draw
 * to AIB::MinReactionSeconds — the sensorium never does its own clamping, one law one site.
 *
 * SightLost matures like everything else: a bot keeps "seeing" a vanished enemy for one
 * reaction's length — which is exactly how humans get juked around corners — and the
 * matured loss converts visibility into a memory, not into instant map knowledge.
 */
class AIBOT_API FAIBSensorium
{
public:
	void Configure(float InReactionSecondsMin, float InReactionSecondsMax)
	{
		ReactionSecondsMin = InReactionSecondsMin;
		ReactionSecondsMax = InReactionSecondsMax;
	}

	/** Specs pin behaviour with a fixed seed; runtime keeps the default stream. */
	void SetRandomSeed(int32 Seed) { Random.Initialize(Seed); }

	// -- events in (the controller's perception delegates call these) -------------
	void NoteSighting(AActor* Who, const FVector& Where, double NowSeconds);
	void NoteSightingLost(AActor* Who, const FVector& LastSeenAt, double NowSeconds);
	void NoteSound(AActor* Who, const FVector& Where, double NowSeconds);
	void NoteDamageFrom(AActor* Who, const FVector& Where, double NowSeconds);
	void NoteIncomingBlast(const FVector& Center, float Radius, double DetonateAtSeconds, double NowSeconds);

	/** Mature the queue. Called from the controller's think timer — never a tick. */
	void Pump(double NowSeconds);

	// -- matured awareness out (the whole of what the brain may know) --------------
	AActor* GetVisibleTarget() const { return VisibleTarget.Get(); }
	bool HasVisibleTarget() const { return VisibleTarget.IsValid(); }

	/** True while a matured, not-yet-detonated blast threatens; false after boom. */
	bool GetIncomingBlast(double NowSeconds, FVector& OutCenter, float& OutRadius) const;

	const FAIBTargetMemory& Memory() const { return TargetMemory; }
	float MemoryAgeSeconds(double NowSeconds) const { return TargetMemory.AgeSeconds(NowSeconds); }

	int32 NumPendingStimuli() const { return Clock.NumPending(); }
	void Reset();

	/** Seconds between a stimulus happening and the brain learning of it, for the last
	 *  matured stimulus — aib-verifier's fairness sample reads this via the log line. */
	float LastMaturedLatencySeconds() const { return LastMaturedLatency; }

private:
	float DrawLatency() { return Random.FRandRange(ReactionSecondsMin, ReactionSecondsMax); }

	float ReactionSecondsMin = 0.22f;
	float ReactionSecondsMax = 0.45f;
	FRandomStream Random;

	FAIBReactionClock Clock;
	FAIBTargetMemory TargetMemory;

	TWeakObjectPtr<AActor> VisibleTarget;
	FVector BlastCenter = FVector::ZeroVector;
	float BlastRadius = 0.f;
	double BlastDetonateAtSeconds = -1.0;
	float LastMaturedLatency = -1.f;
};
