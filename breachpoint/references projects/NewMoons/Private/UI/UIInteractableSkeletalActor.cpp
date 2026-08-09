// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/UIInteractableSkeletalActor.h"

AUIInteractableSkeletalActor::AUIInteractableSkeletalActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(GetRootComponent());
}

void AUIInteractableSkeletalActor::BeginPlay()
{
	Super::BeginPlay();

}

void AUIInteractableSkeletalActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
