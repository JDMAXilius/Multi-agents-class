#include "Team/AIBTargetClaims.h"

#include "Core/AIBTypes.h"

namespace
{
	FAIBReleasedTargetClaim Released(const FAIBTargetClaim& Claim, EAIBTargetClaimRelease Reason)
	{
		FAIBReleasedTargetClaim Out;
		Out.ClaimantName = Claim.ClaimantName;
		Out.TargetName = Claim.TargetName;
		Out.Reason = Reason;
		return Out;
	}
}

EAIBTargetClaimResult FAIBTargetClaims::TryClaim(FObjectKey Claimant, const AActor* ClaimantPawn, const AActor* Target,
	double Now, float TtlSeconds, FAreAllies AreAllies, FIsLiveEnemy IsLiveEnemy, int32& OutHolders,
	float ClaimantPhaseDeg, const FString& ClaimantName, const FString& TargetName)
{
	OutHolders = 0;
	if (!Target || TtlSeconds <= 0.f)
	{
		return EAIBTargetClaimResult::Denied;
	}
	if (!IsLiveEnemy(ClaimantPawn, Target))
	{
		return EAIBTargetClaimResult::Dead; // F8-2: a corpse is never claimed, and never DENIED
	}

	FAIBTargetClaim* Mine = nullptr;
	int32 AlliedOthers = 0;
	for (FAIBTargetClaim& Claim : Claims)
	{
		if (!Claim.IsLive(Now) || Claim.Target.Get() != Target)
		{
			continue;
		}
		if (Claim.Claimant == Claimant)
		{
			Mine = &Claim;
		}
		else if (AreAllies(ClaimantPawn, Claim.ClaimantPawn.Get()))
		{
			++AlliedOthers;
		}
	}

	if (Mine)
	{
		Mine->ExpiresAtSeconds = Now + TtlSeconds;
		OutHolders = AlliedOthers + 1;
		return EAIBTargetClaimResult::Renewed;
	}
	if (AlliedOthers >= AIB::TargetClaimCap)
	{
		OutHolders = AlliedOthers;
		return EAIBTargetClaimResult::Denied;
	}

	FAIBTargetClaim& NewClaim = Claims.AddDefaulted_GetRef();
	NewClaim.Target = Target;
	NewClaim.Claimant = Claimant;
	NewClaim.ClaimantPawn = ClaimantPawn;
	NewClaim.GrantedAtSeconds = Now;
	NewClaim.ExpiresAtSeconds = Now + TtlSeconds;
	NewClaim.PhaseDeg = ClaimantPhaseDeg;
	NewClaim.ClaimantName = ClaimantName;
	NewClaim.TargetName = TargetName;
	OutHolders = AlliedOthers + 1;
	return EAIBTargetClaimResult::Granted;
}

int32 FAIBTargetClaims::CountAlliesOn(FObjectKey Asker, const AActor* AskerPawn, const AActor* Target,
	double Now, FAreAllies AreAllies) const
{
	int32 Count = 0;
	for (const FAIBTargetClaim& Claim : Claims)
	{
		if (Claim.IsLive(Now) && Claim.Target.Get() == Target && Claim.Claimant != Asker
			&& AreAllies(AskerPawn, Claim.ClaimantPawn.Get()))
		{
			++Count;
		}
	}
	return Count;
}

int32 FAIBTargetClaims::Ordinal(FObjectKey Asker, const AActor* AskerPawn, const AActor* Target,
	double Now, FAreAllies AreAllies) const
{
	const FAIBTargetClaim* Mine = nullptr;
	for (const FAIBTargetClaim& Claim : Claims)
	{
		if (Claim.IsLive(Now) && Claim.Target.Get() == Target && Claim.Claimant == Asker)
		{
			Mine = &Claim;
			break;
		}
	}
	if (!Mine)
	{
		return INDEX_NONE;
	}
	int32 Before = 0;
	for (const FAIBTargetClaim& Claim : Claims)
	{
		if (&Claim != Mine && Claim.IsLive(Now) && Claim.Target.Get() == Target
			&& Claim.GrantedAtSeconds < Mine->GrantedAtSeconds
			&& AreAllies(AskerPawn, Claim.ClaimantPawn.Get()))
		{
			++Before;
		}
	}
	return Before;
}

