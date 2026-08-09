// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/NMDebugVolume.h"
#include "Components/BoxComponent.h"
#include "Characters/Components/NMHealthComponent.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ANMDebugVolume::ANMDebugVolume()
	: DamageOrHealPerSecond(20.0f)
	, bDamageOrHeal(true)
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	// Create and configure the box component
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(200.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	SetRootComponent(CollisionBox);

	// Bind overlap events
	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ANMDebugVolume::OnOverlapBegin);
	CollisionBox->OnComponentEndOverlap.AddDynamic(this, &ANMDebugVolume::OnOverlapEnd);
}

// Called when the game starts or when spawned
void ANMDebugVolume::BeginPlay()
{
	Super::BeginPlay();
}

void ANMDebugVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && !OverlappingActors.Contains(OtherActor))
	{
		if (UNMHealthComponent* HealthComponent = UNMHealthComponent::FindHealthComponent(OtherActor))
		{
 			OverlappingActors.Add(OtherActor);
			GetWorld()->GetTimerManager().SetTimer(DamageOrHealTimerHandle, this, &ANMDebugVolume::ApplyDamageOrHeal, 1.0f, true);
		}
	}
}

void ANMDebugVolume::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (OtherActor && OverlappingActors.Contains(OtherActor))
	{
		OverlappingActors.Remove(OtherActor);

		if (OverlappingActors.Num() == 0)
		{
			GetWorld()->GetTimerManager().ClearTimer(DamageOrHealTimerHandle);
		}
	}
}

void ANMDebugVolume::ApplyDamageOrHeal()
{
	for (int32 i = 0; i < OverlappingActors.Num(); ++i)
	{
		AActor* Actor = OverlappingActors[i];
		if (Actor == nullptr) continue;

		if (UNMHealthComponent* HealthComponent = UNMHealthComponent::FindHealthComponent(Actor))
		{
			if (bDamageOrHeal)
			{
				UGameplayStatics::ApplyDamage(Actor, DamageOrHealPerSecond, this->GetInstigatorController(), this, UDamageType::StaticClass());
				// HealthComponent->TakeDamage(DamageOrHealPerSecond, Actor, this);
			}
			else
			{
				HealthComponent->TakeHeal(DamageOrHealPerSecond, Actor, this);
			}
		}
	}
}