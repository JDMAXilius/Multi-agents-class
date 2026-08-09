// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSBaseLayerWidget.h"
#include "Core/OSGameState.h"
#include "Data/OSHitDamageContext.h"
#include "UI/Components/OSKillFeedComponentWidget.h"
#include "OSKillFeedLayerWidget.generated.h"

/**
 * 
 */
 UENUM(BlueprintType) 
 enum class EOSKillFeedType: uint8
 {
	GLOBAL,
 	INSTIGATOR,
 	VICTIM
 };

UCLASS()
class ONSIGHT_API UOSKillFeedLayerWidget : public UOSBaseLayerWidget
{
	GENERATED_BODY()
public:
	virtual void NativeDestruct() override;
	virtual void RebuildFromGameState() override;
protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UOSKillFeedComponentWidget> KillFeedComponentClass;
	
	UPROPERTY(EditDefaultsOnly)
	FLinearColor WinColor = FLinearColor::Green;
	UPROPERTY(EditDefaultsOnly)
	FLinearColor NeutralColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly)
	FLinearColor LoseColor = FLinearColor::Red;
	UPROPERTY(EditDefaultsOnly)
	int32 TruncateAt = 12;
	UPROPERTY(EditDefaultsOnly)
	int32 MaxKillFeedEntries = 5;
	UPROPERTY(EditDefaultsOnly)
	int32 MaxPersonalFeedEntries = 3;
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnKillFeedHook(const FOSKillfeedEntry& KillfeedEntry);
	UFUNCTION(BlueprintImplementableEvent)
	void OnPlayerDeathHook(const FOSDeathEventInfo& DeathEventInfo, EOSKillFeedType Type);
	
private:
	UFUNCTION()
	void OnKillFeedEntry(const FOSKillfeedEntry& KillfeedEntry);
	UFUNCTION()
	void OnPlayerDeath(const FOSDeathEventInfo& DeathEventInfo);
	UPROPERTY(meta=(BindWidgetOptional))
	class UVerticalBox* MainKillFeed;
	
	UPROPERTY(meta=(BindWidgetOptional))
	UVerticalBox* PersonalKillFeed;
};
