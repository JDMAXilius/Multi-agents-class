// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "ZoransResistance/AbilitySystem/Data/AbilitySystemData.h"
#include "DamageWidgetComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable )
class ZORANSRESISTANCE_API UDamageWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:

	UDamageWidgetComponent();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void DisplayDamageText(const AActor* DamagedActor, float DamageAmount, const ETargetHitType HitType, const EHitSeverity HitSeverity);
};