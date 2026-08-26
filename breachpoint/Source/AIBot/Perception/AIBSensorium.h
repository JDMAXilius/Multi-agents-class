#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Perception/AIBReactionClock.h"
#include "Perception/AIBTargetMemory.h"
#include "UObject/ObjectKey.h"

/** One matured, not-yet-detonated blast. A LIST, because two live grenades are two
 *  problems: the single-slot design let a distant second grenade erase a live first one
 *  and the bot walked back into it blind (W-REVIEW F-3.2/B-1). */
struct AIBOT_API FAIBLiveBlast
{
	FVector Center = FVector::ZeroVector;
	float Radius = 0.f;
	double DetonateAtSeconds = 0.0;
};

/**
 * The fair envelope's brain-facing half (FAIRPLAY F2/F3/F5), worldless in the headless
 * sense: time is a parameter, actors are opaque weak handles. The controller owns the
 * engine's perception and forwards events as Note*() calls; this class runs the clock,
 * matures on Pump(), and holds the ONLY awareness downstream code may read.
 *
 * (That exclusivity is a LAW WITH A GREP, not a property this class can enforce: the
 * controller's inherited raw-perception accessors are public engine API — FAIRPLAY F8
 * is the quarantine and carries the check. W-REVIEW F1-A.)
 *
 * Latency: one draw per stimulus at Note-time, uniform in the tier's [Min,Max], from a
 * seedable stream — seeded PER BOT by the controller, because four bots sharing one
 * default sequence acquire in lockstep and read as coordinated omniscience (F-3.7).
 * The clock clamps every draw to AIB::MinReactionSeconds; one law, one site.
 *
 * SightLost matures like everything else — the juke window — but with three honest
 * edges: a matured loss OLDER than the last applied gain for the same actor is stale and
 * ignored (a re-peek must not blind the bot to an enemy in the open, F-3.1); the
 * moment a loss is NOTED the sight stops being "current", so consumers stop reading the
 * live actor and hold the last seen spot — a human juked around a corner aims at where
 * they THINK you are, not at your true position through the wall (F2-B); and a gain
 * whose actor already has a NOTED loss at-or-after the gain's event is SUPERSEDED — it
 * lands as memory, never as current sight (a short peek whose loss drew the faster
 * reaction must not mature into live tracking of an occluded enemy — W-REVIEW P3).
 */
class AIBOT_API FAIBSensorium
{
public:
	/** Sanitised: inverted ranges swap, NaN/negatives fall back to defaults. The floor
	 *  is not applied here — the clock owns that law. */
	void Configure(float InReactionSecondsMin, float InReactionSecondsMax);

	/** Controller seeds per bot at possession; specs seed for determinism. */
	void SetRandomSeed(int32 Seed) { Random.Initialize(Seed); }

	// -- events in (the controller's perception delegates call these) --------------
	void NoteSighting(AActor* Who, const FVector& Where, double NowSeconds);
	void NoteSightingLost(AActor* Who, const FVector& LastSeenAt, double NowSeconds);
	void NoteSound(AActor* Who, const FVector& Where, double NowSeconds);
	void NoteDamageFrom(AActor* Who, const FVector& Where, double NowSeconds);
	void NoteIncomingBlast(const FVector& Center, float Radius, double DetonateAtSeconds, double NowSeconds);
	/** Engine perception aged the actor out entirely: a loss at the last seen spot. */
	void NoteForgotten(AActor* Who, double NowSeconds);

	/** Mature the queue. Called from the controller's think timer — never a tick. */
	void Pump(double NowSeconds);

	// -- matured awareness out (the whole of what downstream may know) --------------
	AActor* GetVisibleTarget() const { return VisibleTarget.Get(); }
	bool HasVisibleTarget() const { return VisibleTarget.IsValid(); }

	/** False while a loss is pending or matured for the visible target: consumers must
	 *  then use GetLastSeenLocation, never the live actor position (F2-B). */
	bool IsSightCurrent() const { return VisibleTarget.IsValid() && bSightCurrent; }

	/** THE position fact for the visible target: a belief re-sampled once per Pump
	 *  while sight is current, frozen the moment a loss is noted. Downstream code reads
	 *  THIS, never the live actor — position flows through one site at sensorium
	 *  cadence (W-REVIEW P2 H1). The residual pre-report window (the engine's sight
	 *  sense not yet reporting an occlusion) is bounded by engine internals and is a
	 *  dated FAIRPLAY acceptance. Valid only while HasVisibleTarget(). */
	FVector GetLastSeenLocation() const { return VisibleTargetLastSeen; }

	/** The most imminent still-live blast, if any; detonated ones are pruned. */
	bool GetIncomingBlast(double NowSeconds, FVector& OutCenter, float& OutRadius) const;
	bool GetIncomingBlast(double NowSeconds, FAIBLiveBlast& OutBlast) const;

	const FAIBTargetMemory& Memory() const { return TargetMemory; }
	float MemoryAgeSeconds(double NowSeconds) const { return TargetMemory.AgeSeconds(NowSeconds); }

	/** DIAGNOSTIC ONLY (specs, debugger). Gating behaviour on the pre-maturation queue
	 *  is an F2 breach by definition — aib-critic attacks any gameplay read of this. */
	int32 NumPendingStimuli() const { return Clock.NumPending(); }

	void Reset();

	/** The HONEST instrument: seconds from the acquiring stimulus HAPPENING to the pump
	 *  that surfaced it — includes think quantisation, measured for the acquisition
	 *  specifically, never overwritten by an unrelated later stimulus (F1-B/F1-C).
	 *  Negative until the first acquisition. */
	float LastAcquisitionLatencySeconds() const { return LastAcquisitionLatency; }

private:
	float DrawLatency() { return Random.FRandRange(ReactionSecondsMin, ReactionSecondsMax); }

	float ReactionSecondsMin = 0.22f;
	float ReactionSecondsMax = 0.45f;
	FRandomStream Random;

	FAIBReactionClock Clock;
	FAIBTargetMemory TargetMemory;

	TWeakObjectPtr<AActor> VisibleTarget;
	FVector VisibleTargetLastSeen = FVector::ZeroVector;
	bool bSightCurrent = false;
	double LastAppliedGainEventSeconds = -1.0;

	/** Per-actor event time of the latest NOTED sight loss — written at Note time, so it
	 *  survives both a loss that matures before its own gain and a loss the clock's
	 *  pending cap dropped. A maturing gain checks it; pruned once per pump. */
	TMap<FObjectKey, double> NotedLossEvents;

	TArray<FAIBLiveBlast> LiveBlasts;
	float LastAcquisitionLatency = -1.f;
};
