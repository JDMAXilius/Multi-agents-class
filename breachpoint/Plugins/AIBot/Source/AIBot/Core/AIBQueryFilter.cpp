#include "Core/AIBQueryFilter.h"

#include "Core/AIBBotController.h"
#include "Core/AIBNavArea_Lane.h"
#include "Core/AIBRouteBias.h"
#include "NavigationData.h"

UAIBQueryFilter::UAIBQueryFilter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bInstantiateForQuerier = true;
}

void UAIBQueryFilter::InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const
{
	Super::InitializeFilter(NavData, Querier, Filter);
	const AAIBBotController* Bot = Cast<AAIBBotController>(Querier);
	if (!Bot)
	{
		return; // a non-bot querier paths at cost 1 everywhere
	}
	for (int32 Lane = 1; Lane <= AIB::MaxRouteLanes; ++Lane)
	{
		const int32 AreaId = NavData.GetAreaID(AIBLanes::ClassOf(Lane));
		if (AreaId != INDEX_NONE)
		{
			Filter.SetAreaCost(static_cast<uint8>(AreaId), Bot->GetRouteLaneCost(Lane));
		}
	}
}
