// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/UIInteractableStaticActor.h"

AUIInteractableStaticActor::AUIInteractableStaticActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(GetRootComponent());
}

void AUIInteractableStaticActor::BeginPlay()
{
	Super::BeginPlay();

}

void AUIInteractableStaticActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}