// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "ZoransCharacterBase.h"
#include "ZoransNonPlayerCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNPCTeamAssignmentChangeDelegate);

UCLASS(Blueprintable)
class ZORANSRESISTANCE_API AZoransNonPlayerCharacter : public AZoransCharacterBase
{
	GENERATED_BODY()

public:

	AZoransNonPlayerCharacter(const FObjectInitializer& ObjectInitializer);
};