// Breachpoint. The authored-space lookups. No nav divination, no invented destinations.
#include "AI/BREnvQueryContexts.h"

#include "AI/BRBotController.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRBotSpace, Log, All);

namespace BRBotArenaTags
{
	const FName Landmark(TEXT("BR.Landmark"));
	const FName RocketPad(TEXT("BR.Landmark.RocketPad"));
	const FName Cover(TEXT("BR.Cover"));
	const FName Perch(TEXT("BR.Perch"));
}

namespace
{
	const ABRBotController* GetQuerierBotController(const AActor* Querier)
	{
		if (const ABRBotController* AsController = Cast<ABRBotController>(Querier))
		{
			return AsController;
		}
		if (const APawn* AsPawn = Cast<APawn>(Querier))
		{
			return Cast<ABRBotController>(AsPawn->GetController());
		}
		return nullptr;
	}
}

void BRBotArena::GatherTagged(const UWorld* World, FName Tag, TArray<AActor*>& OutActors)
{
	OutActors.Reset();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It) && It->ActorHasTag(Tag))
		{
			OutActors.Add(*It);
		}
	}

	if (OutActors.Num() == 0)
	{
	}
}

bool BRBotArena::GetRocketPadLocation(const UWorld* World, FVector& OutLocation)
{
	TArray<AActor*> Pads;
	GatherTagged(World, BRBotArenaTags::RocketPad, Pads);
	if (Pads.Num() == 0)
	{
		return false;
	}

	Pads.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });
	OutLocation = Pads[0]->GetActorLocation();
	return true;
}

float BRBotArena::GetCoverQualityAt(const APawn* Pawn)
{
	if (Pawn == nullptr)
	{
		return 0.f;
	}

	TArray<AActor*> CoverActors;
	GatherTagged(Pawn->GetWorld(), BRBotArenaTags::Cover, CoverActors);
	if (CoverActors.Num() == 0)
	{
		return 0.f;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();

	FVector PawnOrigin = FVector::ZeroVector;
	FVector PawnExtent = FVector::ZeroVector;
	Pawn->GetActorBounds(true, PawnOrigin, PawnExtent);
	const float PawnHeight = FMath::Max(PawnExtent.Z * 2.f, KINDA_SMALL_NUMBER);

	float BestQuality = 0.f;
	for (const AActor* CoverActor : CoverActors)
	{
		FVector CoverOrigin = FVector::ZeroVector;
		FVector CoverExtent = FVector::ZeroVector;
		CoverActor->GetActorBounds(true, CoverOrigin, CoverExtent);

		const FBox Footprint(
			FVector(CoverOrigin.X - CoverExtent.X, CoverOrigin.Y - CoverExtent.Y, -HALF_WORLD_MAX),
			FVector(CoverOrigin.X + CoverExtent.X, CoverOrigin.Y + CoverExtent.Y, HALF_WORLD_MAX));

		if (!Footprint.IsInsideOrOn(PawnLocation))
		{
			continue;
		}

		const float CoverHeight = CoverExtent.Z * 2.f;
		BestQuality = FMath::Max(BestQuality, FMath::Clamp(CoverHeight / PawnHeight, 0.f, 1.f));
	}

	return BestQuality;
}

void UBREnvQueryContext_Threat::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const AActor* Querier = Cast<AActor>(QueryInstance.Owner.Get());
	const ABRBotController* Bot = GetQuerierBotController(Querier);
	if (Bot == nullptr)
	{
		return;
	}

	if (AActor* Target = Bot->GetCurrentTarget())
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Target);
	}
}

void UBREnvQueryContext_RocketPad::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const AActor* Querier = Cast<AActor>(QueryInstance.Owner.Get());
	if (Querier == nullptr)
	{
		return;
	}

	FVector PadLocation = FVector::ZeroVector;
	if (BRBotArena::GetRocketPadLocation(Querier->GetWorld(), PadLocation))
	{
		UEnvQueryItemType_Point::SetContextHelper(ContextData, PadLocation);
	}
}

void UBREnvQueryContext_CoverPoints::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const AActor* Querier = Cast<AActor>(QueryInstance.Owner.Get());
	if (Querier == nullptr)
	{
		return;
	}

	TArray<AActor*> CoverActors;
	BRBotArena::GatherTagged(Querier->GetWorld(), BRBotArenaTags::Cover, CoverActors);
	CoverActors.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, CoverActors);
}

void UBREnvQueryContext_GrapplePerches::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const AActor* Querier = Cast<AActor>(QueryInstance.Owner.Get());
	if (Querier == nullptr)
	{
		return;
	}

	TArray<AActor*> Perches;
	BRBotArena::GatherTagged(Querier->GetWorld(), BRBotArenaTags::Perch, Perches);
	Perches.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, Perches);
}

void UBREnvQueryContext_Teammates::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const AActor* Querier = Cast<AActor>(QueryInstance.Owner.Get());
	const ABRBotController* Bot = GetQuerierBotController(Querier);
	if (Bot == nullptr || Bot->GetWorld() == nullptr)
	{
		return;
	}

	const APawn* SelfPawn = Bot->GetPawn();

	TArray<AActor*> Teammates;
	for (TActorIterator<APawn> It(const_cast<UWorld*>(Bot->GetWorld())); It; ++It)
	{
		APawn* Other = *It;
		if (!IsValid(Other) || Other == SelfPawn)
		{
			continue;
		}
		if (Bot->GetTeamAttitudeTowards(*Other) == ETeamAttitude::Friendly)
		{
			Teammates.Add(Other);
		}
	}

	Teammates.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, Teammates);
}

void UBREnvQueryContext_StepAnchor::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const AActor* Querier = Cast<AActor>(QueryInstance.Owner.Get());
	const ABRBotController* Bot = GetQuerierBotController(Querier);
	if (Bot == nullptr)
	{
		return;
	}

	const FBRBotPlanStep Step = Bot->GetCurrentStep();

	switch (Step.Anchor)
	{
	case EBRBotAnchor::Target:
	case EBRBotAnchor::FiringPoint:
		if (AActor* Target = Bot->GetCurrentTarget())
		{
			UEnvQueryItemType_Actor::SetContextHelper(ContextData, Target);
		}
		break;

	case EBRBotAnchor::RocketPad:
	{
		FVector PadLocation = FVector::ZeroVector;
		if (BRBotArena::GetRocketPadLocation(Bot->GetWorld(), PadLocation))
		{
			UEnvQueryItemType_Point::SetContextHelper(ContextData, PadLocation);
		}
		break;
	}

	case EBRBotAnchor::Cover:
	{
		TArray<AActor*> CoverActors;
		BRBotArena::GatherTagged(Bot->GetWorld(), BRBotArenaTags::Cover, CoverActors);
		CoverActors.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, CoverActors);
		break;
	}

	case EBRBotAnchor::Perch:
	{
		TArray<AActor*> Perches;
		BRBotArena::GatherTagged(Bot->GetWorld(), BRBotArenaTags::Perch, Perches);
		Perches.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Perches);
		break;
	}

	case EBRBotAnchor::Landmark:
	{
		TArray<AActor*> Landmarks;
		BRBotArena::GatherTagged(Bot->GetWorld(), BRBotArenaTags::Landmark, Landmarks);
		Landmarks.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Landmarks);
		break;
	}

	case EBRBotAnchor::Teammate:
	{
		const UBREnvQueryContext_Teammates* TeammateContext = GetDefault<UBREnvQueryContext_Teammates>();
		TeammateContext->ProvideContext(QueryInstance, ContextData);
		break;
	}

	default:
		break;
	}
}
