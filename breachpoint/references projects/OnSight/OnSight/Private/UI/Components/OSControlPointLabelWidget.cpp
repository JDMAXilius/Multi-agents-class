// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Components/OSControlPointLabelWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/OSPlayerState.h"

void UOSControlPointLabelWidget::BindToPoint(AOSControlPoint* Point)
{
	if (!Point) return;

	if (BoundPoint.IsValid())
		BoundPoint->OnPointProgressUpdated.RemoveAll(this);

	BoundPoint = Point;

	if (PointLabelText)
		PointLabelText->SetText(Point->GetPointLabelText());

	Point->OnPointProgressUpdated.AddDynamic(this, &UOSControlPointLabelWidget::HandlePointStateChanged);

	RefreshFromPoint();
}

void UOSControlPointLabelWidget::HandlePointStateChanged(AOSControlPoint* Point, EControlPointState PreviousState, EControlPointState CurrentState)
{
	RefreshFromPoint();
}

void UOSControlPointLabelWidget::RefreshFromPoint()
{
	if (!BoundPoint.IsValid()) return;

	const AOSPlayerState* PS = GetOSPlayerState();
	if (!PS) return;

	const FLinearColor Color = BoundPoint->GetDisplayColorForTeam(PS->GetTeamIndex());

	if (CaptureProgressBar)
	{
		CaptureProgressBar->SetPercent(BoundPoint->GetCapturePercent());
		CaptureProgressBar->SetFillColorAndOpacity(Color);
	}

	if (PointLabelText)
		PointLabelText->SetColorAndOpacity(FSlateColor(Color));
}

void UOSControlPointLabelWidget::NativeDestruct()
{
	if (BoundPoint.IsValid())
		BoundPoint->OnPointProgressUpdated.RemoveAll(this);

	Super::NativeDestruct();
}
