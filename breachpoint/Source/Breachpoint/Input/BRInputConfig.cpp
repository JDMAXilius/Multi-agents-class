#include "Input/BRInputConfig.h"

#include "Engine/AssetManager.h"
#include "InputAction.h"

#if WITH_EDITOR
#include "GameplayTagsManager.h"
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "BRInputConfig"

const UInputAction* FBRInputAction::GetInputAction(bool bLoadSynchronousIfNeeded) const
{
	if (InputAction.IsNull())
	{
		return nullptr;
	}

	if (const UInputAction* Resident = InputAction.Get())
	{
		return Resident;
	}

	return bLoadSynchronousIfNeeded ? InputAction.LoadSynchronous() : nullptr;
}

const UInputAction* UBRInputConfig::FindActionForTag(const TArray<FBRInputAction>& Rows, const FGameplayTag& InputTag, bool bEnsureIfNotFound, const TCHAR* ListName)
{
	if (!InputTag.IsValid())
	{
		ensureMsgf(!bEnsureIfNotFound, TEXT("BRInputConfig: asked for an invalid InputTag in the %s list."), ListName);
		return nullptr;
	}

	for (const FBRInputAction& Row : Rows)
	{
		if (Row.InputTag.MatchesTagExact(InputTag))
		{
			if (const UInputAction* Action = Row.GetInputAction())
			{
				return Action;
			}

			ensureMsgf(!bEnsureIfNotFound, TEXT("BRInputConfig: row '%s' in the %s list names an InputAction that failed to load ('%s')."),
				*InputTag.ToString(), ListName, *Row.InputAction.ToSoftObjectPath().ToString());
			return nullptr;
		}
	}

	ensureMsgf(!bEnsureIfNotFound, TEXT("BRInputConfig: no %s row carries the tag '%s'."), ListName, *InputTag.ToString());
	return nullptr;
}

const UInputAction* UBRInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bEnsureIfNotFound) const
{
	return FindActionForTag(NativeInputActions, InputTag, bEnsureIfNotFound, TEXT("native"));
}

const UInputAction* UBRInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bEnsureIfNotFound) const
{
	return FindActionForTag(AbilityInputActions, InputTag, bEnsureIfNotFound, TEXT("ability"));
}

void UBRInputConfig::GetAllInputActionPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Reserve(OutPaths.Num() + NativeInputActions.Num() + AbilityInputActions.Num());

	for (const TArray<FBRInputAction>* List : { &NativeInputActions, &AbilityInputActions })
	{
		for (const FBRInputAction& Row : *List)
		{
			if (!Row.InputAction.IsNull())
			{
				OutPaths.AddUnique(Row.InputAction.ToSoftObjectPath());
			}
		}
	}
}

bool UBRInputConfig::AreInputActionsLoaded() const
{
	for (const TArray<FBRInputAction>* List : { &NativeInputActions, &AbilityInputActions })
	{
		for (const FBRInputAction& Row : *List)
		{
			if (!Row.InputAction.IsNull() && !Row.InputAction.IsValid())
			{
				return false;
			}
		}
	}

	return true;
}

TSharedPtr<FStreamableHandle> UBRInputConfig::PreloadInputActionsAsync(const FStreamableDelegate& OnLoaded) const
{
	TArray<FSoftObjectPath> Paths;
	GetAllInputActionPaths(Paths);

	if (Paths.IsEmpty() || !UAssetManager::IsInitialized())
	{
		OnLoaded.ExecuteIfBound();
		return nullptr;
	}

	return UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths), OnLoaded, FStreamableManager::DefaultAsyncLoadPriority, false, false, GetName());
}

#if WITH_EDITOR
EDataValidationResult UBRInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	const FGameplayTag InputTagRoot = UGameplayTagsManager::Get().RequestGameplayTag(FName("InputTag"), false);

	TSet<FGameplayTag> SeenTags;

	auto ValidateList = [&](const TArray<FBRInputAction>& Rows, const TCHAR* ListName)
	{
		TSet<FGameplayTag> TagsInThisList;

		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const FBRInputAction& Row = Rows[Index];

			if (Row.InputAction.IsNull())
			{
				Context.AddError(FText::Format(LOCTEXT("NullAction", "{0} row {1} names no InputAction asset."), FText::FromString(ListName), Index));
				Result = EDataValidationResult::Invalid;
			}

			if (!Row.InputTag.IsValid())
			{
				Context.AddError(FText::Format(LOCTEXT("NoTag", "{0} row {1} carries no InputTag. A row without a tag can never be bound."), FText::FromString(ListName), Index));
				Result = EDataValidationResult::Invalid;
				continue;
			}

			if (InputTagRoot.IsValid() && !Row.InputTag.MatchesTag(InputTagRoot))
			{
				Context.AddError(FText::Format(LOCTEXT("WrongFamily", "{0} row {1} carries '{2}', which is not an InputTag.* tag."), FText::FromString(ListName), Index, FText::FromString(Row.InputTag.ToString())));
				Result = EDataValidationResult::Invalid;
			}

			if (TagsInThisList.Contains(Row.InputTag))
			{
				Context.AddError(FText::Format(LOCTEXT("DuplicateInList", "{0} list carries '{1}' more than once; the second row is unreachable by lookup."), FText::FromString(ListName), FText::FromString(Row.InputTag.ToString())));
				Result = EDataValidationResult::Invalid;
			}
			TagsInThisList.Add(Row.InputTag);

			if (SeenTags.Contains(Row.InputTag))
			{
				Context.AddError(FText::Format(LOCTEXT("DuplicateAcrossLists", "'{0}' appears in both the native and ability lists. A tag is either a pawn verb or an ability verb, never both."), FText::FromString(Row.InputTag.ToString())));
				Result = EDataValidationResult::Invalid;
			}
			SeenTags.Add(Row.InputTag);
		}
	};

	ValidateList(NativeInputActions, TEXT("Native"));
	ValidateList(AbilityInputActions, TEXT("Ability"));

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
