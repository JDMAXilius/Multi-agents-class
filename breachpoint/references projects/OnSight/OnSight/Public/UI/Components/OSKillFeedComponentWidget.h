// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "OSKillFeedComponentWidget.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSKillFeedComponentWidget : public UUserWidget
{
	GENERATED_BODY()
	virtual void NativeConstruct() override;
public:
	UPROPERTY(meta=(BindWidget))
	UTextBlock* TextBlock;
	
	FTimerHandle DestroyTimer;
	
	UPROPERTY(EditDefaultsOnly)
	float AutoDestroyDelay = 5.0f;
	
	UFUNCTION()
	void PrintFeed( const FText& text,  FLinearColor color = FLinearColor::White);
	
	UFUNCTION()
	void Remove();
};
