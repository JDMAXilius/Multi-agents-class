// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/OSUserInterfaceWidget.h"
#include "Actors/OSControlPoint.h"
#include "OSControlPointLabelWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class ONSIGHT_API UOSControlPointLabelWidget : public UOSUserInterfaceWidget
{
	GENERATED_BODY()

public:
	void BindToPoint(AOSControlPoint* Point);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CaptureProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PointLabelText;

	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TWeakObjectPtr<AOSControlPoint> BoundPoint;

	UFUNCTION()
	void HandlePointStateChanged(AOSControlPoint* Point, EControlPointState PreviousState, EControlPointState CurrentState);

	void RefreshFromPoint();
};
