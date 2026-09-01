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
	// THE EARLY FREEZE, and it has to happen on the CANDIDATE, not only on the derived
	// view. Selection now rebuilds bSightCurrent from the candidate every pump, so
	// clearing the sensorium's copy alone would be undone on the very next tick and the
	// juke window would quietly become a wall-track again. Clearing it here is also
	// strictly better than before: an enemy the bot is NOT currently fighting stops being
	// "in sight" at note time too, so it cannot win a selection on a visibility it has
	// already lost.
	if (FAIBTargetCandidate* Candidate = FindCandidate(Who))
	{
		Candidate->bSightCurrent = false;
		Candidate->bSightPending = true;   // still ours until the loss matures
		Candidate->LastKnownLocation = LastSeenAt;
	}
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

FAIBTargetCandidate* FAIBSensorium::FindCandidate(const AActor* Who)
{
	if (!Who)
	{
		return nullptr;
	}
	return Candidates.FindByPredicate([Who](const FAIBTargetCandidate& C)
	{
		return C.Actor.Get() == Who;
	});
}

FAIBTargetCandidate* FAIBSensorium::FindOrAddCandidate(AActor* Who)
{
	if (!Who)
	{
		return nullptr; // a blast has no identity; there is nobody to fight
	}
	if (FAIBTargetCandidate* Existing = FindCandidate(Who))
	{
		return Existing;
	}
	FAIBTargetCandidate& Added = Candidates.AddDefaulted_GetRef();
	Added.Actor = Who;
	return &Added;
}

void FAIBSensorium::PruneCandidates(double NowSeconds)
{
	// Belief expires. Without this the table is a leak AND a cheat: an enemy killed
	// twenty seconds ago would keep scoring a proximity term off a corpse's last spot.
	// Anything in sight is kept whatever its stamps say; anything else must have been
	// perceived — seen, heard or felt — within the tier's window, hard-capped at F5's
	// module ceiling so no tier can author itself an infinite memory.
	const double Window = FMath::Min(static_cast<double>(MemoryWindowSeconds),
		static_cast<double>(AIB::MaxMemorySeconds));
	Candidates.RemoveAll([NowSeconds, Window](const FAIBTargetCandidate& C)
	{
		if (!C.Actor.IsValid())
		{
			return true;
		}
		if (C.bSightCurrent)
		{
			return false;
		}
		const double Newest = FMath::Max(C.LastSeenAtSeconds, C.LastDamagedMeAtSeconds);
		return Newest < 0.0 || (NowSeconds - Newest) > Window;
	});
}

