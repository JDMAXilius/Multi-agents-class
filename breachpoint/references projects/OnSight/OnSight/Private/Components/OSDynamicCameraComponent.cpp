// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/OSDynamicCameraComponent.h"
#include "Core/OSPlayerController.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

UOSDynamicCameraComponent::UOSDynamicCameraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UOSDynamicCameraComponent::ToggleLockOn()
{
    bLockOnEnabled = !bLockOnEnabled;
    UE_LOG(LogTemp, Warning, TEXT("Lock-On: %s"), bLockOnEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));

    if (!bLockOnEnabled)
    {
        TargetEnemy.Reset();
        NearbyEnemies.Empty();
    }
}

void UOSDynamicCameraComponent::SwitchTarget()
{
    if (!bLockOnEnabled || NearbyEnemies.Num() == 0)
    {
        return;
    }

    // Find current target index
    int32 CurrentIndex = -1;
    for (int32 i = 0; i < NearbyEnemies.Num(); i++)
    {
        if (NearbyEnemies[i].Get() == TargetEnemy.Get())
        {
            CurrentIndex = i;
            break;
        }
    }

    // Cycle to next enemy
    int32 NextIndex = (CurrentIndex + 1) % NearbyEnemies.Num();
    if (NearbyEnemies.IsValidIndex(NextIndex) && NearbyEnemies[NextIndex].IsValid())
    {
        TargetEnemy = NearbyEnemies[NextIndex];
        UE_LOG(LogTemp, Warning, TEXT("Switched to target: %s"), *TargetEnemy->GetName());
    }
}

void UOSDynamicCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!OwnerPC)
    {
        OwnerPC = Cast<APlayerController>(GetOwner());
        if (!OwnerPC) return;
    }

    if (!PlayerPawn || PlayerPawn != OwnerPC->GetPawn())
    {
        PlayerPawn = OwnerPC->GetPawn();
        if (!PlayerPawn) return;

        PlayerCamera = PlayerPawn->FindComponentByClass<UCameraComponent>();
        if (!PlayerCamera)
        {
            UE_LOG(LogTemp, Error, TEXT("No camera component found on player!"));
            return;
        }
    }

    if (bLockOnEnabled)
    {
        DetectEnemies();
    }
    else
    {
        TargetEnemy.Reset();
        NearbyEnemies.Empty();
    }

    UpdateCamera(DeltaTime);
}

void UOSDynamicCameraComponent::DetectEnemies()
{
    NearbyEnemies.Empty();

    TArray<FHitResult> Hits;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(DetectionRadius);
    FVector PlayerLoc = PlayerPawn->GetActorLocation();

    GetWorld()->SweepMultiByChannel(Hits, PlayerLoc, PlayerLoc, FQuat::Identity, ECC_Pawn, Sphere);

    for (const FHitResult& Hit : Hits)
    {
        AActor* HitActor = Hit.GetActor();
        if (HitActor && HitActor != PlayerPawn && HitActor->GetClass() == PlayerPawn->GetClass())
        {
            // Team-safe lock-on: reject same-team actors so lock-on never targets allies (#39).
            if (UOSCombatBlueprintLibrary::AreActorsFriendly(PlayerPawn, HitActor))
            {
                continue;
            }
            NearbyEnemies.Add(HitActor);
        }
    }

    // If we have a valid target and it's still in range, keep it
    if (TargetEnemy.IsValid())
    {
        bool bTargetStillValid = false;
        for (const auto& Enemy : NearbyEnemies)
        {
            if (Enemy.Get() == TargetEnemy.Get())
            {
                bTargetStillValid = true;
                break;
            }
        }

        if (!bTargetStillValid)
        {
            TargetEnemy.Reset();
        }
    }

    // If no target, lock to closest enemy
    if (!TargetEnemy.IsValid() && NearbyEnemies.Num() > 0)
    {
        float ClosestDistance = FLT_MAX;
        AActor* ClosestEnemy = nullptr;

        for (const auto& Enemy : NearbyEnemies)
        {
            if (Enemy.IsValid())
            {
                float Distance = FVector::Distance(PlayerLoc, Enemy->GetActorLocation());
                if (Distance < ClosestDistance)
                {
                    ClosestDistance = Distance;
                    ClosestEnemy = Enemy.Get();
                }
            }
        }

        if (ClosestEnemy)
        {
            TargetEnemy = ClosestEnemy;
            UE_LOG(LogTemp, Warning, TEXT("Locked onto: %s"), *ClosestEnemy->GetName());
        }
    }
}

