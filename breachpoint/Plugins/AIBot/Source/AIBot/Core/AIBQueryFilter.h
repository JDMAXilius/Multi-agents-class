#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AIBQueryFilter.generated.h"

/** Phase 14 — THE ONE FILTER CLASS (W-AUDIT, adopted). AAIBBotController names it as
 *  DefaultNavigationFilterClass, so the mover door AND every raw MoveToLocation carry it
 *  with no per-site plumbing. bInstantiateForQuerier: the engine builds a fresh filter per
 *  query with the controller as Querier, and InitializeFilter installs THAT bot's lane
 *  costs (per bot, never per query — the draw is per life; see FAIBRouteBias). Area costs
 *  only: never SetExcludedArea / SetIncludeFlags, which turn a preference into a partial-
 *  path storm hiding as "bad AI". Link areas stay NavArea_Default — egress is never priced. */
UCLASS()
class AIBOT_API UAIBQueryFilter : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	UAIBQueryFilter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const override;
};
