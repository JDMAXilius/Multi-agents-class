// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NMInteractionComponent.generated.h"


UCLASS(ClassGroup = (NM), meta = (BlueprintSpawnableComponent), Blueprintable)
class NEWMOONS_API UNMInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UNMInteractionComponent();

	UFUNCTION(BlueprintCallable)
	void Interact();

	APawn* GetOwningPawn();

	APlayerController* GetOwningPlayerController();
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void DestroyComponent(bool bPromoteChildren) override;

	TWeakObjectPtr<APawn> OwningPawn;
	TWeakObjectPtr<APlayerController> OwningPlayerController;
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
