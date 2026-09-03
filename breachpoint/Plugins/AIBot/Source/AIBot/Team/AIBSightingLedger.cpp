#include "Team/AIBSightingLedger.h"

void FAIBSightingLedger::Publish(FObjectKey Reporter, const AActor* ReporterPawn, const FString& ReporterName,
	AActor* Target, const FVector& Where, double SeenAtSeconds, double Now)
{
	if (!Target)
	{
		return;
	}
	FAIBSighting* Entry = Reports.FindByPredicate([&](const FAIBSighting& R)
	{
		return R.Reporter == Reporter && R.Target.Get() == Target;
	});
	if (!Entry)
	{
		Entry = &Reports.AddDefaulted_GetRef();
		Entry->Reporter = Reporter;
		Entry->Target = Target;
	}
	Entry->ReporterPawn = ReporterPawn;
	Entry->ReporterName = ReporterName;
	Entry->Where = Where;
	Entry->SeenAtSeconds = SeenAtSeconds;
	Entry->PublishedAtSeconds = Now;
}

void FAIBSightingLedger::ForEachReport(FObjectKey Asker, const AActor* AskerPawn, double Now, float StaleSeconds,
	FAreAllies AreAllies, TFunctionRef<void(const FAIBSighting&)> Visit) const
{
	// ponytail: O(n²) over a ledger of at most (bots × targets) entries — 16 in a 4v4.
	TArray<const FAIBSighting*> Best;
	for (const FAIBSighting& R : Reports)
	{
		if (R.Reporter == Asker || !R.Target.IsValid() || Now - R.PublishedAtSeconds > StaleSeconds
			|| !AreAllies(AskerPawn, R.ReporterPawn.Get()))
		{
			continue;
		}
		const FAIBSighting** Slot = Best.FindByPredicate([&](const FAIBSighting* B) { return B->Target == R.Target; });
		if (!Slot)
		{
			Best.Add(&R);
		}
		else if (R.SeenAtSeconds > (*Slot)->SeenAtSeconds)
		{
			*Slot = &R;
		}
	}
	for (const FAIBSighting* R : Best)
	{
		Visit(*R);
	}
}

void FAIBSightingLedger::Prune(double Now, float StaleSeconds)
{
	Reports.RemoveAll([&](const FAIBSighting& R)
	{
		return !R.Target.IsValid() || Now - R.PublishedAtSeconds > StaleSeconds;
	});
}
