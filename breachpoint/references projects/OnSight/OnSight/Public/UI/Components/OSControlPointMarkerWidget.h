// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Actors/OSControlPoint.h"
#include "OSControlPointMarkerWidget.generated.h"
class UTextBlock;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSControlPointMarkerWidget : public UUserWidget
{
	GENERATED_BODY()
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TextBlock;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandlePointUIChanged(AOSControlPoint* Point, EControlPointState PreviousState, EControlPointState CurrentState);
	
	UPROPERTY()
	TWeakObjectPtr<AOSControlPoint> BoundPoint;
	public:
	
	void BindToPoint(AOSControlPoint* Point);

};
