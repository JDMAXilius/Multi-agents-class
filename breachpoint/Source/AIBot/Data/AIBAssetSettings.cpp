#include "Data/AIBAssetSettings.h"

#if WITH_EDITOR
#include "AIBotModule.h"
#include "Execution/AIBTreeAuthoring.h"
#endif

const UAIBAssetSettings* UAIBAssetSettings::Get()
{
	return GetDefault<UAIBAssetSettings>();
}

FName UAIBAssetSettings::GetCategoryName() const
{
	return FName(TEXT("AI Bot"));
}

#if WITH_EDITOR
void UAIBAssetSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAIBAssetSettings, bRebuildBotAssets)
		&& bRebuildBotAssets)
	{
		// Reset FIRST: the authoring saves packages, and a re-entrant edit notification
		// arriving mid-save must not start a second build on top of the first.
		bRebuildBotAssets = false;
		UE_LOG(LogAIBot, Log, TEXT("AIBAuthoring: rebuild requested from the editor."));
		UE_LOG(LogAIBot, Log, TEXT("%s"), *UAIBTreeAuthoring::BuildBotAssets());
	}
}
#endif
