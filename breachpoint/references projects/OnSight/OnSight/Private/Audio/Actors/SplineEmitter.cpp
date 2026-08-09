// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/Actors/SplineEmitter.h"

#include "Kismet/GameplayStatics.h"

// Sets default values
ASplineEmitter::ASplineEmitter()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	Spline = CreateDefaultSubobject<USplineComponent>("Spline");

}

// Called when the game starts or when spawned
void ASplineEmitter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASplineEmitter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASplineEmitter::Main()
{
	//Set Player Location
	PlayerLocation = FVector::ZeroVector;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	PlayerLocation = Pawn->GetActorLocation();
	//DrawDebugSphere(GetWorld(), PlayerLocation, 10, 12, FColor::Purple, false, 0.1); 
	
	//Set Closest Location
	ClosestLocation = Spline->FindLocationClosestToWorldLocation(PlayerLocation, ESplineCoordinateSpace::World);
	//DrawDebugSphere(GetWorld(), ClosestLocation, 10, 12, FColor::Purple, false, 0.1);
	
	//If the player is inside the spline loop
	if (Spline->IsClosedLoop())
	{
		float DotProduct = FVector::DotProduct(PlayerLocation - ClosestLocation, Spline->FindRightVectorClosestToWorldLocation(PlayerLocation, ESplineCoordinateSpace::World));
		float VerticalDistance = FMath::Abs(PlayerLocation.Z - ClosestLocation.Z);
		//GEngine->AddOnScreenDebugMessage(1, 9, FColor::Purple, FString::Printf(TEXT("DotProduct: %f"), DotProduct));
		
		if (DotProduct < 0.0f && VerticalDistance <= MaxVerticalDistance)
		{
			

			bInSpline = true;
		}
		else
		{
			bInSpline = false;
			
		}
	}
	else
	{
		bInSpline = false;
	}
	
	//Set EmitterLocation from bInSpline
	if (bInSpline)
	{
		EmitterLocation = FVector(PlayerLocation.X, PlayerLocation.Y, ClosestLocation.Z);
	}
	else
	{
		EmitterLocation = ClosestLocation;
	}
	//DrawDebugSphere(GetWorld(), EmitterLocation, 30, 12, FColor::Orange, false, 0.1);
	
	//Distance to closest location
	
	DistanceToClosestLocation = FVector::Distance(PlayerLocation, EmitterLocation);
	//GEngine->AddOnScreenDebugMessage(2, 9, FColor::Purple, FString::Printf(TEXT("Distance: %f"), DistanceToClosestLocation));
}	