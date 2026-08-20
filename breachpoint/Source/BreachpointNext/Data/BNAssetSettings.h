#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BNAssetSettings.generated.h"

class UDataTable;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Breachpoint Next"))
class BREACHPOINTNEXT_API UBNAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UBNAssetSettings* Get();

	virtual FName GetCategoryName() const override;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> WeaponTable;

#if WITH_EDITORONLY_DATA
	/** A BUTTON, not a setting — it resets itself to false the moment it is read, and it is
	 *  Transient so it never lands in an ini. Flipping it rebuilds ST_BNBot and
	 *  DT_BNBotAmbitions through UBNBotAuthoring.
	 *
	 *  It exists because the MCP bridge has no tool that calls a function: 23 toolsets, 309
	 *  tools, and not one of them executes a console command or a line of Python (checked, not
	 *  assumed). Setting a property IS reachable, and UE routes an editor property write through
	 *  PostEditChangeProperty — so a bool is the smallest honest handle the terminal can pull to
	 *  run the authoring without restarting the editor. */
	UPROPERTY(Transient, EditDefaultsOnly, Category = "Bot Authoring")
	bool bRebuildBotAssets = false;
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
