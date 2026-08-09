// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSWinConditionLayerWidget.h"

#include "Core/OSGameState.h"

void UOSWinConditionLayerWidget::Update(const FOSWinConditionEntry& entry, bool InSameTeam, int32 TeamIndex)
{
	auto ps = GetOSPlayerState();
	if (!ps) return;

	int32 score = CalculateScore(entry, TeamIndex);

	if (InSameTeam)
	{
		if (SelfWinConditionComponent) SelfWinConditionComponent->UpdateSelf(score, entry.StatTarget, entry.GetStatLabel());
	}
	else if (OtherWinConditionComponent) OtherWinConditionComponent->UpdateSelf(score, entry.StatTarget, entry.GetStatLabel());
}

void UOSWinConditionLayerWidget::NativeDestruct()
{
	if (_gameState)
		_gameState->OnWinConditionChanged.RemoveAll(this);

	if (auto ps = GetOSPlayerState())
		ps->OnStatsChanged.RemoveAll(this);

	Super::NativeDestruct();
}

void UOSWinConditionLayerWidget::RebuildFromGameState()
{
	Super::RebuildFromGameState();
	if (!_gameState) return;

	_gameState->OnWinConditionChanged.RemoveAll(this);
	_gameState->OnWinConditionChanged.AddDynamic(this, &UOSWinConditionLayerWidget::UpdateWinConditionInfo);
	UpdateWinConditionInfo(_gameState->WinConditionEntry);

	if (auto ps = GetOSPlayerState())
	{
		ps->OnStatsChanged.RemoveAll(this);
		ps->OnStatsChanged.AddUObject(this, &UOSWinConditionLayerWidget::UpdateStatsInfo);
	}
}

int32 UOSWinConditionLayerWidget::CalculateScore( const FOSWinConditionEntry& entry, int32 TeamIndex )
{
	auto ps = GetOSPlayerState();
	if (!ps) return -1;
	int32 score = -2;
	const auto& stats = ps->Stats;
	switch (entry.StatType)
	{
	case FOSWinConditionType::SCORE:
		score = stats.Score;
		break;
	case FOSWinConditionType::KILLS:
		score = stats.Kills;
		break;
	default:
		break;
	}
	return score;
}
void UOSWinConditionLayerWidget::UpdateWinConditionInfo( const FOSWinConditionEntry& entry )
{
	// update for both when a win condition changes
	Update(entry, true);
	Update(entry, false);
}	

void UOSWinConditionLayerWidget::UpdateStatsInfo( AOSPlayerState* playerState )
{
	if (!_gameState) return;
	Update(_gameState->WinConditionEntry, true);
}

bool UOSWinConditionLayerWidget::IsInSameTeam( AOSPlayerState* playerState )
{
	if (!playerState || !GetOSPlayerState()) return false;
	return GetOSPlayerState()->GetTeamIndex() == playerState->GetTeamIndex();
}