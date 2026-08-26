#include "AIBotAdapter/BNAIBWorldQuery.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIBotAdapter/BNAIBModeTags.h"
#include "Core/BNGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "Match/BNHillPoint.h"

bool UBNAIBWorldQuery::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBNAIBWorldQuery::RegisterHill(ABNHillPoint* Hill)
{
	if (Hill)
	{
		Hills.AddUnique(Hill);
	}
}

void UBNAIBWorldQuery::QueryPointsOfInterest(const AActor* Asker, float MaxDistance,
	TArray<FAIBPointOfInterest>& OutPoints) const
{
	for (const TWeakObjectPtr<ABNHillPoint>& HillPtr : Hills)
	{
		const ABNHillPoint* Hill = HillPtr.Get();
		if (!Hill)
		{
			continue;
		}
		FAIBPointOfInterest& Point = OutPoints.AddDefaulted_GetRef();
		Point.Location = Hill->GetActorLocation();
		Point.Kind = BNAIBTags::POI_Hill;
		Point.Worth = 1.f;
		// The hill's OWN radius is what "on the hill" means — the same number HillTick
		// scores with. Without it the mover invents its own, smaller, arrival test and
		// a bot banking points is told it has not arrived.
		Point.ReachRadiusUU = Hill->Radius;
		Point.Actor = const_cast<ABNHillPoint*>(Hill);
	}
}

void UBNAIBWorldQuery::QueryVisibleEnemies(const AActor* Asker, float Radius,
	TArray<AActor*>& OutEnemies) const
{
	// Empty by design — the header says why. Do not "improve" this with GetAllActorsOfClass.
	OutEnemies.Reset();
}

int32 UBNAIBWorldQuery::CountNearbyAllies(const AActor* Asker, float Radius) const
{
	return 0;
}

bool UBNAIBWorldQuery::AreEnemies(const AActor* A, const AActor* B) const
{
	if (A == B || !A || !B)
	{
		return false;
	}
	if (!A->IsA<APawn>() || !B->IsA<APawn>())
	{
		return false;
	}
	// The adapter's audited no-cast door plus the one tag that means dead — the same
	// pair of lines UBNAIBAvatarAdapter::IsAliveTarget uses. A corpse is nobody's enemy.
	const UAbilitySystemComponent* ASCA = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(A));
	const UAbilitySystemComponent* ASCB = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(B));
	const bool bAliveA = ASCA && !ASCA->HasMatchingGameplayTag(BNTags::State_Dead);
	const bool bAliveB = ASCB && !ASCB->HasMatchingGameplayTag(BNTags::State_Dead);
	return bAliveA && bAliveB;
}