float FAIBTargetClaims::RingAngleDeg(FObjectKey Asker, const AActor* AskerPawn, const AActor* Target,
	double Now, FAreAllies AreAllies, float AskerPhaseDeg) const
{
	const int32 MyOrdinal = Ordinal(Asker, AskerPawn, Target, Now, AreAllies);
	if (MyOrdinal == INDEX_NONE)
	{
		return AskerPhaseDeg + 90.f; // denied: my own slot, not everyone's perpendicular
	}
	// The ring's base is the EARLIEST allied holder's phase (mine, when I am first).
	const FAIBTargetClaim* First = nullptr;
	for (const FAIBTargetClaim& Claim : Claims)
	{
		if (Claim.IsLive(Now) && Claim.Target.Get() == Target
			&& (Claim.Claimant == Asker || AreAllies(AskerPawn, Claim.ClaimantPawn.Get()))
			&& (!First || Claim.GrantedAtSeconds < First->GrantedAtSeconds))
		{
			First = &Claim;
		}
	}
	return (First ? First->PhaseDeg : AskerPhaseDeg) + MyOrdinal * 180.f;
}

bool FAIBTargetClaims::Holds(FObjectKey Claimant, const AActor* Target, double Now) const
{
	return Claims.ContainsByPredicate([&](const FAIBTargetClaim& Claim)
	{
		return Claim.IsLive(Now) && Claim.Target.Get() == Target && Claim.Claimant == Claimant;
	});
}

void FAIBTargetClaims::ReleaseOthers(FObjectKey Claimant, const AActor* KeepTarget, double Now,
	TArray<FAIBReleasedTargetClaim>& OutReleased)
{
	Claims.RemoveAll([&](const FAIBTargetClaim& Claim)
	{
		if (Claim.Claimant != Claimant || Claim.Target.Get() == KeepTarget || !Claim.IsLive(Now))
		{
			return false;
		}
		OutReleased.Add(Released(Claim, EAIBTargetClaimRelease::Switch));
		return true;
	});
}

void FAIBTargetClaims::NoteAmbition(FObjectKey Claimant, bool bEngaging, double Now, float DwellSeconds,
	TArray<FAIBReleasedTargetClaim>& OutReleased)
{
	if (bEngaging)
	{
		NonEngageSince.Remove(Claimant);
		return;
	}
	const double Since = NonEngageSince.FindOrAdd(Claimant, Now);
	if (Now - Since < static_cast<double>(FMath::Max(DwellSeconds, 0.f)))
	{
		return; // a blink: the TTL is the only thing that lapses it
	}
	Claims.RemoveAll([&](const FAIBTargetClaim& Claim)
	{
		if (Claim.Claimant != Claimant || !Claim.IsLive(Now))
		{
			return false;
		}
		OutReleased.Add(Released(Claim, EAIBTargetClaimRelease::Exit));
		return true;
	});
}

void FAIBTargetClaims::ReleaseAll(FObjectKey Claimant, TArray<FAIBReleasedTargetClaim>& OutReleased)
{
	NonEngageSince.Remove(Claimant);
	Claims.RemoveAll([&](const FAIBTargetClaim& Claim)
	{
		if (Claim.Claimant != Claimant)
		{
			return false;
		}
		OutReleased.Add(Released(Claim, EAIBTargetClaimRelease::Unpossess));
		return true;
	});
}

void FAIBTargetClaims::Prune(double Now, FIsLiveEnemy IsLiveEnemy, TArray<FAIBReleasedTargetClaim>& OutReleased)
{
	Claims.RemoveAll([&](const FAIBTargetClaim& Claim)
	{
		if (Now >= Claim.ExpiresAtSeconds)
		{
			OutReleased.Add(Released(Claim, EAIBTargetClaimRelease::Ttl));
			return true;
		}
		// The 25-Aug omniscience acceptance, on the same terms: no position or vitals
		// flow, and bots fighting a corpse read as broken faster than the leak reads unfair.
		if (!Claim.Target.IsValid() || !IsLiveEnemy(Claim.ClaimantPawn.Get(), Claim.Target.Get()))
		{
			OutReleased.Add(Released(Claim, EAIBTargetClaimRelease::Death));
			return true;
		}
		return false;
	});
}

int32 FAIBTargetClaims::NumLive(double Now) const
{
	int32 Count = 0;
	for (const FAIBTargetClaim& Claim : Claims)
	{
		Count += Claim.IsLive(Now) ? 1 : 0;
	}
	return Count;
}
