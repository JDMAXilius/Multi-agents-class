#include "Core/AIBFactsBuilder.h"

#include "Core/AIBBotController.h"
#include "Data/AIBDataRows.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/AIBAvatarInterface.h"
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
	AActor* Visible = Sensorium.GetVisibleTarget();
	if (Visible)
	{
		Facts.bHasTarget = true;
		Facts.bTargetVisible = true;

		// The juke honesty rule (F2-B): live reads only while sight is CURRENT.
		FVector TargetLocation;
		if (Sensorium.IsSightCurrent())
		{
			TargetLocation = Visible->GetActorLocation();
			if (const IAIBAvatarInterface* Avatar = Bot.GetAvatar())
			{
				Facts.TargetHealthNorm = Avatar->GetHealthNormOf(Visible);
				Facts.bTargetAlive = Avatar->IsAliveTarget(Visible);
			}
		}
		else
		{
			TargetLocation = Sensorium.GetLastSeenLocation();
			Facts.bTargetFactsFromMemory = true;
		}

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
		const FAIBTierRow Defaults; // Phase 8 resolves the real tier
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

	// Phase 3: allies/enemies via IAIBWorldQuery; objectives via IAIBAmbitionProvider,
	// with Urgency clamped 0..1 HERE, the one site. Until then those stay at their
	// honest zeros.

	return Facts;
}
