#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BNInputConfig.generated.h"

class UInputAction;

// BlueprintType is here for Python, not for Blueprints: UE only generates editor-script
// bindings for exposed structs, and without it Tools/bn/10_input_assets.py cannot build a
// row — leaving hand-placing the asset as the only option, which law 7 forbids.
USTRUCT(BlueprintType)
struct FBNInputBinding
{
	GENERATED_BODY()

	// EditAnywhere, not EditDefaultsOnly: a DataAsset on disk IS an instance, so
	// EditDefaultsOnly sets CPF_DisableEditOnInstance and Python refuses to write the
	// row ("cannot be edited on instances"). Same details panel, but scriptable.
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> InputAction;

	UPROPERTY(EditAnywhere)
	FGameplayTag InputTag;
};

UCLASS()
class BREACHPOINTNEXT_API UBNInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputAction* FindInputActionByTag(FGameplayTag InputTag) const;

	UPROPERTY(EditAnywhere)
	TArray<FBNInputBinding> Bindings;
};