void FAIBSensorium::SelectTarget(double NowSeconds)
{
	// THE ONE PLACE the bot decides who it is fighting (founder, 1 Sep). Runs once per
	// pump, after every stimulus for this tick — so the decision is made on a settled
	// view rather than re-made by whichever sighting happened to mature last, which is
	// exactly the bug.
	// ONLY THE ELIGIBLE compete for the slot — see FAIBTargetCandidate::IsEligible. The
	// rest are leads, not enemies being fought, and promoting one would silently redefine
	// "I have a target" and delete the Search ambition.
	TArray<FAIBTargetScoreInput> Inputs;
	TArray<int32> Eligible;
	Inputs.Reserve(Candidates.Num());
	Eligible.Reserve(Candidates.Num());
	int32 Incumbent = INDEX_NONE;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FAIBTargetCandidate& C = Candidates[i];
		if (!C.IsEligible(NowSeconds))
		{
			continue;
		}
		if (VisibleTarget.IsValid() && C.Actor.Get() == VisibleTarget.Get())
		{
			Incumbent = Eligible.Num();
		}
		Eligible.Add(i);
		FAIBTargetScoreInput In;
		In.bSightCurrent = C.bSightCurrent;
		In.DistanceUU = bSelfLocationKnown
			? static_cast<float>(FVector::Dist(SelfLocation, C.LastKnownLocation))
			: -1.f;
		In.SecondsSinceSeen = C.LastSeenAtSeconds >= 0.0
			? static_cast<float>(NowSeconds - C.LastSeenAtSeconds) : -1.f;
		In.SecondsSinceDamagedMe = C.LastDamagedMeAtSeconds >= 0.0
			? static_cast<float>(NowSeconds - C.LastDamagedMeAtSeconds) : -1.f;
		Inputs.Add(In);
	}

	const int32 Winner = FAIBTargetPolicy::Choose(Inputs, Incumbent, MemoryWindowSeconds);
	if (Winner == INDEX_NONE)
	{
		// Nobody eligible: the bot has no target. Unchanged from before selection existed
		// — a matured loss with nothing else live still blanks the slot, which is what
		// hands the branch over to Search.
		VisibleTarget = nullptr;
		bSightCurrent = false;
		return;
	}
	const FAIBTargetCandidate& Chosen = Candidates[Eligible[Winner]];
	VisibleTarget = Chosen.Actor;
	VisibleTargetLastSeen = Chosen.LastKnownLocation;
	bSightCurrent = Chosen.bSightCurrent;
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
			FAIBTargetCandidate* Candidate = FindOrAddCandidate(Stimulus.Source.Get());
			if (const double* NotedLoss = Stimulus.Source.IsValid()
				? NotedLossEvents.Find(FObjectKey(Stimulus.Source.Get())) : nullptr)
			{
				if (*NotedLoss >= Stimulus.EventSeconds)
				{
					// Superseded: a belief, never a live sight. The candidate still
					// learns WHERE and WHEN — that is what the bot honestly knows — but
					// bSightCurrent stays false, so it can be selected only as a
					// remembered enemy and the wall-track stays closed.
					if (Candidate)
					{
						Candidate->LastSeenAtSeconds = FMath::Max(Candidate->LastSeenAtSeconds, Stimulus.EventSeconds);
						Candidate->LastKnownLocation = Stimulus.Location;
					}
					TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, Stimulus.EventSeconds);
					break;
				}
			}
			if (Candidate)
			{
				Candidate->bSightCurrent = true;
				Candidate->bSightPending = false;
				Candidate->LastKnownLocation = Stimulus.Location;
				Candidate->LastSeenAtSeconds = FMath::Max(Candidate->LastSeenAtSeconds, Stimulus.EventSeconds);
				Candidate->LastAppliedGainEventSeconds = Stimulus.EventSeconds;
			}
			// The honest instrument: happened -> surfaced, pump quantisation included,
			// recorded HERE so no unrelated stimulus can overwrite it (F1-B/F1-C). It
			// measures the SIGHT, not the selection, so it is recorded whether or not
			// this enemy ends up being the one the bot chooses to fight.
			LastAcquisitionLatency = static_cast<float>(NowSeconds - Stimulus.EventSeconds);
			TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, Stimulus.EventSeconds);
			break;
		}

		case EAIBStimulusKind::SightLost:
		{
			// A loss OLDER than the last applied gain is stale — the enemy re-peeked and
			// we re-acquired since. Applying it would blind the bot to someone standing
			// in the open, permanently, on ~half of corner peeks (F-3.1). The gain stamp
			// is now PER CANDIDATE, which is strictly more correct: one global number
			// meant two enemies peeking in turn could make each other's losses look
			// stale.
			//
			// And it applies to ANY candidate, not only the selected one. Losing sight of
			// an enemy the bot is not currently fighting is still losing sight of them —
			// under the old single-slot design there was nowhere to record that, so an
			// unselected enemy stayed "visible" forever in the only sense that existed.
			FAIBTargetCandidate* Candidate = FindCandidate(Stimulus.Source.Get());
			if (Candidate && Stimulus.EventSeconds >= Candidate->LastAppliedGainEventSeconds)
			{
				Candidate->bSightCurrent = false;
				Candidate->bSightPending = false;   // the juke window closed: they are gone
				Candidate->LastKnownLocation = Stimulus.Location;
				Candidate->LastSeenAtSeconds = FMath::Max(Candidate->LastSeenAtSeconds, Stimulus.EventSeconds);
				if (VisibleTarget == Stimulus.Source)
				{
					// Unchanged for the held target: the last seen spot becomes the
					// memory the search will walk to.
					TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, Stimulus.EventSeconds);
				}
			}
			break;
		}

		case EAIBStimulusKind::Sound:
		case EAIBStimulusKind::Damage:
		{
			// Ears and pain PLACE, they do not SEE. Both give the candidate a position
			// and a time — that is genuinely what the bot knows — but neither sets
			// bSightCurrent, so an enemy known only by ear or by being shot can be
			// SELECTED, and is then scored and aimed at as a belief, never as a live
			// track. The damage point is already bearing-capped at the sight envelope by
			// the controller before it ever reaches here, so a sniper across the map
			// hands over a direction, not a perch.
			FAIBTargetCandidate* Candidate = FindOrAddCandidate(Stimulus.Source.Get());
			if (Candidate)
			{
				// Only if this is NEWER than what sight already knows: a hit must not
				// drag a currently visible enemy's belief backwards to the muzzle flash.
				if (Stimulus.EventSeconds > Candidate->LastSeenAtSeconds)
				{
					Candidate->LastKnownLocation = Stimulus.Location;
					Candidate->LastSeenAtSeconds = Stimulus.EventSeconds;
				}
				if (Stimulus.Kind == EAIBStimulusKind::Damage)
				{
					// THE FOUNDER'S ASK, and the whole of it: "it's not knowing who is
					// shooting him". This one stamp is what lets the selection policy
					// weigh an attacker at all. It does not lock on — it makes the
					// shooter a strong CANDIDATE, which a visible enemy at knife range
					// still outscores.
					Candidate->LastDamagedMeAtSeconds =
						FMath::Max(Candidate->LastDamagedMeAtSeconds, Stimulus.EventSeconds);
				}
			}
			// TargetMemory's own eviction rule is UNCHANGED (F-2.3): while a target is
			// held, an unrelated footstep may not overwrite the memory of the enemy.
			// Selection is a separate question now and does not touch this.
			if (!VisibleTarget.IsValid() || VisibleTarget == Stimulus.Source)
			{
				TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, Stimulus.EventSeconds);
			}
			break;
		}

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

	// THE BELIEF RULE (W-REVIEW P2 H1, ruling 2): a visible enemy's position is a tracked
	// belief re-sampled ONCE per pump, at sensorium cadence, at this one site — never a
	// live read at think rate by downstream code. While a loss is pending the belief
	// FREEZES at the last seen spot: no tracking through the pillar.
	//
	// It now runs over every CURRENTLY VISIBLE candidate rather than the single held one,
	// because selection compares distances and a challenger frozen at where it was first
	// spotted would be scored on a stale position. Same cadence, same one site, same rule
	// — just applied to everyone the bot can actually see.
	for (FAIBTargetCandidate& Candidate : Candidates)
	{
		if (Candidate.bSightCurrent && Candidate.Actor.IsValid())
		{
			Candidate.LastKnownLocation = Candidate.Actor->GetActorLocation();
		}
	}

	// Forget the gone and the long stale, THEN decide who to fight. Both after every
	// stimulus for this tick, so the choice is made on a settled view.
	PruneCandidates(NowSeconds);
	SelectTarget(NowSeconds);

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
	Candidates.Reset();
	bSelfLocationKnown = false;   // absolute stamps and positions die with the body
	NotedLossEvents.Reset();
	LiveBlasts.Reset();
	LastAcquisitionLatency = -1.f;
}
