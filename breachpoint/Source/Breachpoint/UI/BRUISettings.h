#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "BRUISettings.generated.h"

class UBRActivatableWidget;
class UBRKillfeedEntryWidget;
class UBRRootLayout;

UCLASS(config = Game, defaultconfig)
class BREACHPOINT_API UBRUISettings : public UObject
{
	GENERATED_BODY()

public:
	static const UBRUISettings& Get()
	{
		return *GetDefault<UBRUISettings>();
	}

	UPROPERTY(config, EditDefaultsOnly, Category = "Layers")
	TSoftClassPtr<UBRRootLayout> RootLayoutClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> HUDLayoutClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> MainMenuScreenClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> DeathOverlayClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRActivatableWidget> CarnageReportClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Screens")
	TSoftClassPtr<UBRKillfeedEntryWidget> KillfeedEntryClass;

	UPROPERTY(config, EditDefaultsOnly, Category = "Killfeed", meta = (ClampMin = "1", ClampMax = "16"))
	int32 KillfeedMaxVisibleEntries = 5;

	UPROPERTY(config, EditDefaultsOnly, Category = "Killfeed", meta = (ClampMin = "0.5"))
	float KillfeedEntryLifetimeSeconds = 6.0f;

	UPROPERTY(config, EditDefaultsOnly, Category = "MVVM")
	FName CombatViewModelContextName = FName("BRCombat");

	UPROPERTY(config, EditDefaultsOnly, Category = "MVVM")
	FName MatchViewModelContextName = FName("BRMatch");
};
