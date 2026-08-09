// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OSScoreboardInfoComponentWidget.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSScoreboardInfoComponentWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void BindToPlayerState(class AOSPlayerState* inPS);
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleStatsChanged(AOSPlayerState* ChangedPS);

	void UpdateFromPlayerState();
	
protected:
	UPROPERTY(meta=(BindWidget))
	class UTextBlock* PlayerName;
	
	UPROPERTY(meta=(BindWidget))
	UTextBlock* PlayerKills;
	
	UPROPERTY(meta=(BindWidget))
	UTextBlock* PlayerDeaths;
	
	UPROPERTY(meta=(BindWidget))
	UTextBlock* PlayerScore;

private:
	UPROPERTY()
	TObjectPtr<AOSPlayerState> BoundPS;


};
