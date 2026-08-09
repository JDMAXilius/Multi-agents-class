// Fill out your copyright notice in the Description page of Project Settings.

#include "InventorySystem/InteractableSkeletalActor.h"

AInteractableSkeletalActor::AInteractableSkeletalActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(GetRootComponent());
}

void AInteractableSkeletalActor::BeginPlay()
{
	Super::BeginPlay();

}

void AInteractableSkeletalActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
