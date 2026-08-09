// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Wraps death state and broadcast: subscribes to AttributeSet health changes, runs server-only death handling. Does not hold health value (GAS does).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OSHitDamageContext.h"
#include "OSHealthComponent.generated.h"

class AOSCharacter;

/* Thin state/UI helper around GAS health. Death is routed through GA_OSDeath via the
   GameplayEvent.Death path in OSAttributeSet: this component no longer broadcasts its
   own death delegate. It exists to own the DeathTags container and answer IsDead(). */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ONSIGHT_API UOSHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSHealthComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Death|Properties")
	bool IsDead() const;

	const FGameplayTagContainer& GetDeathTags() const { return DeathTags; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death|Properties")
	FGameplayTagContainer DeathTags;

	TWeakObjectPtr<AOSCharacter> Owner;
};
