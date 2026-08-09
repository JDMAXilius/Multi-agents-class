// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractableActorBase.h"
#include "InteractableSkeletalActor.generated.h"

UCLASS()
class NEWMOONS_API AInteractableSkeletalActor : public AInteractableActorBase
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AInteractableSkeletalActor(const FObjectInitializer& ObjectInitializer);

	FORCEINLINE USkeletalMeshComponent* GetMesh() const { return SkeletalMeshComponent; }

	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	/** Static Mesh SubObject */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* SkeletalMeshComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
