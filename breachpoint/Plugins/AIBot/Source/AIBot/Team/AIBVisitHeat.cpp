#include "Team/AIBVisitHeat.h"

FIntVector FAIBVisitHeat::CellOf(const FVector& Where, float CellUU)
{
	const float Pitch = FMath::Max(CellUU, 1.f);
	return FIntVector(FMath::FloorToInt(Where.X / Pitch), FMath::FloorToInt(Where.Y / Pitch), FMath::FloorToInt(Where.Z / Pitch));
}

void FAIBVisitHeat::Stamp(FObjectKey Visitor, const AActor* VisitorPawn, const FVector& Where, double Now, float CellUU)
{
	TArray<FStamp>& Stamps = Cells.FindOrAdd(CellOf(Where, CellUU));
	FStamp* Mine = Stamps.FindByPredicate([&](const FStamp& S) { return S.Visitor == Visitor; });
	if (!Mine)
	{
		Mine = &Stamps.AddDefaulted_GetRef();
		Mine->Visitor = Visitor;
	}
	Mine->VisitorPawn = VisitorPawn;
	Mine->AtSeconds = Now;
}

float FAIBVisitHeat::HeatAt(FObjectKey Asker, const AActor* AskerPawn, const FVector& Where, double Now,
	float CellUU, float DecaySeconds, FAreAllies AreAllies) const
{
	const TArray<FStamp>* Stamps = Cells.Find(CellOf(Where, CellUU));
	if (!Stamps)
	{
		return 0.f;
	}
	double Freshest = -1.0;
	for (const FStamp& S : *Stamps)
	{
		if ((S.Visitor == Asker || AreAllies(AskerPawn, S.VisitorPawn.Get())) && S.AtSeconds > Freshest)
		{
			Freshest = S.AtSeconds;
		}
	}
	if (Freshest < 0.0)
	{
		return 0.f;
	}
	const double Age = FMath::Max(Now - Freshest, 0.0);
	return static_cast<float>(FMath::Exp(-Age / FMath::Max(static_cast<double>(DecaySeconds), 0.01)));
}

void FAIBVisitHeat::Prune(double Now, float DecaySeconds)
{
	const double Horizon = Now - 4.0 * FMath::Max(static_cast<double>(DecaySeconds), 0.01);
	for (auto It = Cells.CreateIterator(); It; ++It)
	{
		It->Value.RemoveAll([Horizon](const FStamp& S) { return S.AtSeconds < Horizon; });
		if (It->Value.Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}
