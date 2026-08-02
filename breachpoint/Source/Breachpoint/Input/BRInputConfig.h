#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRInputConfig.generated.h"

class UInputAction;

USTRUCT()
struct FBRInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	bool IsValidRow() const
	{
		return !InputAction.IsNull() && InputTag.IsValid();
	}

	const UInputAction* GetInputAction(bool bLoadSynchronousIfNeeded = true) const;
};

UCLASS(meta = (DisplayName = "BR Input Config"))
class BREACHPOINT_API UBRInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:

	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bEnsureIfNotFound = true) const;

	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bEnsureIfNotFound = true) const;

	const TArray<FBRInputAction>& GetNativeInputActions() const { return NativeInputActions; }
	const TArray<FBRInputAction>& GetAbilityInputActions() const { return AbilityInputActions; }

	void GetAllInputActionPaths(TArray<FSoftObjectPath>& OutPaths) const;

	bool AreInputActionsLoaded() const;

	TSharedPtr<FStreamableHandle> PreloadInputActionsAsync(const FStreamableDelegate& OnLoaded = FStreamableDelegate()) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, Category = "Input|Native", meta = (TitleProperty = "InputTag"))
	TArray<FBRInputAction> NativeInputActions;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Ability", meta = (TitleProperty = "InputTag"))
	TArray<FBRInputAction> AbilityInputActions;

private:
	static const UInputAction* FindActionForTag(const TArray<FBRInputAction>& Rows, const FGameplayTag& InputTag, bool bEnsureIfNotFound, const TCHAR* ListName);
};
