// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Components/OSScoreboardComponent.h"

#include "Components/VerticalBox.h"
#include "UI/Components/OSScoreboardInfoComponentWidget.h"

void UOSScoreboardComponent::AddRow(AOSPlayerState* PS)
{
	if (!PS || !ScoreboardRows || !RowWidgetClass) return;

	auto* row = CreateWidget<UOSScoreboardInfoComponentWidget>(GetOwningPlayer(), RowWidgetClass);
	if (!row) return;

	ScoreboardRows->AddChild(row);
	row->BindToPlayerState(PS);
	RowByPS.Add(PS, row);
}

void UOSScoreboardComponent::RemoveRow(AOSPlayerState* PS)
{
	if (!PS) return;

	if (auto Found = RowByPS.Find(PS))
		if (auto Row = Found->Get())
			Row->RemoveFromParent();

	RowByPS.Remove(PS);
}

void UOSScoreboardComponent::RemoveRowsNotIn(const TSet<TObjectPtr<AOSPlayerState>>& ActivePlayers)
{
	TArray<TObjectPtr<AOSPlayerState>> ToRemove;
	for (auto& Pair : RowByPS)
	{
		if (!ActivePlayers.Contains(Pair.Key))
			ToRemove.Add(Pair.Key);
	}
	for (auto PS : ToRemove)
		RemoveRow(PS);
}

void UOSScoreboardComponent::ClearRows()
{
	for (auto row : RowByPS)
		RemoveRow(row.Key);
}