int32 UOSDynamicCameraComponent::GetNearbyEnemyCount()
{
    if (!PlayerPawn) return 0;

    int32 Count = 0;
    FVector PlayerLoc = PlayerPawn->GetActorLocation();

    for (const auto& Enemy : NearbyEnemies)
    {
        if (Enemy.IsValid() && Enemy.Get() != TargetEnemy.Get())
        {
            float Distance = FVector::Distance(PlayerLoc, Enemy->GetActorLocation());
            if (Distance < MultiEnemyThreshold)
            {
                Count++;
            }
        }
    }

    return Count;
}

void UOSDynamicCameraComponent::UpdateCamera(float DeltaTime)
{
    if (!PlayerPawn || !PlayerCamera) return;

    FVector TargetCameraPos;
    float TargetFOV;
    FVector LookTarget;

    if (bLockOnEnabled && TargetEnemy.IsValid())
    {
        int32 NearbyCount = GetNearbyEnemyCount();

        if (NearbyCount > 0)
        {
            // Multi-enemy mode: other enemies nearby
            TargetCameraPos = CalculateMultiEnemyCamera();
            TargetFOV = MultiFOV;
        }
        else
        {
            // Single combat mode: only one target
            TargetCameraPos = CalculateCombatCamera();
            TargetFOV = CombatFOV;
        }

        // Always look at locked target
        LookTarget = TargetEnemy->GetActorLocation();
        LookTarget.Z += 90;
    }
    else
    {
        // Normal mode
        TargetCameraPos = CalculateNormalCamera();
        TargetFOV = NormalFOV;
        LookTarget = PlayerPawn->GetActorLocation() + FVector(0, 0, 90);
    }

    // Smooth interpolation
    FVector CurrentCameraPos = PlayerCamera->GetComponentLocation();
    FVector NewCameraPos = FMath::VInterpTo(CurrentCameraPos, TargetCameraPos, DeltaTime, LerpSpeed);

    FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(NewCameraPos, LookTarget);
    FRotator CurrentRotation = PlayerCamera->GetComponentRotation();
    FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, LerpSpeed);

    float CurrentFOV = PlayerCamera->FieldOfView;
    float NewFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, LerpSpeed);

    // Apply
    PlayerCamera->SetWorldLocation(NewCameraPos);
    PlayerCamera->SetWorldRotation(NewRotation);
    PlayerCamera->SetFieldOfView(NewFOV);

    // Debug
    if (bDebugDraw)
    {
        FVector PlayerLoc = PlayerPawn->GetActorLocation();
        DrawDebugSphere(GetWorld(), PlayerLoc, DetectionRadius, 32, FColor::Yellow, false, -1, 0, 2);
        DrawDebugSphere(GetWorld(), PlayerLoc, MultiEnemyThreshold, 16, FColor::Orange, false, -1, 0, 1);
        DrawDebugSphere(GetWorld(), NewCameraPos, 30, 12, FColor::Blue, false, -1, 0, 3);
        DrawDebugLine(GetWorld(), NewCameraPos, LookTarget, FColor::Cyan, false, -1, 0, 2);

        // Draw locked target
        if (TargetEnemy.IsValid())
        {
            DrawDebugLine(GetWorld(), PlayerLoc, TargetEnemy->GetActorLocation(), FColor::Red, false, -1, 0, 3);
            DrawDebugSphere(GetWorld(), TargetEnemy->GetActorLocation(), 60, 12, FColor::Red, false, -1, 0, 2);
        }

        // Draw other nearby enemies
        for (const auto& Enemy : NearbyEnemies)
        {
            if (Enemy.IsValid() && Enemy.Get() != TargetEnemy.Get())
            {
                float Distance = FVector::Distance(PlayerLoc, Enemy->GetActorLocation());
                if (Distance < MultiEnemyThreshold)
                {
                    DrawDebugLine(GetWorld(), PlayerLoc, Enemy->GetActorLocation(), FColor::Orange, false, -1, 0, 2);
                    DrawDebugSphere(GetWorld(), Enemy->GetActorLocation(), 40, 12, FColor::Orange, false, -1, 0, 2);
                }
            }
        }

        int32 NearbyCount = GetNearbyEnemyCount();
        FString StatusText = FString::Printf(TEXT("Lock-On: %s | Target: %s | Nearby: %d | Mode: %s"),
            bLockOnEnabled ? TEXT("ON") : TEXT("OFF"),
            TargetEnemy.IsValid() ? TEXT("YES") : TEXT("NO"),
            NearbyCount,
            NearbyCount > 0 ? TEXT("MULTI") : TEXT("SINGLE"));
        DrawDebugString(GetWorld(), PlayerLoc + FVector(0, 0, 200), StatusText, nullptr, FColor::White, 0.0f, true);
    }
}

