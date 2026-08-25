#include "Perception/AIBSensorium.h"

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

void FAIBSensorium::NoteSighting(AActor* Who, const FVector& Where, double NowSeconds)
{
	Clock.Push(MakeStimulus(EAIBStimulusKind::SightGained, Who, Where), NowSeconds, DrawLatency());
}

void FAIBSensorium::NoteSightingLost(AActor* Who, const FVector& LastSeenAt, double NowSeconds)
{
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
	// The blast rides the SAME clock as every sense (F2 — the wall-dodge lesson: BN shipped
	// a bot that dodged, through a wall, a grenade it never saw, because explosives had a
	// side channel). Detonation time travels on the stimulus so maturity can't extend it.
	FAIBStimulus Stimulus = MakeStimulus(EAIBStimulusKind::IncomingBlast, nullptr, Center, Radius);
	Stimulus.PayloadSeconds = DetonateAtSeconds;
	Clock.Push(Stimulus, NowSeconds, DrawLatency());
}

void FAIBSensorium::Pump(double NowSeconds)
{
	TArray<FAIBStimulus> Matured;
	Clock.PopMatured(NowSeconds, Matured);

	for (const FAIBStimulus& Stimulus : Matured)
	{
		LastMaturedLatency = static_cast<float>(Stimulus.MatureAtSeconds - Stimulus.EventSeconds);

		switch (Stimulus.Kind)
		{
		case EAIBStimulusKind::SightGained:
			VisibleTarget = Stimulus.Source;
			TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, NowSeconds);
			break;

		case EAIBStimulusKind::SightLost:
			// Only the CURRENT target's loss blanks visibility — a stale loss for an enemy
			// we re-acquired since must not blind us to the reacquisition.
			if (VisibleTarget == Stimulus.Source)
			{
				VisibleTarget = nullptr;
			}
			TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, NowSeconds);
			break;

		case EAIBStimulusKind::Sound:
		case EAIBStimulusKind::Damage:
			// Ears and pain place, they do not SEE: memory updates, visibility does not.
			TargetMemory.Remember(Stimulus.Source.Get(), Stimulus.Location, NowSeconds);
			break;

		case EAIBStimulusKind::IncomingBlast:
			BlastCenter = Stimulus.Location;
			BlastRadius = Stimulus.Radius;
			BlastDetonateAtSeconds = Stimulus.PayloadSeconds;
			break;
		}
	}
}

bool FAIBSensorium::GetIncomingBlast(double NowSeconds, FVector& OutCenter, float& OutRadius) const
{
	if (BlastDetonateAtSeconds < 0.0 || NowSeconds >= BlastDetonateAtSeconds)
	{
		return false; // no threat, or it already went off — dodging afterwards reads as broken
	}
	OutCenter = BlastCenter;
	OutRadius = BlastRadius;
	return true;
}

void FAIBSensorium::Reset()
{
	Clock.Reset();
	TargetMemory.Forget();
	VisibleTarget = nullptr;
	BlastCenter = FVector::ZeroVector;
	BlastRadius = 0.f;
	BlastDetonateAtSeconds = -1.0;
	LastMaturedLatency = -1.f;
}
