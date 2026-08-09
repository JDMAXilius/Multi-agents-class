// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/OSTestPlayerController.h"
#include "Components/OSDynamicCameraComponent.h"

AOSTestPlayerController::AOSTestPlayerController()
{
    DynamicCamera = CreateDefaultSubobject<UOSDynamicCameraComponent>(TEXT("DynamicCamera"));
}

void AOSTestPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Tab = Toggle lock-on
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AOSTestPlayerController::ToggleLockOn);

    // Q = Switch target (cycle through enemies)
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AOSTestPlayerController::SwitchTarget);

    // G = Toggle debug
    InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AOSTestPlayerController::ToggleDebug);
}

void AOSTestPlayerController::ToggleLockOn()
{
    if (DynamicCamera)
    {
        DynamicCamera->ToggleLockOn();
    }
}

void AOSTestPlayerController::SwitchTarget()
{
    if (DynamicCamera)
    {
        DynamicCamera->SwitchTarget();
    }
}

void AOSTestPlayerController::ToggleDebug()
{
    if (DynamicCamera)
    {
        DynamicCamera->bDebugDraw = !DynamicCamera->bDebugDraw;
    }
}