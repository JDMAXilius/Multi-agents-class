// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Components/OSKillFeedComponentWidget.h"

void UOSKillFeedComponentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, this, &UOSKillFeedComponentWidget::Remove, AutoDestroyDelay, false);
}

void UOSKillFeedComponentWidget::PrintFeed( const FText& text, FLinearColor color )
{
	TextBlock->SetText(text);
	TextBlock->SetColorAndOpacity(color);
}

void UOSKillFeedComponentWidget::Remove()
{
	RemoveFromParent();
}
