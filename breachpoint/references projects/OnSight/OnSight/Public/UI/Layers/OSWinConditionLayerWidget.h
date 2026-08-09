// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSBaseLayerWidget.h"
#include "Core/OSGameState.h"
#include "UI/Components/OSWinConditionInfoComponentWidget.h"
#include "OSWinConditionLayerWidget.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSWinConditionLayerWidget : public UOSBaseLayerWidget
{
	GENERATED_BODY()
public:
	void Update( const FOSWinConditionEntry& entry, bool InSameTeam = true, int32 TeamIndex = -1 );

protected:
	virtual void NativeDestruct() override;
	virtual void RebuildFromGameState() override;
	// returns Score it calculates to display based on helpers.
	// If it returns:
	// -1: Required variables are null
	// -2: Score Type isn't implemented
	virtual int32 CalculateScore(const FOSWinConditionEntry& entry, int32 TeamIndex = -1);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UOSWinConditionInfoComponentWidget* SelfWinConditionComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UOSWinConditionInfoComponentWidget* OtherWinConditionComponent;

	UFUNCTION()
	virtual void UpdateWinConditionInfo( const FOSWinConditionEntry& entry );
	

	
private:
	UFUNCTION()
	void UpdateStatsInfo(AOSPlayerState* playerState);
	
	// TODO: Will rewrite this later. Instead of enemy team will print leader. If leader, print out the closest competitor as OtherComponent instead.
	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual bool IsInSameTeam(AOSPlayerState* playerState);
};
