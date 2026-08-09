// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSDynamicCameraComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ONSIGHT_API UOSDynamicCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    UOSDynamicCameraComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Lock-on
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock-On")
    bool bLockOnEnabled = false;

    UFUNCTION(BlueprintCallable, Category = "Lock-On")
    void ToggleLockOn();

    UFUNCTION(BlueprintCallable, Category = "Lock-On")
    void SwitchTarget(); // Cycle to next enemy

    // Normal Camera (no lock-on)
    UPROPERTY(EditAnywhere, Category = "Camera|Normal")
    float NormalDistance = 350.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Normal")
    float NormalHeight = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Normal")
    float NormalSideOffset = 60.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Normal")
    float NormalFOV = 90.0f;

    // Combat Camera (locked on single enemy)
    UPROPERTY(EditAnywhere, Category = "Camera|Combat")
    float CombatDistance = 180.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Combat")
    float CombatHeight = 70.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Combat")
    float CombatSideOffset = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Camera|Combat")
    float CombatFOV = 80.0f;

    // Multi-Enemy Combat Camera (multiple threats)
    UPROPERTY(EditAnywhere, Category = "Camera|Multi-Enemy")
    float MultiDistance = 250.0f; // Pull back more

    UPROPERTY(EditAnywhere, Category = "Camera|Multi-Enemy")
    float MultiHeight = 110.0f; // Higher for better view

    UPROPERTY(EditAnywhere, Category = "Camera|Multi-Enemy")
    float MultiSideOffset = 120.0f; // More to side

    UPROPERTY(EditAnywhere, Category = "Camera|Multi-Enemy")
    float MultiFOV = 90.0f; // Wider FOV

    UPROPERTY(EditAnywhere, Category = "Camera|Multi-Enemy")
    float MultiEnemyThreshold = 1000.0f; // Distance to consider enemies as "nearby threats"

    // Detection
    UPROPERTY(EditAnywhere, Category = "Detection")
    float DetectionRadius = 2000.0f;

    // Interpolation
    UPROPERTY(EditAnywhere, Category = "Camera")
    float LerpSpeed = 8.0f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = false;

private:
    UPROPERTY()
    class APlayerController* OwnerPC;

    UPROPERTY()
    class APawn* PlayerPawn;

    UPROPERTY()
    class UCameraComponent* PlayerCamera;

    TWeakObjectPtr<AActor> TargetEnemy;
    TArray<TWeakObjectPtr<AActor>> NearbyEnemies;

    void DetectEnemies();
    void UpdateCamera(float DeltaTime);
    FVector CalculateNormalCamera();
    FVector CalculateCombatCamera();
    FVector CalculateMultiEnemyCamera();

    int32 GetNearbyEnemyCount(); // Enemies near player, excluding current target
};
