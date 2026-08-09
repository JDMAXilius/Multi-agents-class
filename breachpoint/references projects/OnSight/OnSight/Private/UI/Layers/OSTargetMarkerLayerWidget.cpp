// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layers/OSTargetMarkerLayerWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/Character.h"
#include "UI/OSHUDWidget.h"

void UOSTargetMarkerLayerWidget::NativeDestruct()
{
	// if (UOSHUDWidget* HUDWidget = GetHUD())
	// 	HUDWidget->OnTargetChanged.RemoveAll(this);

	Super::NativeDestruct();
}

void UOSTargetMarkerLayerWidget::Show(AActor* Target, FName Socket)
{
	if (!IsValid(Target))
	{
		return;
	}
	TargetActorWeak = Target;
	SocketName = Socket;
	SetVisibility(ESlateVisibility::Visible);
}

void UOSTargetMarkerLayerWidget::Hide()
{
	TargetActorWeak.Reset();
	SocketName = NAME_None;
	SetVisibility(ESlateVisibility::Hidden);
}

void UOSTargetMarkerLayerWidget::Init( UOSHUDWidget* HUDWidget )
{
	Super::Init(HUDWidget);
	// if (HUDWidget)
	// 	HUDWidget->OnTargetChanged.AddUObject(this, &UOSTargetMarkerLayerWidget::OnTargetChanged);
}

void UOSTargetMarkerLayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AActor* const Target = TargetActorWeak.Get();
	if (!IsValid(Target))
	{
		if (GetVisibility() == ESlateVisibility::Visible)
		{
			Hide();
		}
		return;
	}

	if (!GetOwningPlayer())
	{
		return;
	}

	const FVector Location = Get3DLocation(Target);

	FVector2D ScreenPos;
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Location, ScreenPos, true);

	OnTick(Location, ScreenPos);
}

FVector UOSTargetMarkerLayerWidget::Get3DLocation(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return FVector::ZeroVector;
	}

	FVector TargetLocation = Target->GetActorLocation();

	const ACharacter* const Chara = Cast<ACharacter>(Target);
	if (!Chara)
	{
		return TargetLocation;
	}

	const USkeletalMeshComponent* const Mesh = Chara->GetMesh();
	if (!Mesh)
	{
		return TargetLocation;
	}

	const FVector Loc = Mesh->GetBoneLocation(SocketName);
	if (!Loc.IsNearlyZero())
	{
		TargetLocation = Loc;
	}

	return TargetLocation;
}

void UOSTargetMarkerLayerWidget::OnTargetChanged( AActor* Actor )
{
	if (!Actor)
		Hide();
	else
		Show(Actor);
}
