// Fill out your copyright notice in the Description page of Project Settings.

#include "InventorySystem/InteractableStaticActor.h"

AInteractableStaticActor::AInteractableStaticActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(GetRootComponent());
}

void AInteractableStaticActor::BeginPlay()
{
	Super::BeginPlay();

}

void AInteractableStaticActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}