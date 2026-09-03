#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "AIBNavArea_Lane.generated.h"

/** Phase 14 — a tactical LANE. The blockout scripts paint each lane folder (culvert, tower,
 *  south lane, gallery, yard...) with one of the six concrete classes below through an
 *  AAIBLaneVolume; UAIBQueryFilter prices them per bot. DefaultCost stays 1, so every
 *  non-bot query (and a bot with no bias) paths exactly as before. Six, because Recast
 *  area ids are a uint8 budget shared with the engine's own areas; a map needs ≤6 lanes. */
UCLASS(Abstract)
class AIBOT_API UAIBNavArea_Lane : public UNavArea
{
	GENERATED_BODY()

public:
	UAIBNavArea_Lane(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{
		DrawColor = FColor(80, 200, 255);
	}

	/** 1-based lane ordinal; each concrete class sets its own (0 = not a lane). */
	int32 LaneId = 0;
};

UCLASS()
class AIBOT_API UAIBNavArea_Lane1 : public UAIBNavArea_Lane
{
	GENERATED_BODY()
public:
	UAIBNavArea_Lane1(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { LaneId = 1; }
};

UCLASS()
class AIBOT_API UAIBNavArea_Lane2 : public UAIBNavArea_Lane
{
	GENERATED_BODY()
public:
	UAIBNavArea_Lane2(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { LaneId = 2; }
};

UCLASS()
class AIBOT_API UAIBNavArea_Lane3 : public UAIBNavArea_Lane
{
	GENERATED_BODY()
public:
	UAIBNavArea_Lane3(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { LaneId = 3; }
};

UCLASS()
class AIBOT_API UAIBNavArea_Lane4 : public UAIBNavArea_Lane
{
	GENERATED_BODY()
public:
	UAIBNavArea_Lane4(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { LaneId = 4; }
};

UCLASS()
class AIBOT_API UAIBNavArea_Lane5 : public UAIBNavArea_Lane
{
	GENERATED_BODY()
public:
	UAIBNavArea_Lane5(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { LaneId = 5; }
};

UCLASS()
class AIBOT_API UAIBNavArea_Lane6 : public UAIBNavArea_Lane
{
	GENERATED_BODY()
public:
	UAIBNavArea_Lane6(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { LaneId = 6; }
};

namespace AIBLanes
{
	/** The lane class for an ordinal, null outside 1..AIB::MaxRouteLanes. */
	inline UClass* ClassOf(int32 LaneId)
	{
		switch (LaneId)
		{
		case 1: return UAIBNavArea_Lane1::StaticClass();
		case 2: return UAIBNavArea_Lane2::StaticClass();
		case 3: return UAIBNavArea_Lane3::StaticClass();
		case 4: return UAIBNavArea_Lane4::StaticClass();
		case 5: return UAIBNavArea_Lane5::StaticClass();
		case 6: return UAIBNavArea_Lane6::StaticClass();
		default: return nullptr;
		}
	}

	/** The ordinal of a nav area class, 0 when it is not a lane (default, jump, null...). */
	inline int32 LaneIdOf(const UClass* AreaClass)
	{
		return AreaClass && AreaClass->IsChildOf(UAIBNavArea_Lane::StaticClass())
			? AreaClass->GetDefaultObject<UAIBNavArea_Lane>()->LaneId : 0;
	}
}
