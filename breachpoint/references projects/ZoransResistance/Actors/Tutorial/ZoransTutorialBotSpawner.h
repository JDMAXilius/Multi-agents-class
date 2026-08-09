// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "ZoransResistance/Teams/ZoransTeamSpawner.h"
#include "ZoransTutorialBotSpawner.generated.h"

UCLASS()
class ZORANSRESISTANCE_API AZoransTutorialBotSpawner : public AZoransTeamSpawner
{
	GENERATED_BODY()

public:
	AZoransTutorialBotSpawner(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
};
