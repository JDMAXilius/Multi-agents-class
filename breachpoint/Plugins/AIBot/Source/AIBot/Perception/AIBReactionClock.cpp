#include "Perception/AIBReactionClock.h"

#include "Core/AIBTypes.h"

void FAIBReactionClock::Push(const FAIBStimulus& Stimulus, double NowSeconds, float DrawnLatencySeconds)
{
	FAIBStimulus Queued = Stimulus;

	// Honour a caller's true event time when it supplied one; stamp otherwise. Maturity
	// is measured from NOW regardless, so a backdated event can only lengthen the
	// reported latency, never shorten the wait (F1).
	if (Queued.EventSeconds < 0.0)
	{
		Queued.EventSeconds = NowSeconds;
	}

	// FAIRPLAY F1, the one clamp. Upward only. NaN draws also land on the floor
	// (Max's comparison fails), so a poisoned tier row cannot stall the queue.
	const float Drawn = FMath::IsNaN(DrawnLatencySeconds) ? AIB::MinReactionSeconds : DrawnLatencySeconds;
	const float Latency = FMath::Max(Drawn, AIB::MinReactionSeconds);
	Queued.MatureAtSeconds = NowSeconds + Latency;

	// Drop-oldest at the cap: losing the stalest pending reaction is information LOSS,
	// which fairness permits; unbounded growth on a dead bot is not (F-1.2).
	if (Pending.Num() >= AIB::MaxPendingStimuli)
	{
		Pending.RemoveAt(0, 1, EAllowShrinking::No);
	}

	// Sorted insert keeps PopMatured a front-trim; <= keeps ties FIFO-stable.
	int32 Index = 0;
	while (Index < Pending.Num() && Pending[Index].MatureAtSeconds <= Queued.MatureAtSeconds)
	{
		++Index;
	}
	Pending.Insert(Queued, Index);
}

void FAIBReactionClock::PopMatured(double NowSeconds, TArray<FAIBStimulus>& OutMatured)
{
	OutMatured.Reset();

	int32 Count = 0;
	while (Count < Pending.Num() && Pending[Count].MatureAtSeconds <= NowSeconds)
	{
		++Count;
	}

	if (Count > 0)
	{
		OutMatured.Append(Pending.GetData(), Count);
		Pending.RemoveAt(0, Count, EAllowShrinking::No);
	}
}
