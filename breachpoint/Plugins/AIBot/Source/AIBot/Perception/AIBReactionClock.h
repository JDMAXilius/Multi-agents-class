#pragma once

#include "CoreMinimal.h"

class AActor;

/** What kind of thing was noticed. One clock serves every sense — that IS the law
 *  (FAIRPLAY F2): explosives and sounds get no faster lane than sightings. */
enum class EAIBStimulusKind : uint8
{
	SightGained,
	SightLost,
	Sound,
	Damage,
	IncomingBlast
};

/** One noticed thing, waiting to be reacted to. */
struct AIBOT_API FAIBStimulus
{
	EAIBStimulusKind Kind = EAIBStimulusKind::SightGained;
	TWeakObjectPtr<AActor> Source;
	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;                 // blast radius; unused otherwise
	double EventSeconds = -1.0;         // when it HAPPENED. A caller that knows the true
	                                    // event time may set it (>=0) before Push; left
	                                    // negative, Push stamps the push time. Latency is
	                                    // measured from the push either way — backdating
	                                    // can label, never accelerate (W-REVIEW F-1.1).
	double MatureAtSeconds = 0.0;       // when the brain may know
	double PayloadSeconds = -1.0;       // blast detonation time. Riding the stimulus keeps
	                                    // two PENDING grenades from corrupting each other;
	                                    // the post-maturity multi-blast case is the
	                                    // sensorium's live-blast list, not this field.
};

/**
 * The single point stimuli mature (FAIRPLAY F1/F2). Worldless: time comes in as a
 * parameter, actors ride as opaque weak pointers, so the whole class runs headless.
 *
 * Push() clamps the drawn latency to AIB::MinReactionSeconds AT THIS ONE SITE — a law
 * enforced in one place instead of remembered at N call sites (the R11 config breach is
 * why). One draw per stimulus, at push: nothing downstream can re-roll a reaction.
 *
 * Bounded at AIB::MaxPendingStimuli, drop-oldest: an unpossessed bot must not grow an
 * unbounded backlog with O(n) inserts for the rest of a match.
 */
class AIBOT_API FAIBReactionClock
{
public:
	/** Enqueue. DrawnLatencySeconds is clamped up to the floor, never down. */
	void Push(const FAIBStimulus& Stimulus, double NowSeconds, float DrawnLatencySeconds);

	/** The ONLY exit: OutMatured is RESET, then filled with everything matured by Now,
	 *  oldest maturity first (FIFO-stable on ties), and those entries are removed. */
	void PopMatured(double NowSeconds, TArray<FAIBStimulus>& OutMatured);

	int32 NumPending() const { return Pending.Num(); }
	void Reset() { Pending.Reset(); }

private:
	TArray<FAIBStimulus> Pending;   // kept sorted by MatureAtSeconds on insert
};
