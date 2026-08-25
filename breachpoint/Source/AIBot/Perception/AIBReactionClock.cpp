#include "Perception/AIBReactionClock.h"

#include "Core/AIBTypes.h"

void FAIBReactionClock::Push(const FAIBStimulus& Stimulus, double NowSeconds, float DrawnLatencySeconds)
{
	FAIBStimulus Queued = Stimulus;
	Queued.EventSeconds = NowSeconds;

	// FAIRPLAY F1, the one clamp. Upward only: a tier may be slower than the floor,
	// never faster through it.
	const float Latency = FMath::Max(DrawnLatencySeconds, AIB::MinReactionSeconds);
	Queued.MatureAtSeconds = NowSeconds + Latency;

	// Sorted insert keeps PopMatured a front-trim and the maturity order honest even
	// when a slow tier's early stimulus outwaits a fast draw pushed later.
	int32 Index = 0;
	while (Index < Pending.Num() && Pending[Index].MatureAtSeconds <= Queued.MatureAtSeconds)
	{
		++Index;
	}
	Pending.Insert(Queued, Index);
}

void FAIBReactionClock::PopMatured(double NowSeconds, TArray<FAIBStimulus>& OutMatured)
{
	int32 Count = 0;
	while (Count < Pending.Num() && Pending[Count].MatureAtSeconds <= NowSeconds)
	{
		++Count;
	}

	if (Count > 0)
	{
		OutMatured.Append(Pending.GetData(), Count);
		Pending.RemoveAt(0, Count);
	}
}
