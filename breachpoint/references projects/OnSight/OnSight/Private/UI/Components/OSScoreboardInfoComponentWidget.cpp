// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Components/OSScoreboardInfoComponentWidget.h"

#include "Components/TextBlock.h"
#include "Core/OSPlayerState.h"

void UOSScoreboardInfoComponentWidget::NativeDestruct()
{
	if (BoundPS)
		BoundPS->OnStatsChanged.RemoveAll(this);

	Super::NativeDestruct();
}

void UOSScoreboardInfoComponentWidget::BindToPlayerState( AOSPlayerState* inPS )
{
	if (!inPS) return;
	BoundPS = inPS;
	
	inPS->OnStatsChanged.AddUObject(this, &UOSScoreboardInfoComponentWidget::HandleStatsChanged);
	UpdateFromPlayerState();
}

void UOSScoreboardInfoComponentWidget::HandleStatsChanged( AOSPlayerState* ChangedPS )
{
	if (!BoundPS || ChangedPS != BoundPS) return;
	UpdateFromPlayerState();
}

void UOSScoreboardInfoComponentWidget::UpdateFromPlayerState()
{
	if (!BoundPS) return;

	PlayerName->SetText(FText::FromString(BoundPS->GetPlayerName()));

	const auto& S = BoundPS->Stats;
	PlayerKills->SetText(FText::AsNumber(S.Kills));
	PlayerDeaths->SetText(FText::AsNumber(S.Deaths));
	PlayerScore->SetText(FText::AsNumber(S.Score));
}