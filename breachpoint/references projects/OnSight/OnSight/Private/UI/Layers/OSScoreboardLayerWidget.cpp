// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSScoreboardLayerWidget.h"
#include "Core/OSGameState.h"
#include "Core/OSPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "UI/Components/OSScoreboardComponent.h"

void UOSScoreboardLayerWidget::NativeDestruct()
{
	if (_gameState)
		_gameState->OnPlayerArrayChanged.RemoveAll(this);

	Super::NativeDestruct();
}

void UOSScoreboardLayerWidget::RebuildFromGameState()
{
	if (!ScoreboardComponent) return;
	if (!_gameState) return;

	_gameState->OnPlayerArrayChanged.AddUObject(this, &UOSScoreboardLayerWidget::UpdateScoreboard);

	TSet<TObjectPtr<AOSPlayerState>> Current;
	Current.Reserve(_gameState->PlayerArray.Num());

	for (auto PSBase : _gameState->PlayerArray)
	{
		auto* PS = Cast<AOSPlayerState>(PSBase);
		if (!IsValid(PS)) continue;
		Current.Add(PS);
		ScoreboardComponent->AddRow(PS);
	}

	ScoreboardComponent->RemoveRowsNotIn(Current);
}

void UOSScoreboardLayerWidget::UpdateScoreboard(AOSPlayerState* PlayerState, bool ToAdd)
{
	if (!ScoreboardComponent) return;
	if (ToAdd)
		ScoreboardComponent->AddRow(PlayerState);
	else
		ScoreboardComponent->RemoveRow(PlayerState);
}