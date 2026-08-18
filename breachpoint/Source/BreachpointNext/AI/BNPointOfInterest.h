#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNPointOfInterest.generated.h"

/** A named spot a roaming bot walks to. Server-placed level actor read only by server-side AI,
 *  so it neither ticks nor replicates. EQS later replaces the PICK among these, never this actor. */
UCLASS()
class BREACHPOINTNEXT_API ABNPointOfInterest : public AActor
{
	GENERATED_BODY()

public:
	ABNPointOfInterest();

	UPROPERTY(EditAnywhere, Category = "BN")
	FName PointName;

	UPROPERTY(EditAnywhere, Category = "BN")
	float Radius = 200.f;
};
