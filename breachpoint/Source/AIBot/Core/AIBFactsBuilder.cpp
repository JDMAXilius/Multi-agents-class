#include "Core/AIBFactsBuilder.h"

#include "Core/AIBBotController.h"
#include "Data/AIBDataRows.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/AIBAmbitionProvider.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "Interfaces/AIBWorldQuery.h"
#include "Perception/AIBSensorium.h"

FAIBFacts AIBFactsBuilder::Build(const AAIBBotController& Bot, double NowSeconds)
{
	FAIBFacts Facts;

	const APawn* Pawn = Bot.GetPawn();
	const FVector SelfLocation = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;

	// -- self: the avatar door, or honest unknowns ---------------------------------
	if (const IAIBAvatarInterface* Avatar = Bot.GetAvatar())
	{
		Facts.bVitalsKnown = true;
		Facts.HealthNorm = Avatar->GetHealthNorm();
		Facts.AmmoNorm = Avatar->GetAmmoNorm();
		Facts.bHasReserveAmmo = Avatar->HasReserveAmmo();
		Facts.bWeaponCanFight = Avatar->CanWeaponFight();
		Facts.GrenadeCount = Avatar->GetGrenadeCount();
		Facts.bGrounded = Avatar->IsGrounded();
	}

	// -- the target, as perceived ---------------------------------------------------
	const FAIBSensorium& Sensorium = Bot.GetSensorium();
	if (Sensorium.HasVisibleTarget())
	{
		Facts.bHasTarget = true;
		Facts.bTargetVisible = true;

		// THE BELIEF RULE (W-REVIEW P2 H1): position comes from the sensorium's
		// once-per-pump belief, NEVER a live actor read at think rate. The builder no
		// longer touches the target actor at all — F3's "the brain sees only what the
		// sensorium admits" is literally true again.
		const FVector TargetLocation = Sensorium.GetLastSeenLocation();
		Facts.bTargetFactsFromMemory = !Sensorium.IsSightCurrent();

		// Enemy vitals are NOT a perceivable live float (W-REVIEW P2 H2): no human
		// reads an exact health fraction off a silhouette at 1400uu. Until the
		// damage-I-dealt estimate lands (the FAIRPLAY ruling), target vitals stay
		// UNKNOWN and the selectors score ValueWhenUnknown.
		// bTargetVitalsKnown stays false; TargetHealthNorm/bTargetAlive stay unread.

		Facts.DistToTargetUU = static_cast<float>(FVector::Dist(SelfLocation, TargetLocation));
		Facts.HeightAdvantageUU = static_cast<float>(SelfLocation.Z - TargetLocation.Z);
	}

	// -- memory (F5): age plus the tier window, so a worldless consideration can
	//    judge freshness without ever seeing the world -------------------------------
	const float MemoryAge = Sensorium.MemoryAgeSeconds(NowSeconds);
	if (MemoryAge >= 0.f)
	{
		Facts.bHasMemory = true;
		Facts.LastKnownAgeSeconds = MemoryAge;
		static const FAIBTierRow Defaults; // Phase 8 resolves the real tier
		Facts.MemoryFreshWindowSeconds = FMath::Min(Defaults.MemoryFreshSeconds, AIB::MaxMemorySeconds);
	}

	// -- the incoming blast, relative — the dodge needs no world ---------------------
	FAIBLiveBlast Blast;
	if (Sensorium.GetIncomingBlast(NowSeconds, Blast))
	{
		Facts.bIncomingBlast = true;
		Facts.BlastSecondsToDetonation = static_cast<float>(Blast.DetonateAtSeconds - NowSeconds);
		Facts.BlastCenterRelative = Blast.Center - SelfLocation;
		Facts.BlastRadius = Blast.Radius;
	}

	// -- Phase 6: the mode's objectives, urgency clamped at THE one site ---------------
	// One fact per mode ambition, joined by tag in the engine. DistanceUU comes from the
	// nearest matching POI when the world query can name one; the position itself stays
	// out of facts on purpose (the mover asks the world query — Brain/ stays worldless).
	if (IAIBAmbitionProvider* Provider = Bot.GetAmbitionProvider())
	{
		TArray<FAIBModeAmbition> ModeAmbitions;
		Provider->GetModeAmbitions(ModeAmbitions);
		IAIBWorldQuery* Query = Bot.GetWorldQuery();
		TArray<FAIBPointOfInterest> Points;
		if (Query && ModeAmbitions.Num() > 0)
		{
			Query->QueryPointsOfInterest(Pawn, AIB::ObjectiveQueryRadiusUU, Points);
		}
		for (const FAIBModeAmbition& Mode : ModeAmbitions)
		{
			FAIBObjectiveFact& Fact = Facts.Objectives.AddDefaulted_GetRef();
			Fact.AmbitionTag = Mode.AmbitionTag;
			Fact.Urgency = ClampUrgency(Provider->GetObjectiveUrgency(Pawn, Mode.AmbitionTag));
			for (const FAIBPointOfInterest& Point : Points)
			{
				if (Point.Kind == Mode.ObjectiveKind)
				{
					const float Dist = FVector::Dist(SelfLocation, Point.Location);
					if (Fact.DistanceUU < 0.f || Dist < Fact.DistanceUU)
					{
						Fact.DistanceUU = Dist;
					}
				}
			}
		}

		// Crowd counts: allies are HUD-grade through the query; a bounded ENEMY count
		// does not exist yet (an unmatured feed was refused by the Phase-6 audit), so
		// bCrowdKnown stays FALSE and the outnumbered read stays an honest unknown —
		// never "confidently alone" (F-6.10's shape).
		if (Query)
		{
			Facts.NearbyAllies = Query->CountNearbyAllies(Pawn, AIB::ObjectiveQueryRadiusUU);
		}
	}

	return Facts;
}

float AIBFactsBuilder::ClampUrgency(float RawUrgency)
{
	// THE clamp site the contracts promise (three of them stated it in the present
	// tense before this code existed — W-AUDIT P6). Non-finite is scrubbed to 0, not
	// clamped: FMath::Clamp(NaN) yields NaN, NaN poisons every Rescore comparison, and
	// the first-registered spec silently wins the whole match (finding 4).
	if (!FMath::IsFinite(RawUrgency))
	{
		return 0.f;
	}
	return FMath::Clamp(RawUrgency, 0.f, 1.f);
}
