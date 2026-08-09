// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventorySystem/InteractableActorBase.h"
#include "UIInteractableInterface.h"
#include "Blueprint/UserWidget.h"
#include "UIInteractableActorBase.generated.h"

UCLASS()
class NEWMOONS_API AUIInteractableActorBase : public AInteractableActorBase, public IUIInteractableInterface
{
	GENERATED_BODY()
	
public:
	AUIInteractableActorBase(const FObjectInitializer& ObjectInitializer);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Inherited via AInteractableActorBase -> IInteractableInterface
	virtual void Interact_Implementation(AActor* Interactor) override;

	// Inherited via IUIInteractableInterface
	void ShowUI_Implementation(AActor* Interactor) override;
	void HideUI_Implementation(AActor* Interactor) override;

protected:
	/** Widget class to spawn when interacted with this actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> WidgetClass;

	/** Instance of the widget */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UUserWidget* WidgetInstance;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called when an actor is no longer overlapping with the sphere
	virtual void OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) override;
};
