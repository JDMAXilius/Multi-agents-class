// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Widgets/Layout/Anchors.h"
#include "OSLayerLayoutInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UOSLayerLayoutInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
USTRUCT(BlueprintType)
struct FOSLayerLayout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FAnchors Anchors = FAnchors(0,0,1,1);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Alignment = FVector2D(0,0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FMargin Offsets = FMargin(0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAutoSize = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ZOrder = 0;
};

class ONSIGHT_API IOSLayerLayoutInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	FOSLayerLayout GetLayerLayout() const;
};
