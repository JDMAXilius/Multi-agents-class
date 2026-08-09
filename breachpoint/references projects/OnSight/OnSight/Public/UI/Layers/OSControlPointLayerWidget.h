// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSBaseLayerWidget.h"
#include "Core/GameStates/OSGameState_Domination.h"
#include "OSControlPointLayerWidget.generated.h"

class UOSControlPointMarkerWidget;
class UCanvasPanel;
class UOSControlPointInfoWidget;
class UVerticalBox;
/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSControlPointLayerWidget : public UOSBaseLayerWidget
{
	GENERATED_BODY()
	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UFUNCTION()
	void         HandlePointsChanged();
	virtual void RebuildFromGameState() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UVerticalBox> PointList;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCanvasPanel> WorldMarkerRoot;
	


protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UOSControlPointInfoWidget> InfoWidgetClass;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UOSControlPointMarkerWidget> MarkerWidgetClass;
	
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
private:
	UPROPERTY()
	TMap<TObjectPtr<AOSControlPoint>, TObjectPtr<UUserWidget>> Markers;

	void RebuildMarkers();
};
