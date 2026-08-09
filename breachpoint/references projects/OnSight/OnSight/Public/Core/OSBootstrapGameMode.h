#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OSBootstrapGameMode.generated.h"

UCLASS()
class ONSIGHT_API AOSBootstrapGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOSBootstrapGameMode(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

private:
	/** Steam authentication is automatic - this function is kept for compatibility but does nothing */
	void AttemptLogin();
};

