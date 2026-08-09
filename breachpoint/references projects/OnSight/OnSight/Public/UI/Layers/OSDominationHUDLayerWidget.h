// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Layers/OSBaseLayerWidget.h"
#include "OSDominationHUDLayerWidget.generated.h"

class AOSGameState_Domination;
class UOSControlPointLabelWidget;
class UOSTeamScoreWidget;

UCLASS()
class ONSIGHT_API UOSDominationHUDLayerWidget : public UOSBaseLayerWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOSControlPointLabelWidget> W_ControlPointLabel_A;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOSControlPointLabelWidget> W_ControlPointLabel_B;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOSControlPointLabelWidget> W_ControlPointLabel_C;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOSTeamScoreWidget> TeamScore;

	virtual void RebuildFromGameState() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<AOSGameState_Domination> DomGameState;

	UFUNCTION()
	void HandleScoreChanged(int32 TeamIndex);

	UFUNCTION()
	void HandlePointsChanged();
};
