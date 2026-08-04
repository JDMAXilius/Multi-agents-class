#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"

#include "BRLoadingScreenSettings.generated.h"

class UUserWidget;

/**
 * Project settings for the loading screen. Editable under Project Settings > Game > Breachpoint
 * Loading Screen, persisted to `Config/DefaultGame.ini`.
 *
 * A `UDeveloperSettings`, not a `UBRUISettings`-style plain config object, because these are
 * knobs a designer changes and wants to see in the editor UI. The widget class is SOFT (law 3):
 * a hard reference here would pull the entire loading screen's asset chain into the cook root
 * and, worse, into memory during module load.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Breachpoint Loading Screen"))
class BREACHPOINT_API UBRLoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UBRLoadingScreenSettings& Get() { return *GetDefault<UBRLoadingScreenSettings>(); }

	virtual FName GetCategoryName() const override { return FName("Game"); }

	/** The widget shown while loading. Empty means no loading screen at all, which is legal. */
	UPROPERTY(config, EditAnywhere, Category = "Loading Screen")
	TSoftClassPtr<UUserWidget> LoadingScreenWidgetClass;

	/**
	 * The screen stays up at least this long once shown.
	 *
	 * NOT COSMETIC PADDING. A map that loads in 200 ms produces a flash of loading screen that
	 * reads as a graphical glitch, and on a listen server the host finishes loading well before
	 * the clients do. A floor means the transition looks the same either way.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float MinimumDisplayTimeSeconds = 2.0f;

	/**
	 * A hold is force-released after this long, and the release is logged as a warning naming the
	 * reason that leaked.
	 *
	 * This exists because the failure mode it prevents is unrecoverable: one `AddLoadingReason`
	 * whose matching `RemoveLoadingReason` never runs -- an early return, a failed join, an actor
	 * destroyed mid-handshake -- leaves the player staring at a loading screen with no input and
	 * no way out. A timeout turns a soft-lock into a visible bug report.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "1.0"))
	float HoldTimeoutSeconds = 30.0f;

	/** Above the HUD, below nothing. The loading screen is the top of the stack by definition. */
	UPROPERTY(config, EditAnywhere, Category = "Loading Screen")
	int32 ViewportZOrder = 1000;
};
