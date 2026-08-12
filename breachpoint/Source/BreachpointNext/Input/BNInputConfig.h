#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BNInputConfig.generated.h"

class UInputAction;

USTRUCT()
struct FBNInputBinding
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag;
};

UCLASS()
class BREACHPOINTNEXT_API UBNInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputAction* FindInputActionByTag(FGameplayTag InputTag) const;

	UPROPERTY(EditDefaultsOnly)
	TArray<FBNInputBinding> Bindings;
};
