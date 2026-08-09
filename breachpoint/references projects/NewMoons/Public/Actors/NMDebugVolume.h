// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NMDebugVolume.generated.h"

class UNMHealthComponent;
class UBoxComponent;

UCLASS()
class NEWMOONS_API ANMDebugVolume : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANMDebugVolume();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Amount of health to damage or heal applied every second
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default")
	float DamageOrHealPerSecond;

	// True apply damage, false apply heal
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default")
	bool bDamageOrHeal;

	// Timer handle for applying damage
	FTimerHandle DamageOrHealTimerHandle;

	// Overlap begin function
	UFUNCTION()
	void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
						class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
						bool bFromSweep, const FHitResult & SweepResult);

	// Overlap end function
	UFUNCTION()
	void OnOverlapEnd(class UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
					  class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Applies damage or heal to the actor
	void ApplyDamageOrHeal();

private:
	// Array to hold actors being damaged or healed
	TArray<AActor*> OverlappingActors;
    
	// Collision box component
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* CollisionBox;
};
