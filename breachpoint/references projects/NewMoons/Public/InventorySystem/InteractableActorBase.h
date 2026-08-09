// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h"
#include "InteractableActorBase.generated.h"

class UUserWidget;
class UWidgetComponent;
class USphereComponent;

UCLASS(abstract)
class NEWMOONS_API AInteractableActorBase : public AActor, public IInteractableInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInteractableActorBase(const FObjectInitializer& ObjectInitializer);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Inherited via IInteractableInterface
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	/** Root Transform Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	USceneComponent* RootSceneComponent;

	/** Interact Sphere Collider SubObject */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	USphereComponent* InteractSphereCollider;

	/** Interact Widget Component SubObject */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* InteractWidgetComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called when an actor overlap with the sphere
	UFUNCTION()
	virtual void OnInteractSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Called when an actor is no longer overlapping with the sphere
	UFUNCTION()
	virtual void OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void SetInteractionWidgetVisibility(ESlateVisibility InVisibility);

	FORCEINLINE UUserWidget* GetInteractableWidget() const;
};