FVector UOSDynamicCameraComponent::CalculateNormalCamera()
{
    FVector PlayerLoc = PlayerPawn->GetActorLocation();
    FVector PlayerForward = PlayerPawn->GetActorForwardVector();
    FVector PlayerRight = PlayerPawn->GetActorRightVector();

    FVector CameraPos = PlayerLoc;
    CameraPos -= PlayerForward * NormalDistance;
    CameraPos += PlayerRight * NormalSideOffset;
    CameraPos += FVector(0, 0, NormalHeight);

    return CameraPos;
}

FVector UOSDynamicCameraComponent::CalculateCombatCamera()
{
    if (!TargetEnemy.IsValid())
    {
        return CalculateNormalCamera();
    }

    FVector PlayerLoc = PlayerPawn->GetActorLocation();
    FVector EnemyLoc = TargetEnemy->GetActorLocation();

    FVector ToEnemy = (EnemyLoc - PlayerLoc).GetSafeNormal();
    FVector RightOfCombat = FVector::CrossProduct(ToEnemy, FVector::UpVector).GetSafeNormal();

    FVector CameraPos = PlayerLoc;
    CameraPos += RightOfCombat * CombatSideOffset;
    CameraPos -= ToEnemy * CombatDistance;
    CameraPos.Z = PlayerLoc.Z + CombatHeight;

    return CameraPos;
}

FVector UOSDynamicCameraComponent::CalculateMultiEnemyCamera()
{
    if (!TargetEnemy.IsValid())
    {
        return CalculateNormalCamera();
    }

    FVector PlayerLoc = PlayerPawn->GetActorLocation();
    FVector EnemyLoc = TargetEnemy->GetActorLocation();

    // Still look at primary target, but pull back to show threats
    FVector ToEnemy = (EnemyLoc - PlayerLoc).GetSafeNormal();
    FVector RightOfCombat = FVector::CrossProduct(ToEnemy, FVector::UpVector).GetSafeNormal();

    // Calculate how spread out enemies are
    float MaxSpread = 0.0f;
    for (const auto& Enemy : NearbyEnemies)
    {
        if (Enemy.IsValid())
        {
            float Distance = FVector::Distance(PlayerLoc, Enemy->GetActorLocation());
            MaxSpread = FMath::Max(MaxSpread, Distance);
        }
    }

    // Adjust distance based on spread
    float AdjustedDistance = MultiDistance + (MaxSpread * 0.1f);
    AdjustedDistance = FMath::Min(AdjustedDistance, 400.0f); // Cap max distance

    FVector CameraPos = PlayerLoc;
    CameraPos += RightOfCombat * MultiSideOffset;
    CameraPos -= ToEnemy * AdjustedDistance;
    CameraPos.Z = PlayerLoc.Z + MultiHeight;

    return CameraPos;
}