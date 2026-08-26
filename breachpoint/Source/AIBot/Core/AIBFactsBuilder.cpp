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

	// Phase 3: allies/enemies via IAIBWorldQuery; objectives via IAIBAmbitionProvider,
	// with Urgency clamped 0..1 HERE, the one site. Until then those stay at their
	// honest zeros.

	return Facts;
}
