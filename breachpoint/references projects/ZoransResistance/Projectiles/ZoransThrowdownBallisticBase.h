// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "ZoransProjectileBallistic.h"
#include "ZoransThrowdownBallisticBase.generated.h"

UCLASS(Abstract)
class ZORANSRESISTANCE_API AZoransThrowdownBallisticBase : public AZoransProjectileBallistic
{
	GENERATED_BODY()

public:
	AZoransThrowdownBallisticBase();

protected:
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
};
