// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Components/OSControlPointMarkerWidget.h"

#include "Actors/OSControlPoint.h"
#include "Components/TextBlock.h"

void UOSControlPointMarkerWidget::HandlePointUIChanged( AOSControlPoint* Point, EControlPointState PreviousState, EControlPointState CurrentState )
{
	if (!Point || !TextBlock) return;
	TextBlock->SetText(Point->GetPointLabelText());
}

void UOSControlPointMarkerWidget::NativeDestruct()
{
	if (BoundPoint.IsValid())
		BoundPoint->OnPointProgressUpdated.RemoveAll(this);

	Super::NativeDestruct();
}

void UOSControlPointMarkerWidget::BindToPoint( AOSControlPoint* Point )
{
	if (!Point) return;

	if (BoundPoint.IsValid())
		BoundPoint->OnPointProgressUpdated.RemoveAll(this);

	BoundPoint = Point;
	Point->OnPointProgressUpdated.AddDynamic(this, &UOSControlPointMarkerWidget::HandlePointUIChanged);

	HandlePointUIChanged(Point, Uninitialized, Point->GetControlPointState());
}
