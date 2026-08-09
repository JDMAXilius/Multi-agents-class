// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Layers/OSControlPointMarkerLayerWidget.h"

#include "Actors/OSControlPoint.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/GameStates/OSGameState_Domination.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Components/OSControlPointMarkerWidget.h"

void UOSControlPointMarkerLayerWidget::RebuildFromGameState()
{
	Super::RebuildFromGameState();
	if (!GetWorld()) return;

	auto* GS = GetWorld()->GetGameState<AOSGameState_Domination>();
	if (!GS) return;

	GS->OnControlPointsChanged.RemoveDynamic(this, &UOSControlPointMarkerLayerWidget::RebuildMarkers);
	GS->OnControlPointsChanged.AddDynamic(this, &UOSControlPointMarkerLayerWidget::RebuildMarkers);

	RebuildMarkers();
}

void UOSControlPointMarkerLayerWidget::RebuildMarkers()
{
	if (!MarkerRoot || !MarkerWidgetClass) return;

	MarkerRoot->ClearChildren();
	Markers.Empty();
	Points.Empty();

	if (!GetWorld()) return;
	auto* GS = GetWorld()->GetGameState<AOSGameState_Domination>();
	if (!GS) return;

	for (AOSControlPoint* P : GS->ControlPoints)
	{
		if (!IsValid(P)) continue;

		Points.Add(P);

		UUserWidget* W = CreateWidget<UUserWidget>(GetOwningPlayer(), MarkerWidgetClass);
		if (!W) continue;

		MarkerRoot->AddChild(W);

		if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(W->Slot))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(.5f, .5f));
		}

		if (auto* Marker = Cast<UOSControlPointMarkerWidget>(W))
			Marker->BindToPoint(P);

		Markers.Add(P, W);
	}
}

void UOSControlPointMarkerLayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->PlayerCameraManager) return;

	int32 SizeX, SizeY;
	PC->GetViewportSize(SizeX, SizeY);
	const FVector2D ViewportSize((float)SizeX, (float)SizeY);

	for (auto& Pair : Markers)
	{
		AOSControlPoint* Point = Pair.Key;
		UUserWidget* Marker = Pair.Value;

		if (!IsValid(Point) || !IsValid(Marker))
			continue;

		const FVector WorldLoc = Point->GetActorLocation() + FVector(0, 0, 120.f);

		FVector2D Screen;
		const bool bOnScreen = UGameplayStatics::ProjectWorldToScreen(PC, WorldLoc, Screen, true);

		const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
		const FVector CamFwd = PC->PlayerCameraManager->GetActorForwardVector();
		const bool bInFront = FVector::DotProduct((WorldLoc - CamLoc).GetSafeNormal(), CamFwd) > 0.f;

		const bool bVisible = bOnScreen && bInFront &&
			Screen.X >= 0.f && Screen.Y >= 0.f &&
			Screen.X <= ViewportSize.X && Screen.Y <= ViewportSize.Y;

		Marker->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bVisible) continue;

		const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
		Screen /= Scale;
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
			CanvasSlot->SetPosition(Screen);
	}
}

void UOSControlPointMarkerLayerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		if (auto* GS = World->GetGameState<AOSGameState_Domination>())
			GS->OnControlPointsChanged.RemoveDynamic(this, &UOSControlPointMarkerLayerWidget::RebuildMarkers);
	}

	Super::NativeDestruct();
}
