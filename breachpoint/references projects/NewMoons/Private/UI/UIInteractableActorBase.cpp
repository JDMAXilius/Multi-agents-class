// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/UIInteractableActorBase.h"
#include "Kismet/GameplayStatics.h"

AUIInteractableActorBase::AUIInteractableActorBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AUIInteractableActorBase::BeginPlay()
{
	Super::BeginPlay();

}

void AUIInteractableActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AUIInteractableActorBase::Interact_Implementation(AActor* Interactor)
{
	Super::Interact_Implementation(Interactor);

	IUIInteractableInterface::Execute_ShowUI(this, Interactor);
}

void AUIInteractableActorBase::ShowUI_Implementation(AActor* Interactor)
{
	if (!IsValid(WidgetClass))
	{
		return;
	}

	if (WidgetInstance) // Widget is still on the screen
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Interactor->GetOwner()))
	{
		WidgetInstance = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
		WidgetInstance->AddToViewport();
	}

	SetInteractionWidgetVisibility(ESlateVisibility::Hidden);
}

void AUIInteractableActorBase::HideUI_Implementation(AActor* Interactor)
{
	if (WidgetInstance) // Widget is still on the screen
	{
		WidgetInstance->RemoveFromParent();
		WidgetInstance = nullptr;
	}
}

void AUIInteractableActorBase::OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Super::OnInteractSphereEndOverlap(OverlappedComp, OtherActor, OtherComp, OtherBodyIndex);

	IUIInteractableInterface::Execute_HideUI(this, OtherActor);
}
