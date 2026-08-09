// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Actors/OSControlPoint.h"
#include "OSControlPointInfoWidget.generated.h"
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSControlPointInfoWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void BindToPoint( AOSControlPoint* Point );
private:
	
	virtual void NativeDestruct() override;

	UFUNCTION()
	void UpdateFromPoint(AOSControlPoint* Point, EControlPointState PreviousState, EControlPointState CurrentState);
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TextBlock;
	
	UPROPERTY()
	TWeakObjectPtr<AOSControlPoint> BoundPoint;
};
