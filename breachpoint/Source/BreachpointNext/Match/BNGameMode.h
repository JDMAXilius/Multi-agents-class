#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/SoftObjectPath.h"
#include "BNGameMode.generated.h"

UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABNGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	UPROPERTY(Config)
	FSoftClassPath DefaultPawnClassPath;
};
