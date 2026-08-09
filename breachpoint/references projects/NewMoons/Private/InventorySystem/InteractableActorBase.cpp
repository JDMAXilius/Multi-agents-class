// Fill out your copyright notice in the Description page of Project Settings.

#include "InventorySystem/InteractableActorBase.h"

#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"

// Sets default values
AInteractableActorBase::AInteractableActorBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	InteractSphereCollider = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphereCollider"));
	InteractSphereCollider->SetSphereRadius(200.f);
	InteractSphereCollider->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnInteractSphereBeginOverlap);
	InteractSphereCollider->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnInteractSphereEndOverlap);
	InteractSphereCollider->SetupAttachment(GetRootComponent());

	InteractWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractWidgetComponent"));
	InteractWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	InteractWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	InteractWidgetComponent->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void AInteractableActorBase::BeginPlay()
{
	Super::BeginPlay();

	// Hide the interact widget by default
	SetInteractionWidgetVisibility(ESlateVisibility::Hidden);
}

// Called every frame
void AInteractableActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AInteractableActorBase::OnInteractSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<ACharacter>(OtherActor))
	{
		SetInteractionWidgetVisibility(ESlateVisibility::Visible);
	}
}

void AInteractableActorBase::OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Cast<ACharacter>(OtherActor))
	{
		SetInteractionWidgetVisibility(ESlateVisibility::Hidden);
	}
}

void AInteractableActorBase::SetInteractionWidgetVisibility(ESlateVisibility InVisibility)
{
	if (!GetInteractableWidget())
	{
		return;
	}

	GetInteractableWidget()->SetVisibility(InVisibility);
}

UUserWidget* AInteractableActorBase::GetInteractableWidget() const
{
	return InteractWidgetComponent->GetWidget();
}

void AInteractableActorBase::Interact_Implementation(AActor* Interactor)
{

}