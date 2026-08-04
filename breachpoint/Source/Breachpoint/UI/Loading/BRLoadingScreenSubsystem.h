#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"

#include "BRLoadingScreenSubsystem.generated.h"

class SWidget;
class UUserWidget;
class UWorld;

/**
 * `UBRLoadingScreenSubsystem` -- covers the seam between two maps, and any wait after it.
 *
 * TWO MECHANISMS, BECAUSE THERE ARE TWO PROBLEMS, and conflating them is why loading screens
 * usually only half work:
 *
 *   1. THE BLOCKING LOAD. While UE is loading a map the game thread is not running and no UMG
 *      widget will draw. The only thing that renders is `IGameMoviePlayer`, which runs on its own
 *      thread. So `PreLoadMap` hands the widget to the movie player and that is what the player
 *      sees during the load itself.
 *
 *   2. THE WAIT AFTER THE LOAD. The map is loaded but the match is not ready -- the player state
 *      has not replicated, the pawn has not possessed, the other clients are still joining. The
 *      movie player is gone by now (it auto-completes when loading completes). This phase is
 *      covered by the same widget added to the viewport at a high Z order.
 *
 * HOLDS ARE NAMED AND EXPLICIT, AND NOTHING POLLS. Gameplay calls `AddLoadingReason(FName)` and
 * later `RemoveLoadingReason(FName)`. The screen is up exactly while at least one reason is
 * outstanding. There is no per-frame "should I still be showing?" check anywhere in this file --
 * that would be a gameplay tick (law 4) and, worse, it would put the decision in the subsystem
 * rather than in the code that actually knows.
 *
 * THE TIMEOUT IS THE INTERESTING PART. A reason that is never removed is a soft-lock with no
 * input and no way out, and it is the single most likely defect in this feature. A timer force-
 * releases every outstanding hold after `HoldTimeoutSeconds` and logs a warning naming them, so
 * the failure is a visible bug instead of a frozen client.
 *
 * NOT CREATED ON A DEDICATED SERVER. There is no viewport, no movie player and nothing to show.
 */
UCLASS()
class BREACHPOINT_API UBRLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UBRLoadingScreenSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/**
	 * Keep the loading screen up until `Reason` is removed. Idempotent -- adding the same reason
	 * twice still needs one removal, because a caller that adds in a retry loop must not need a
	 * matching number of removals to escape.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Loading")
	void AddLoadingReason(FName Reason);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Loading")
	void RemoveLoadingReason(FName Reason);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Loading")
	bool IsLoadingScreenVisible() const { return ViewportWidget.IsValid(); }

	/** The reasons currently holding the screen up. For the debug HUD and the spec. */
	void GetOutstandingReasons(TArray<FName>& OutReasons) const;

private:
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** Resolve and instantiate the configured widget. Null when none is configured. */
	UUserWidget* CreateLoadingWidget();

	void ShowViewportWidget();
	void HideViewportWidget();

	/** Re-evaluate whether the screen should be up. The ONE place visibility is decided. */
	void UpdateVisibility();

	void HandleMinimumDisplayElapsed();
	void HandleHoldTimeout();

	/** The reason the map-travel path holds with. Reserved; gameplay must not reuse it. */
	static const FName MapTravelReason;

	/** Held while the minimum-display floor is running, so it participates in the same rule. */
	static const FName MinimumDisplayReason;

	UPROPERTY(Transient)
	TSet<FName> OutstandingReasons;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingWidget;

	/**
	 * The Slate widget actually added to the viewport. Held separately from `LoadingWidget`
	 * because removal must pass the SAME `TSharedRef` that was added -- calling `TakeWidget()` a
	 * second time can hand back a different object and the remove silently does nothing, leaving
	 * a loading screen welded over the game.
	 */
	TSharedPtr<SWidget> ViewportWidget;

	FTimerHandle MinimumDisplayTimer;
	FTimerHandle HoldTimeoutTimer;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
};
