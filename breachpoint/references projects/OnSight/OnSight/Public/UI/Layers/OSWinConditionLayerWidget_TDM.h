// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSBaseLayerWidget.h"
#include "OSWinConditionLayerWidget.h"
#include "UI/Components/OSWinConditionComponentWidget.h"
#include "OSWinConditionLayerWidget_TDM.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSWinConditionLayerWidget_TDM : public UOSWinConditionLayerWidget
{
	// We can bind the TDM layer here whenever someone's stats changed
	GENERATED_BODY()
	//virtual void RebuildFromGameState() override;

	virtual void NativeDestruct() override;
	virtual void RebuildFromGameState() override;
protected:
	virtual int32 CalculateScore(const FOSWinConditionEntry& entry, int32 TeamIndex) override;
	virtual void UpdateWinConditionInfo(const FOSWinConditionEntry& entry) override;
	
private:
	UFUNCTION()
	void UpdateStatsTeamInfo(int32 TeamIndex);
	virtual bool IsInSameTeamByIndex(int32 TeamIndex);
};
