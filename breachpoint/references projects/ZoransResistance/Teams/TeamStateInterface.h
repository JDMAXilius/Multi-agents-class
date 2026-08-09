// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TeamStateInterface.generated.h"

class AZoransTeamStateBase;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class UTeamStateInterface : public UInterface
{
	GENERATED_BODY()
};

class ZORANSRESISTANCE_API ITeamStateInterface
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	virtual void SetTeamAssignment(int32 InIndex) = 0;

	UFUNCTION(BlueprintCallable)
	virtual int32 GetTeamAssignment() const = 0;
};