// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UIInteractableActorBase.h"
#include "UIInteractableStaticActor.generated.h"

UCLASS()
class NEWMOONS_API AUIInteractableStaticActor : public AUIInteractableActorBase
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AUIInteractableStaticActor(const FObjectInitializer& ObjectInitializer);

	FORCEINLINE UStaticMeshComponent* GetMesh() const { return StaticMeshComponent; }

	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	/** Static Mesh SubObject */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* StaticMeshComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
