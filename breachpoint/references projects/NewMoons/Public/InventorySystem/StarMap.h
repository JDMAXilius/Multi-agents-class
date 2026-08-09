// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI//UIInteractableStaticActor.h"
#include "StarMap.generated.h"

UCLASS()
class NEWMOONS_API AStarMap final : public AUIInteractableStaticActor
{
	GENERATED_BODY()
	
public:
    // Sets default values for this actor's properties
    AStarMap(const FObjectInitializer& ObjectInitializer);

    // Called every frame
    virtual void Tick(float DeltaTime) override;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

private:
    // Inherited via IInteractableInterface
    void Interact_Implementation(AActor* Interactor) override;

    // Inherited via IUIInteractableInterface
    void ShowUI_Implementation(AActor* Interactor) override;
    void HideUI_Implementation(AActor* Interactor) override;
};
