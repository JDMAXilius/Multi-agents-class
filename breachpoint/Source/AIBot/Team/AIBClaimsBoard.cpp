#include "Team/AIBClaimsBoard.h"

namespace
{
	/** The key's grid pitch. Coarse enough that float drift on one slot's location
	 *  cannot split it into two claims; fine enough that two distinct authored slots
	 *  (a provider spacing defensive positions) do not merge. */
	constexpr float ClaimCellUU = 100.f;

	FIntVector QuantizeToCell(const FVector& Location)
	{
		return FIntVector(
			FMath::FloorToInt(Location.X / ClaimCellUU),
			FMath::FloorToInt(Location.Y / ClaimCellUU),
			FMath::FloorToInt(Location.Z / ClaimCellUU));
	}
}

FAIBClaimKey FAIBClaimKey::From(const FAIBPointOfInterest& Target)
{
	FAIBClaimKey Key;
	Key.Actor = Target.Actor;
	Key.Kind = Target.Kind;
	Key.Cell = QuantizeToCell(Target.Location);
	return Key;
}

bool FAIBClaimKey::SameSlotAs(const FAIBClaimKey& Other) const
{
	// Actor identity first: a moving pickup stays ONE slot however its cell drifts.
	if (Actor.IsValid() && Other.Actor.IsValid())
	{
		return Actor == Other.Actor;
	}
	return Kind == Other.Kind && Cell == Other.Cell;
}

bool FAIBClaimsBoard::TryClaim(FObjectKey Claimant, const AActor* ClaimantPawn,
	const FAIBPointOfInterest& Target, double Now, float TtlSeconds,
	TFunctionRef<bool(const AActor*, const AActor*)> AreEnemies)
{
	// ZONES ARE REFUSED HERE, not at a call site someone can forget: the board's whole
	// authority is over things one agent can take, and the provider says which those are.
	if (!Target.bClaimableSlot || TtlSeconds <= 0.f)
	{
		return false;
	}

	Prune(Now);

	const FAIBClaimKey Key = FAIBClaimKey::From(Target);
	for (FAIBClaim& Claim : Claims)
	{
		if (!Claim.Key.SameSlotAs(Key))
		{
			continue;
		}
		if (Claim.Claimant == Claimant)
		{
			// Renewal — the route is still live, the hold extends. Not a fresh grant.
			Claim.ExpiresAtSeconds = Now + TtlSeconds;
			return true;
		}
		// A NON-enemy other holds it: denied. An enemy's claim does not bind — their
		// book is not ours, and honouring it would be reading enemy intent (F3) and
		// conceding contested resources across teams (collusion).
		if (!AreEnemies(ClaimantPawn, Claim.ClaimantPawn.Get()))
		{
			return false;
		}
	}

	FAIBClaim& NewClaim = Claims.AddDefaulted_GetRef();
	NewClaim.Key = Key;
	NewClaim.Claimant = Claimant;
	NewClaim.ClaimantPawn = ClaimantPawn;
	NewClaim.bPawnBound = ClaimantPawn != nullptr;
	NewClaim.ExpiresAtSeconds = Now + TtlSeconds;
	return true;
}

bool FAIBClaimsBoard::IsClaimedByOther(FObjectKey Asker, const AActor* AskerPawn,
	const FAIBPointOfInterest& Target, double Now,
	TFunctionRef<bool(const AActor*, const AActor*)> AreEnemies) const
{
	if (!Target.bClaimableSlot)
	{
		return false; // zones are never suppressed, for anyone
	}
	const FAIBClaimKey Key = FAIBClaimKey::From(Target);
	for (const FAIBClaim& Claim : Claims)
	{
		if (Claim.IsLive(Now) && Claim.Key.SameSlotAs(Key) && Claim.Claimant != Asker
			&& !AreEnemies(AskerPawn, Claim.ClaimantPawn.Get()))
		{
			return true;
		}
	}
	return false;
}

void FAIBClaimsBoard::ReleaseAll(FObjectKey Claimant)
{
	Claims.RemoveAll([&Claimant](const FAIBClaim& Claim)
	{
		return Claim.Claimant == Claimant;
	});
}

void FAIBClaimsBoard::Prune(double Now)
{
	Claims.RemoveAll([Now](const FAIBClaim& Claim)
	{
		return !Claim.IsLive(Now);
	});
}

int32 FAIBClaimsBoard::NumLive(double Now) const
{
	int32 Count = 0;
	for (const FAIBClaim& Claim : Claims)
	{
		Count += Claim.IsLive(Now) ? 1 : 0;
	}
	return Count;
}
