#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AIBAssetSettings.generated.h"

/**
 * The module's editor-facing settings surface. Today it exists for exactly one thing:
 * the authoring trigger the MCP bridge can pull.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "AI Bot"))
class AIBOT_API UAIBAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UAIBAssetSettings* Get();

	virtual FName GetCategoryName() const override;

#if WITH_EDITORONLY_DATA
	/** A BUTTON, not a setting — it resets itself to false the moment it is read, and it
	 *  is Transient so it never lands in an ini. Flipping it rebuilds ST_AIBBot and
	 *  DT_AIBTiers through UAIBTreeAuthoring.
	 *
	 *  It exists because the MCP bridge has no tool that calls a function (the host
	 *  checked: 23 toolsets, 309 tools, none executes a console command or Python).
	 *  Setting a property IS reachable, and UE routes an editor property write through
	 *  PostEditChangeProperty — so a bool is the smallest honest handle the terminal can
	 *  pull to run the authoring without restarting the editor. */
	UPROPERTY(Transient, EditDefaultsOnly, Category = "Authoring")
	bool bRebuildBotAssets = false;
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
