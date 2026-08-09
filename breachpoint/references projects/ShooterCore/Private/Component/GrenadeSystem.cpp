
#include "Component/GrenadeSystem.h"
#include "Main/PlayerCharacter.h"
#include "Actors/GrenadeSplineEndPointBase.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Pickups/GrenadeBase.h"

UGrenadeSystem::UGrenadeSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.SetTickFunctionEnable(true);


	SplineMeshLength = 100.0f;
	ThrowSpeed = 2000.0f;
	SplineIndex = 0;
	bInHand = false;
	bThrow = false;
	bCancel = false;
}

void UGrenadeSystem::BeginPlay()
{
	Super::BeginPlay();	
	Player = Cast<APlayerCharacter>(GetOwner());
}

void UGrenadeSystem::CreatePredictionSpline()
{
	PredictionSpline = NewObject<USplineComponent>(this, USplineComponent::StaticClass(), TEXT("GrenadeTrajectorySpline"));
	PredictionSpline->RegisterComponent();
	PredictionSpline->AttachToComponent(Player->GetGrenadeThrowComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	PredictionEndPoint = GetWorld()->SpawnActor<AGrenadeSplineEndPointBase>(PredictionEndPointClass);
	PredictionEndPoint->SetActorHiddenInGame(false);
	PredictionSpline->SetComponentTickEnabled(true);
	PredictionSpline->PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UGrenadeSystem::DrawPredictionSpline()
{
	if (!PredictionSpline || !Player) return;
	bInHand = true;

	// --- Setup projectile path prediction ---
	FPredictProjectilePathParams PredictParams;
	PredictParams.StartLocation = GetThrowLocation(); // From GrenadeThrowTransform
	PredictParams.LaunchVelocity = GetThrowVelocity();
	PredictParams.ProjectileRadius = 0.0f;
	PredictParams.TraceChannel = ECC_WorldDynamic;
	PredictParams.bTraceComplex = false;
	PredictParams.SimFrequency = 15.0f;
	PredictParams.MaxSimTime = 2.0f;
	PredictParams.OverrideGravityZ = 0.0f;
	PredictParams.ActorsToIgnore.Add(Player);
	PredictParams.bTraceWithCollision = true;
	//PredictParams.DrawDebugType = EDrawDebugTrace::ForOneFrame;

	FPredictProjectilePathResult PredictResult;
	bool bHit = UGameplayStatics::PredictProjectilePath(this, PredictParams, PredictResult);

	if (bHit)
	{
		PredictionEndPoint->SetActorHiddenInGame(false);
		PredictionEndPoint->SetActorLocation(PredictResult.HitResult.ImpactPoint);
	}
	else
	{
		PredictionEndPoint->SetActorHiddenInGame(true);
	}

	// --- Update spline with predicted path points ---
	TArray<FVector> PathPoints;
	for (const FPredictProjectilePathPointData& Point : PredictResult.PathData)
	{
		PathPoints.Add(Point.Location);
	}
	PredictionSpline->SetSplinePoints(PathPoints, ESplineCoordinateSpace::World);
	PredictionSpline->UpdateSpline();

	// --- Destroy old spline meshes ---
	DestroyPredictionMesh();

	// --- Generate new spline meshes along path ---
	const int PointCount = PredictionSpline->GetNumberOfSplinePoints();
	for (int i = 0; i < PointCount - 1; i++)
	{
		USplineMeshComponent* NewSplineMeshComp = NewObject<USplineMeshComponent>(GetOwner(), USplineMeshComponent::StaticClass());
		NewSplineMeshComp->SetMobility(EComponentMobility::Movable);
		NewSplineMeshComp->SetStaticMesh(PredictionSplineMesh);
		NewSplineMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewSplineMeshComp->SetGenerateOverlapEvents(false);
		NewSplineMeshComp->SetStartScale(FVector2D(0.1f, 0.1f));
		NewSplineMeshComp->SetEndScale(FVector2D(0.1f, 0.1f));
		NewSplineMeshComp->RegisterComponent();
		NewSplineMeshComp->AttachToComponent(PredictionSpline, FAttachmentTransformRules::KeepWorldTransform);

		const FVector StartPos = PredictionSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		const FVector StartTan = PredictionSpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
		const FVector EndPos = PredictionSpline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World);
		const FVector EndTan = PredictionSpline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::World);

		NewSplineMeshComp->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);

		PredictionSplineMeshArray.Add(NewSplineMeshComp);
	}
}




