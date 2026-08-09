// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Layers/OSBaseLayerWidget.h"
#include "OSControlPointMarkerLayerWidget.generated.h"

class AOSControlPoint;
class UOSControlPointMarkerWidget;
class UCanvasPanel;

UCLASS()
class ONSIGHT_API UOSControlPointMarkerLayerWidget : public UOSBaseLayerWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MarkerRoot;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UOSControlPointMarkerWidget> MarkerWidgetClass;

	virtual void RebuildFromGameState() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void RebuildMarkers();

	UPROPERTY()
	TArray<TObjectPtr<AOSControlPoint>> Points;

	UPROPERTY()
	TMap<TObjectPtr<AOSControlPoint>, TObjectPtr<UUserWidget>> Markers;
};
