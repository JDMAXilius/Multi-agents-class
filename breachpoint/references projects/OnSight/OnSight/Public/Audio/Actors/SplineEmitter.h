// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "SplineEmitter.generated.h"

UCLASS()
class ONSIGHT_API ASplineEmitter : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASplineEmitter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USplineComponent* Spline;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector PlayerLocation = FVector(0.0f, 0.0f, 0.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ClosestLocation = FVector(0.0f, 0.0f, 0.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector EmitterLocation = FVector(0.0f, 0.0f, 0.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistanceToClosestLocation = 0.0f;
	
	UPROPERTY(EditAnywhere, Category="Spline Audio")
	float MaxVerticalDistance = 200.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInSpline = false;
	
	UFUNCTION(BlueprintCallable)
	void Main();

};