//void UGrenadeSystem::DrawPredictionSpline()
//{
//	if (!PredictionSpline) return;
//	bInHand = true;
//	FPredictProjectilePathParams PredictParams;
//	PredictParams.StartLocation = GetThrowLocation();
//	PredictParams.LaunchVelocity = GetThrowVelocity();
//	PredictParams.ProjectileRadius = 0.0f;
//	PredictParams.TraceChannel = ECC_WorldDynamic;
//	PredictParams.bTraceComplex = false;
//	PredictParams.SimFrequency = 15.0f;
//	PredictParams.MaxSimTime = 2.0f;
//	PredictParams.OverrideGravityZ = 0.0f;
//	PredictParams.ActorsToIgnore.Add(Player);
//	PredictParams.bTraceWithCollision = true;
//	PredictParams.DrawDebugType = EDrawDebugTrace::ForOneFrame;
//	FPredictProjectilePathResult PredictResult;
//
//	bool bHit = UGameplayStatics::PredictProjectilePath(this, PredictParams, PredictResult);
//	if (bHit)
//	{
//		PredictionEndPoint->SetActorHiddenInGame(false);
//		PredictionEndPoint->SetActorLocation(PredictResult.HitResult.ImpactPoint);
//	}
//	else
//	{
//		PredictionEndPoint->SetActorHiddenInGame(true);
//	}
//	TArray<FVector> PathPositions;
//	for (const FPredictProjectilePathPointData& Point : PredictResult.PathData)
//	{
//		PathPositions.Add(Point.Location);
//	}
//	PredictionSpline->SetSplineLocalPoints(PathPositions);
//	//NoCoordinateSpoace
//	PredictionSpline->UpdateSpline();
//	DestroyPredictionMesh();
//
//	int LoopLastIndex = FMath::TruncToInt(PredictionSpline->GetSplineLength() / SplineMeshLength);
//	for (int i = 0; i <= LoopLastIndex; i++)
//	{
//		SplineIndex = i;
//		USplineMeshComponent* NewSplineMeshComp = NewObject<USplineMeshComponent>(GetOwner(), USplineMeshComponent::StaticClass(), TEXT("GrenadePredictionSplineMesh"));
//		NewSplineMeshComp->SetMobility(EComponentMobility::Movable);
//		NewSplineMeshComp->SetStaticMesh(PredictionSplineMesh);
//		NewSplineMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
//		NewSplineMeshComp->SetGenerateOverlapEvents(false);
//		NewSplineMeshComp->SetStartScale(FVector2D(0.1f, 0.1f));
//		NewSplineMeshComp->SetEndScale(FVector2D(0.1f, 0.1f));
//		NewSplineMeshComp->RegisterComponent();
//		NewSplineMeshComp->AttachToComponent(PredictionSpline, FAttachmentTransformRules::KeepRelativeTransform);
//
//		PredictionSplineMeshArray.Add(NewSplineMeshComp);
//		FVector StartPosition = PredictionSpline->GetLocationAtDistanceAlongSpline(SplineIndex * SplineMeshLength, ESplineCoordinateSpace::World);
//		FVector StartTangent = PredictionSpline->GetTangentAtDistanceAlongSpline(SplineIndex * SplineMeshLength, ESplineCoordinateSpace::World);
//		FMath::Clamp(StartTangent.X, 0, 100);
//		FMath::Clamp(StartTangent.Y, 0, 100);
//		FVector EndPosition = PredictionSpline->GetLocationAtDistanceAlongSpline((SplineIndex + 1) * SplineMeshLength, ESplineCoordinateSpace::World);
//		FVector EndTangent = PredictionSpline->GetTangentAtDistanceAlongSpline((SplineIndex + 1) * SplineMeshLength, ESplineCoordinateSpace::World);
//		FMath::Clamp(EndTangent.X, 0, 100);
//		FMath::Clamp(EndTangent.Y, 0, 100);
//		NewSplineMeshComp->SetStartAndEnd(StartPosition, StartTangent, EndPosition, EndTangent, true);
//	}
//}

void UGrenadeSystem::DestroyPredictionMesh()
{
	for (USplineMeshComponent* SplineMesh : PredictionSplineMeshArray)
	{
		if (IsValid(SplineMesh))
		{
			SplineMesh->DestroyComponent();
		}
	}
	PredictionSplineMeshArray.Empty();
}

void UGrenadeSystem::DestroyPredictionSpline()
{
	if (!PredictionSpline) return;
	PredictionSpline->DestroyComponent();
	PredictionSpline = nullptr;
	DestroyPredictionMesh();
	PredictionEndPoint->Destroy();
	PredictionEndPoint = nullptr;
}

//
void UGrenadeSystem::SetGrenadeMesh(bool bGrenadeInHand)
{
	Player->SetInHandMesh(bGrenadeInHand, EPickupType::Grenade);
}

FVector UGrenadeSystem::GetThrowVelocity()
{
	if(!Player) return FVector::ZeroVector;
	return Player->GetGrenadeThrowComponent()->GetForwardVector() * ThrowSpeed;
}

FVector UGrenadeSystem::GetThrowLocation()
{
	if(!Player) return FVector::ZeroVector;
	return Player->GetGrenadeThrowComponent()->GetComponentLocation();
}

void UGrenadeSystem::Throw()
{
	if (!bInHand) return;
	DestroyPredictionSpline();
	bThrow = true;
	FTimerHandle GrenadeThrowTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(GrenadeThrowTimerHandle, [this]()
		{
			SetGrenadeMesh(false);
			//PlayerInventory
			FTransform SpawnGrenadeTransform;
			SpawnGrenadeTransform.SetLocation(GetThrowLocation());
			AGrenadeBase* SpawnnedGrenade = GetWorld()->SpawnActor<AGrenadeBase>(GrenadeClass, SpawnGrenadeTransform);
			SpawnnedGrenade->Throw(GetThrowVelocity(), Player);
			bInHand = false;
			bThrow = false;
			bCancel = false;

		}, 0.5f, false);
}

void UGrenadeSystem::ThrowCancel()
{
	if (!bInHand) return;
	//RemoveLoadingNotification
	DestroyPredictionSpline();
	bCancel = true;
	FTimerHandle GrenadeCancelTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(GrenadeCancelTimerHandle, [this]()
		{
			bInHand = false;
			bThrow = false;
			bCancel = false;
			SetGrenadeMesh(false);

		}, 0.7f, false);
}

void UGrenadeSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	//DrawPredictionSpline();
}

