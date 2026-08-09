// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSControlPointInfoWidget.h"

#include "Actors/OSControlPoint.h"
#include "Core/OSPlayerState.h"

void UOSControlPointInfoWidget::NativeDestruct()
{
	if (BoundPoint.IsValid())
		BoundPoint->OnPointProgressUpdated.RemoveAll(this);

	Super::NativeDestruct();
}

void UOSControlPointInfoWidget::BindToPoint( AOSControlPoint* Point )
{
	if (BoundPoint.IsValid())
		BoundPoint->OnPointProgressUpdated.RemoveAll(this);

	BoundPoint = Point;
	if (!Point) return;
	Point->OnPointProgressUpdated.AddDynamic(this, &UOSControlPointInfoWidget::UpdateFromPoint);
	UpdateFromPoint(Point, Uninitialized, Point->GetControlPointState());
}

void UOSControlPointInfoWidget::UpdateFromPoint( AOSControlPoint* Point, EControlPointState PreviousState, EControlPointState CurrentState )
{
	if (!Point) return;
	
	
	TextBlock->SetText(Point->GetLabeledPointNameText());
	const float Max = FMath::Max(1, Point->GetMaxCap());
	const float Pct = (float)Point->GetCaptureProgress() / Max;
	ProgressBar->SetPercent(Pct);
	
	ProgressBar->SetFillColorAndOpacity(Point->GetDisplayColor());
}
