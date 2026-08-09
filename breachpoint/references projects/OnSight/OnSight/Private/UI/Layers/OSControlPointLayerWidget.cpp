// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSControlPointLayerWidget.h"

#include "Actors/OSControlPoint.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Core/GameStates/OSGameState_Domination.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Components/OSControlPointMarkerWidget.h"
#include "UI/Layers/OSControlPointInfoWidget.h"

void UOSControlPointLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind once — not inside RebuildFromGameState to avoid unbind/rebind race
	if (UWorld* World = GetWorld())
	{
		if (auto* GS = World->GetGameState<AOSGameState_Domination>())
			GS->OnControlPointsChanged.AddDynamic(this, &UOSControlPointLayerWidget::HandlePointsChanged);
	}

	HandlePointsChanged();
}


void UOSControlPointLayerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		if (auto* GS = World->GetGameState<AOSGameState_Domination>())
			GS->OnControlPointsChanged.RemoveDynamic(this, &UOSControlPointLayerWidget::HandlePointsChanged);
	}

	Super::NativeDestruct();
}

void UOSControlPointLayerWidget::HandlePointsChanged()
{
	RebuildMarkers();
	RebuildFromGameState();
}

void UOSControlPointLayerWidget::RebuildFromGameState()
{
	if (!GetWorld()) return;
	auto* GS = GetWorld()->GetGameState<AOSGameState_Domination>();
	if (!PointList || !GS) return;

	PointList->ClearChildren();

	for (AOSControlPoint* point : GS->ControlPoints)
	{
		if (!IsValid(point)) continue;

		auto* info = CreateWidget<UOSControlPointInfoWidget>(GetOwningPlayer(), InfoWidgetClass);
		if (!info) continue;

		PointList->AddChild(info);
		info->BindToPoint(point);
	}
}

void UOSControlPointLayerWidget::NativeTick( const FGeometry& MyGeometry, float InDeltaTime )
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

		const FVector WorldLoc = Point->GetActorLocation() + FVector(0,0, 120.f);

		FVector2D Screen;
		const bool bOnScreen = UGameplayStatics::ProjectWorldToScreen(PC, WorldLoc, Screen, true);

		// hide if behind cam
		

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
		if (UCanvasPanelSlot* slot = Cast<UCanvasPanelSlot>(Marker->Slot))
		{
			slot->SetPosition(Screen);
		}
	}
}

void UOSControlPointLayerWidget::RebuildMarkers()
{
	if (!WorldMarkerRoot || !MarkerWidgetClass) return;
	
	WorldMarkerRoot->ClearChildren();
	Markers.Empty();

	if (!GetWorld()) return;
	auto* GS = GetWorld()->GetGameState<AOSGameState_Domination>();
	if (!GS) return;

	for (AOSControlPoint* P : GS->ControlPoints)
	{
		if (!IsValid(P)) continue;

		UUserWidget* W = CreateWidget<UUserWidget>(GetOwningPlayer(), MarkerWidgetClass);
		if (!W) continue;

		WorldMarkerRoot->AddChild(W);
		
		if (auto* slot = Cast<UCanvasPanelSlot>(W->Slot))
		{
			slot->SetAutoSize(true);
			slot->SetAlignment(FVector2D(.5f, .5f));
		}

		if (auto* marker = Cast<UOSControlPointMarkerWidget>(W))
		{
			marker->BindToPoint(P);
		}

		Markers.Add(P, W);
	}
}
