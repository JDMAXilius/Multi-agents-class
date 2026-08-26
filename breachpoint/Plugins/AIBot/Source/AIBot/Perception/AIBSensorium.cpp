#include "Perception/AIBSensorium.h"

#include "Core/AIBTypes.h"

namespace
{
	FAIBStimulus MakeStimulus(EAIBStimulusKind Kind, AActor* Who, const FVector& Where, float Radius = 0.f)
	{
		FAIBStimulus Stimulus;
		Stimulus.Kind = Kind;
		Stimulus.Source = Who;
		Stimulus.Location = Where;
		Stimulus.Radius = Radius;
		return Stimulus;
	}
}

void FAIBSensorium::Configure(float InReactionSecondsMin, float InReactionSecondsMax)
{
	float Min = InReactionSecondsMin;
	float Max = InReactionSecondsMax;
	if (FMath::IsNaN(Min) || Min < 0.f) { Min = 0.22f; }
	if (FMath::IsNaN(Max) || Max < 0.f) { Max = 0.45f; }
	if (Min > Max) { Swap(Min, Max); }
	ReactionSecondsMin = Min;
	ReactionSecondsMax = Max;
}

void FAIBSensorium::NoteSighting(AActor* Who, const FVector& Where, double NowSeconds)
{
	Clock.Push(MakeStimulus(EAIBStimulusKind::SightGained, Who, Where), NowSeconds, DrawLatency());
}

void FAIBSensorium::NoteSightingLost(AActor* Who, const FVector& LastSeenAt, double NowSeconds)
{
	// The CURRENT target going occluded stops being trackable the moment the loss is
	// NOTED, not when it matures: freezing early only reduces information (lawful), and
	// it is what turns the juke window from a wallhack into a belief (F2-B). Visibility
	// itself still waits for maturity — the bot believes for one reaction longer.
	if (Who && VisibleTarget == Who)
	{
		bSightCurrent = false;
		VisibleTargetLastSeen = LastSeenAt;
	}
	// Recorded at NOTE time, per actor, unconditionally — the ledger a maturing gain
	// checks against. Two orderings need it (W-REVIEW P3, fairness HIGH): a loss whose
	// latency draw beats its own gain's matures into an empty VisibleTarget and would
	// vanish, and a loss dropped by the clock's pending cap never matures at all. In
	// both, the surviving gain would turn on live tracking of an occluded enemy —
	// the wall-track, reopened through ordering. The ledger closes both.
	if (Who)
	{
		double& Recorded = NotedLossEvents.FindOrAdd(FObjectKey(Who));
		Recorded = FMath::Max(Recorded, NowSeconds);
	}
	Clock.Push(MakeStimulus(EAIBStimulusKind::SightLost, Who, LastSeenAt), NowSeconds, DrawLatency());
}

void FAIBSensorium::NoteSound(AActor* Who, const FVector& Where, double NowSeconds)
{
	Clock.Push(MakeStimulus(EAIBStimulusKind::Sound, Who, Where), NowSeconds, DrawLatency());
}

void FAIBSensorium::NoteDamageFrom(AActor* Who, const FVector& Where, double NowSeconds)
{
	Clock.Push(MakeStimulus(EAIBStimulusKind::Damage, Who, Where), NowSeconds, DrawLatency());
}

void FAIBSensorium::NoteIncomingBlast(const FVector& Center, float Radius, double DetonateAtSeconds, double NowSeconds)
{
	// The blast rides the SAME clock as every sense (F2 — the wall-dodge ban).
	FAIBStimulus Stimulus = MakeStimulus(EAIBStimulusKind::IncomingBlast, nullptr, Center, Radius);

	// THE FUSE NOISE (FAIRPLAY, 26 Aug — the open ruling, closed): the stored fuse was
	// ground truth, recomputed each think, and every bot dodged at exactly the same
	// remaining-seconds mark — inhumanly consistent. ONE draw per blast, here, stored
	// in the payload: what matures is the bot's estimate, and every later ask reads
	// the same estimate (no per-think reroll — F4's law, applied to the ear). Negative
	// = "it blows sooner than it does" (dodges early, panicky and free); positive =
	// late realization, occasionally fatal — which is the whole point.
	const double FuseNoise = Random.FRandRange(-AIB::BlastFuseNoiseEarlySeconds, AIB::BlastFuseNoiseLateSeconds);
	Stimulus.PayloadSeconds = DetonateAtSeconds + FuseNoise;
	Clock.Push(Stimulus, NowSeconds, DrawLatency());
}

void FAIBSensorium::NoteForgotten(AActor* Who, double NowSeconds)
{
	// Engine perception aged the actor out with no loss event: mature a loss at the
	// last seen spot so visibility cannot outlive perception (the infinite-sight hole
	// three review passes flagged on the old no-op). Same ledger entry as a real loss.
	if (Who)
	{
		double& Recorded = NotedLossEvents.FindOrAdd(FObjectKey(Who));
		Recorded = FMath::Max(Recorded, NowSeconds);
	}
	Clock.Push(MakeStimulus(EAIBStimulusKind::SightLost, Who, VisibleTargetLastSeen), NowSeconds, DrawLatency());
}

