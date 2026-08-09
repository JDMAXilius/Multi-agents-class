// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LocationMagnitudeInterface.generated.h"

USTRUCT(BlueprintType)
struct FLocationMagnitudeEvent
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Location Magnitude Interface")
	AActor* SourceActor = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "Location Magnitude Interface")
	FVector WorldHitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Location Magnitude Interface")
	FVector RelativeHitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Location Magnitude Interface")
	UPrimitiveComponent* HitComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Location Magnitude Interface")
	float EventMagnitude = 0.f;
};

UINTERFACE(MinimalAPI)
class ULocationMagnitudeInterface : public UInterface
{
	GENERATED_BODY()
};


class ZORANSRESISTANCE_API ILocationMagnitudeInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Location Magnitude Interface")
	void LocationMagnitudeEvent(FLocationMagnitudeEvent LocationMagnitudeEvent);
};
