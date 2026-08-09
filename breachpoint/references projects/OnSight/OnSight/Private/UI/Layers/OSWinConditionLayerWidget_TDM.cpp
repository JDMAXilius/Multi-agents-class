// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSWinConditionLayerWidget_TDM.h"

void UOSWinConditionLayerWidget_TDM::NativeDestruct()
{
	if (_gameState)
	{
		_gameState->OnWinConditionChanged.RemoveAll(this);
		_gameState->OnTeamStatsChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UOSWinConditionLayerWidget_TDM::RebuildFromGameState()
{
	if (!_gameState) return;

	_gameState->OnWinConditionChanged.RemoveAll(this);
	_gameState->OnWinConditionChanged.AddDynamic(this, &UOSWinConditionLayerWidget_TDM::UpdateWinConditionInfo);

	_gameState->OnTeamStatsChanged.RemoveAll(this);
	_gameState->OnTeamStatsChanged.AddDynamic(this, &UOSWinConditionLayerWidget_TDM::UpdateStatsTeamInfo);

	UpdateWinConditionInfo(_gameState->WinConditionEntry);
}

int32 UOSWinConditionLayerWidget_TDM::CalculateScore( const FOSWinConditionEntry& entry, int32 TeamIndex )
{
	auto ps = GetOSPlayerState();
	if (!_gameState) return -1;
	switch (entry.StatType)
	{
	case FOSWinConditionType::KILLS:
		return _gameState->GetTeamKills(TeamIndex);
	default:
		return -2;
	}
}

void UOSWinConditionLayerWidget_TDM::UpdateWinConditionInfo( const FOSWinConditionEntry& entry )
{
	if (!_gameState) return;
	for (int i = 0; i <_gameState->TeamKills.Num(); i++)
	{
		UpdateStatsTeamInfo(i);
	}
}

void UOSWinConditionLayerWidget_TDM::UpdateStatsTeamInfo(int32 TeamIndex)
{
	if (!_gameState) return;
	bool bInSameTeam = IsInSameTeamByIndex(TeamIndex);
	Update(_gameState->WinConditionEntry, bInSameTeam, TeamIndex);
}

bool UOSWinConditionLayerWidget_TDM::IsInSameTeamByIndex( int32 TeamIndex )
{
	if (auto ps = GetOSPlayerState())
	{
		return ps->GetTeamIndex() == TeamIndex;
	}
	return false;
}