void FAIBSensorium::Pump(double NowSeconds)
{
	TArray<FAIBStimulus> Matured;
	Clock.PopMatured(NowSeconds, Matured);

	for (const FAIBStimulus& Stimulus : Matured)
	{
		switch (Stimulus.Kind)
		{
		case EAIBStimulusKind::SightGained:
		{
			// THE SUPERSEDED-GAIN CHECK (W-REVIEW P3, fairness HIGH): a loss NOTED at or
			// after this gain's event means the world already took the sight back before
			// the reaction finished maturing — a 100ms peek whose loss drew the shorter
			// latency. Accepting the gain as current would live-track through the wall
			// for up to SightMaxAge. Net honest outcome of gain-then-loss: a memory at
			// the gained spot, no visible target, no acquisition recorded.
			if (const double* NotedLoss = Stimulus.Source.IsValid()
				? NotedLossEvents.Find(FObjectKey(Stimulus.Source.Get())) : nullptr)
			{
				if (*NotedLoss >= Stimulus.EventSeconds)
				{
					TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, Stimulus.EventSeconds);
					break;
				}
			}
			VisibleTarget = Stimulus.Source;
			VisibleTargetLastSeen = Stimulus.Location;
			bSightCurrent = true;
			LastAppliedGainEventSeconds = Stimulus.EventSeconds;
			// The honest instrument: happened -> surfaced, pump quantisation included,
			// recorded HERE so no unrelated stimulus can overwrite it (F1-B/F1-C).
			LastAcquisitionLatency = static_cast<float>(NowSeconds - Stimulus.EventSeconds);
			TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, Stimulus.EventSeconds);
			break;
		}

		case EAIBStimulusKind::SightLost:
			// A loss OLDER than the last applied gain is stale — the enemy re-peeked and
			// we re-acquired since. Applying it would blind the bot to someone standing
			// in the open, permanently, on ~half of corner peeks (F-3.1).
			if (VisibleTarget == Stimulus.Source
				&& Stimulus.EventSeconds >= LastAppliedGainEventSeconds)
			{
				VisibleTarget = nullptr;
				bSightCurrent = false;
				VisibleTargetLastSeen = Stimulus.Location;
				TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, Stimulus.EventSeconds);
			}
			break;

		case EAIBStimulusKind::Sound:
		case EAIBStimulusKind::Damage:
			// Ears and pain place, they do not SEE — and they must not EVICT: while a
			// visible target holds focus, an unrelated noise may not overwrite the
			// memory of the enemy with a footstep (F-2.3). No focus -> a sound is the
			// new lead, which is what searching by ear IS.
			if (!VisibleTarget.IsValid() || VisibleTarget == Stimulus.Source)
			{
				TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, Stimulus.EventSeconds);
			}
			break;

		case EAIBStimulusKind::IncomingBlast:
		{
			FAIBLiveBlast Blast;
			Blast.Center = Stimulus.Location;
			Blast.Radius = Stimulus.Radius;
			Blast.DetonateAtSeconds = Stimulus.PayloadSeconds;
			LiveBlasts.Add(Blast);
			break;
		}
		}
	}

	// THE BELIEF RULE (W-REVIEW P2 H1, ruling 2): the visible target's position is a
	// tracked belief re-sampled ONCE per pump, at sensorium cadence, at this one site —
	// never a live read at think rate by downstream code. While a loss is pending the
	// belief FREEZES at the last seen spot: no tracking through the pillar.
	if (VisibleTarget.IsValid() && bSightCurrent)
	{
		VisibleTargetLastSeen = VisibleTarget->GetActorLocation();
	}

	// Prune detonated blasts once per pump; the list stays tiny (grenades in flight).
	LiveBlasts.RemoveAll([NowSeconds](const FAIBLiveBlast& Blast)
	{
		return NowSeconds >= Blast.DetonateAtSeconds;
	});

	// Prune the loss ledger: a pending gain's event can be at most one max-latency old
	// when it matures, so an older loss entry can never supersede anything again.
	const double LedgerHorizon = NowSeconds - (static_cast<double>(ReactionSecondsMax) + 1.0);
	for (auto It = NotedLossEvents.CreateIterator(); It; ++It)
	{
		if (It->Value < LedgerHorizon)
		{
			It.RemoveCurrent();
		}
	}
}

bool FAIBSensorium::GetIncomingBlast(double NowSeconds, FAIBLiveBlast& OutBlast) const
{
	// Most imminent STILL-LIVE blast wins: the grenade about to go off at your feet
	// outranks the one across the map, whatever order they matured in (F-3.2/B-1).
	const FAIBLiveBlast* Best = nullptr;
	for (const FAIBLiveBlast& Blast : LiveBlasts)
	{
		if (NowSeconds < Blast.DetonateAtSeconds
			&& (!Best || Blast.DetonateAtSeconds < Best->DetonateAtSeconds))
		{
			Best = &Blast;
		}
	}

	if (!Best)
	{
		return false;
	}
	OutBlast = *Best;
	return true;
}

bool FAIBSensorium::GetIncomingBlast(double NowSeconds, FVector& OutCenter, float& OutRadius) const
{
	FAIBLiveBlast Blast;
	if (!GetIncomingBlast(NowSeconds, Blast))
	{
		return false;
	}
	OutCenter = Blast.Center;
	OutRadius = Blast.Radius;
	return true;
}

void FAIBSensorium::Reset()
{
	Clock.Reset();
	TargetMemory.Forget();
	VisibleTarget = nullptr;
	VisibleTargetLastSeen = FVector::ZeroVector;
	bSightCurrent = false;
	LastAppliedGainEventSeconds = -1.0;
	NotedLossEvents.Reset();
	LiveBlasts.Reset();
	LastAcquisitionLatency = -1.f;
}
