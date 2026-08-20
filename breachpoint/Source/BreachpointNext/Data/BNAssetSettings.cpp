#include "Data/BNAssetSettings.h"

#if WITH_EDITOR
#include "AI/BNBotAuthoring.h"
#include "BreachpointNext.h"
#endif

const UBNAssetSettings* UBNAssetSettings::Get()
{
	return GetDefault<UBNAssetSettings>();
}

FName UBNAssetSettings::GetCategoryName() const
{
	return FName(TEXT("Breachpoint Next"));
}

#if WITH_EDITOR
void UBNAssetSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UBNAssetSettings, bRebuildBotAssets)
		&& bRebuildBotAssets)
	{
		// Reset FIRST: the authoring saves packages, and a re-entrant edit notification arriving
		// mid-save must not start a second build on top of the first.
		bRebuildBotAssets = false;
		UE_LOG(LogBN, Log, TEXT("BNAuthoring: rebuild requested from the editor."));
		UE_LOG(LogBN, Log, TEXT("%s"), *UBNBotAuthoring::BuildBotAssets());
	}
}
#endif
