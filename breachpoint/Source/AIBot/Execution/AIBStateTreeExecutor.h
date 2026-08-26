#pragma once

#include "CoreMinimal.h"
#include "Execution/AIBExecutor.h"
#include "UObject/Object.h"
#include "AIBStateTreeExecutor.generated.h"

class AAIBBotController;
class UStateTree;
class UStateTreeAIComponent;

/**
 * IAIBExecutor over UStateTreeAIComponent — the host controller's proven shape: the
 * component is a controller SUBOBJECT (components must be born in a constructor), this
 * object drives it. One tree, one branch per ambition, gated by FAIBAmbitionGateCondition;
 * see the design block that preceded this implementation in git history.
 */
UCLASS()
class AIBOT_API UAIBStateTreeExecutor : public UObject, public IAIBExecutor
{
	GENERATED_BODY()

public:
	virtual void Start(AAIBBotController& Bot) override;
	virtual void Stop() override;

private:
	TWeakObjectPtr<UStateTreeAIComponent> Component;
};
