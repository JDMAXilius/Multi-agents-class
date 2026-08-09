#pragma once

#include "CoreMinimal.h"
#include "Characters/OSCharacter.h"
#include "OSAICharacter.generated.h"

UCLASS()
class ONSIGHT_API AOSAICharacter : public AOSCharacter
{
	GENERATED_BODY()

public:
	AOSAICharacter(const FObjectInitializer& ObjectInitializer);
	virtual bool IsLocallyControlled() const override;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// ========================================
	// AI CONFIGURATION
	// ========================================
	// AI-specific properties can be added here
	// Example: Behavior Tree, Blackboard, Perception, etc.
};

