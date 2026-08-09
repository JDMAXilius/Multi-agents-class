// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Layers/OSDominationHUDLayerWidget.h"

#include "Core/GameStates/OSGameState_Domination.h"
#include "Core/OSPlayerState.h"
#include "UI/Components/OSControlPointLabelWidget.h"
#include "UI/Components/OSTeamScoreWidget.h"

void UOSDominationHUDLayerWidget::RebuildFromGameState()
{
	Super::RebuildFromGameState();

	AOSGameState_Domination* NewGS = Cast<AOSGameState_Domination>(_gameState);
	if (!NewGS) return;

	// Bind once — if GameState changed, unbind old and bind new
	if (DomGameState != NewGS)
	{
		if (DomGameState)
		{
			DomGameState->OnTeamObjectiveScoreChanged.RemoveAll(this);
			DomGameState->OnControlPointsChanged.RemoveAll(this);
		}
		DomGameState = NewGS;
		DomGameState->OnTeamObjectiveScoreChanged.AddDynamic(this, &UOSDominationHUDLayerWidget::HandleScoreChanged);
		DomGameState->OnControlPointsChanged.AddDynamic(this, &UOSDominationHUDLayerWidget::HandlePointsChanged);
	}

	HandlePointsChanged();
	HandleScoreChanged(-1);
}

void UOSDominationHUDLayerWidget::HandleScoreChanged(int32 TeamIndex)
{
	if (!DomGameState || !TeamScore) return;

	const AOSPlayerState* PS = GetOSPlayerState();
	if (!PS) return;

	const int32 MyTeamIdx = (PS->GetTeamIndex() != INDEX_NONE) ? PS->GetTeamIndex() : 0;
	const int32 OpponentIdx = 1 - MyTeamIdx;  // 2-team game: opponent is the other index

	TeamScore->SetScores(
		DomGameState->GetTeamScore(MyTeamIdx),
		DomGameState->GetTeamScore(OpponentIdx),
		DomGameState->DomScoreLimit);
}

void UOSDominationHUDLayerWidget::HandlePointsChanged()
{
	if (!DomGameState) return;

	UOSControlPointLabelWidget* Labels[] = { W_ControlPointLabel_A, W_ControlPointLabel_B, W_ControlPointLabel_C };
	const FString SlotLabels[] = { TEXT("A"), TEXT("B"), TEXT("C") };
	const TArray<TObjectPtr<AOSControlPoint>>& Points = DomGameState->ControlPoints;

	// Match each widget slot to the point whose in-game label matches.
	// ControlPoints array is in registration order (non-deterministic), so indexing by array
	// position would bind slot A to whichever point registered first — not the A-labeled point.
	for (int32 i = 0; i < 3; ++i)
	{
		if (!Labels[i]) continue;

		AOSControlPoint* MatchingPoint = nullptr;
		for (AOSControlPoint* P : Points)
		{
			if (IsValid(P) && P->GetPointLabelText().ToString() == SlotLabels[i])
			{
				MatchingPoint = P;
				break;
			}
		}

		if (MatchingPoint)
		{
			Labels[i]->BindToPoint(MatchingPoint);
			Labels[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Labels[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Points.Num() > 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DominationHUD] %d control points but only 3 UI slots available"), Points.Num());
	}
}

void UOSDominationHUDLayerWidget::NativeDestruct()
{
	if (DomGameState)
	{
		DomGameState->OnTeamObjectiveScoreChanged.RemoveAll(this);
		DomGameState->OnControlPointsChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}
