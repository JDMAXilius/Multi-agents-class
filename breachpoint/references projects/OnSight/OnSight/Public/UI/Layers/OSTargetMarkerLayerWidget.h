// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSBaseLayerWidget.h"
#include "Blueprint/UserWidget.h"
#include "OSTargetMarkerLayerWidget.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSTargetMarkerLayerWidget : public UOSBaseLayerWidget
{
	GENERATED_BODY()
	
public:
	void Show(AActor* Target, FName Socket = NAME_None);
	void Hide();
	virtual void NativeDestruct() override;

	/** Resolved target for UI/Blueprint; null if destroyed or unset. */
	UFUNCTION(BlueprintPure, Category = "UI|Target")
	AActor* GetMarkedTarget() const { return TargetActorWeak.Get(); }

protected:
	virtual void Init(UOSHUDWidget* HUDWidget) override;
	UFUNCTION(BlueprintImplementableEvent)
	void OnTick(const FVector TargetWorldLocation, const FVector2D TargetScreenLocation);

	UPROPERTY(BlueprintReadOnly)
	FName SocketName;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> TargetActorWeak;

	FVector Get3DLocation(const AActor* Target) const;

	void OnTargetChanged(AActor* Actor);
};
