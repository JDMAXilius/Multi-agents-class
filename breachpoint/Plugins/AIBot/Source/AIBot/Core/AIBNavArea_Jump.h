#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "AIBNavArea_Jump.generated.h"

/** The nav area the blockout scripts paint on a jumpable step or gap (a NavModifierVolume
 *  with this class). UAIBPathFollowingComponent presses JUMP once when a path segment
 *  starts inside it — the only place a traversal verb fires from a path (AIB22 step 4). */
UCLASS(Config = Engine)
class AIBOT_API UAIBNavArea_Jump : public UNavArea
{
	GENERATED_BODY()

public:
	UAIBNavArea_Jump(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{
		DrawColor = FColor(255, 140, 0);
	}
};
