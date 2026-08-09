// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Components/OSWinConditionInfoComponentWidget.h"

#include <string>

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"


void UOSWinConditionInfoComponentWidget::UpdateSelf( int32 newScore, int32 targetScore, const FText& newLabel) const
{
	if (ScoreValueTextBlock) ScoreValueTextBlock->SetText(FText::AsNumber(newScore));
	if (!newLabel.IsEmpty() && ScoreNameTextBlock) ScoreNameTextBlock->SetText(newLabel);
	if (ScoreTargetTextBlock) ScoreTargetTextBlock->SetText(FText::AsNumber(targetScore));
	if (ScoreProgressBar)
	{
		if (targetScore <= 0)
			ScoreProgressBar->SetPercent(0);
		else
			ScoreProgressBar->SetPercent((float)newScore/(float)targetScore);
	}
	OnUpdateSelfHook(newScore, targetScore, newLabel);
}
