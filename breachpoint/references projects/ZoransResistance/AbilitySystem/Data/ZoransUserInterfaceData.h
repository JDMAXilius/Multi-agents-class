// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectUIData.h"
#include "ZoransUserInterfaceData.generated.h"

class UZoransGameplayEffect;
class UGameplayAbility;

UENUM(BlueprintType, Meta = (DisplayName = "UI Data Type"))
enum class EUIDataType : uint8
{
	Misc				UMETA(DisplayNAme = "Misc"),
	Buff				UMETA(DisplayName = "Buff"),
	Debuff				UMETA(DisplayName = "Debuff"),
	Cooldown			UMETA(DisplayName = "Cooldown")
};

UCLASS()
class ZORANSRESISTANCE_API UZoransUserInterfaceData : public UGameplayEffectUIData
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Meta = (DisplayName = "UI Data Type"))
	EUIDataType UIDataType = EUIDataType::Misc;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FString DisplayName = "Display Name";
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Description = FText::FromString("Description");
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TSubclassOf<UUserWidget> WidgetClass;
};