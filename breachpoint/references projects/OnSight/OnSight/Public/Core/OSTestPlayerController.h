// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OSTestPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API AOSTestPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
    AOSTestPlayerController();

protected:
    virtual void SetupInputComponent() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class UOSDynamicCameraComponent* DynamicCamera;

private:
    void ToggleLockOn();
    void SwitchTarget();
    void ToggleDebug();
};
