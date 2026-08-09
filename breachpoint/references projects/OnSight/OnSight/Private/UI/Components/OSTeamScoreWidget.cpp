// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Components/OSTeamScoreWidget.h"
#include "UI/Components/OSWinConditionInfoComponentWidget.h"

void UOSTeamScoreWidget::SetScores(int32 FriendlyScore, int32 EnemyScore, int32 Target)
{
	if (W_TeamScore_Friendly) W_TeamScore_Friendly->UpdateSelf(FriendlyScore, Target);
	if (W_TeamScore_Enemy) W_TeamScore_Enemy->UpdateSelf(EnemyScore, Target);
}
