#pragma once

#include "CoreMinimal.h"
#include "OSGameplayEffect.h"
#include "GE_OSTDMApplyDamage.generated.h"

/** TDM damage GE: UOSTDMDamageExecCalc (friendly-fire off, Damage meta -> AttributeSet death pipeline). */
UCLASS()
class ONSIGHT_API UGE_OSTDMApplyDamage : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSTDMApplyDamage(const FObjectInitializer& ObjectInitializer);
};
