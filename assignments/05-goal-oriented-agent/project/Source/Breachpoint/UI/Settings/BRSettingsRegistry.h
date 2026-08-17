#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Settings/BRSettingsValueObjects.h"

#include "BRSettingsRegistry.generated.h"

class ULocalPlayer;
class UBRGameUserSettings;
class UBRSettingsCollection;
class UBRSettingsValue_KeyRemap;
class UEnhancedInputUserSettings;

/**
 * `UBRSettingsRegistry` -- builds the settings tree and owns apply / cancel / reset.
 *
 * ONE PLACE WHERE A SETTING IS DECLARED. Adding a setting is one call in one `Build*Tab`
 * function that names its id, its text, its range and its accessors. There is deliberately no
 * data-asset or CSV registration path: an entry has to name a C++ getter and setter, so a row
 * authored outside C++ could only ever be half a setting, and the half it was missing would be
 * the half that does anything.
 *
 * THE SCREEN OWNS NO TREE. It asks for a tab's rows and renders them. Rebuilding the registry
 * does not require a widget to exist, which is what lets `Breachpoint.Sim.Settings` construct one
 * with a null local player and drive the whole model headlessly. In that mode the backing stores
 * are absent, every leaf reports `IsBound() == false`, and the spec asserts on exactly that --
 * an unbound tree is a supported state, not a crash waiting to happen.
 *
 * APPLY IS EXPLICIT. Setters write through to the store immediately (so the slider you are
 * dragging shows the value you are dragging it to), but `ApplyAndSave` is what calls
 * `UGameUserSettings::ApplySettings` and commits to disk. `CancelChanges` walks back to the
 * baselines captured when the screen opened. Anything that changes the render pipeline --
 * resolution, window mode, scalability -- only moves on apply, which is why the two are
 * separate rather than every setter applying.
 */
UCLASS(BlueprintType)
class BREACHPOINT_API UBRSettingsRegistry : public UObject
{
	GENERATED_BODY()

public:
	/** Tab ids. Public so the screen, the spec and any deep link name the same constants. */
	static const FName TabGameplay;
	static const FName TabVideo;
	static const FName TabAudio;
	static const FName TabControls;

	/**
	 * Build the whole tree. `LocalPlayer` may be null -- the tree is still built in full, with
	 * every leaf unbound. That is the headless path and it is a first-class one.
	 *
	 * Safe to call again; the previous tree is dropped and rebuilt.
	 */
	void Build(ULocalPlayer* LocalPlayer);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	bool IsBuilt() const { return Tabs.Num() > 0; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	const TArray<UBRSettingsCollection*>& GetTabs() const { return TabsView; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	UBRSettingsCollection* FindTab(FName TabId) const;

	/** The flattened row list a list view consumes for one tab. */
	void GetRowsForTab(FName TabId, TArray<UBRSettingsDataObject*>& OutRows) const;

	/** Every rebindable row, across every slot. The rebind screen's source. */
	void GetKeyRemapRows(TArray<UBRSettingsValue_KeyRemap*>& OutRows) const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	bool HasUnsavedChanges() const;

	/** Snapshot every leaf as "unchanged". Call when the settings screen opens. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void CaptureBaselines();

	/** Commit: `ApplySettings` then `SaveSettings`, then re-baseline. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void ApplyAndSave();

	/** Walk every leaf back to its baseline. The Cancel button. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void CancelChanges();

	/** Shipped defaults for one tab, or for everything when `TabId` is None. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void ResetToDefaults(FName TabId);

	/** Fired when any leaf anywhere in the tree changed. The screen redraws from this. */
	FBRSettingsValueChanged OnAnySettingChanged;

private:
	void BuildGameplayTab();
	void BuildVideoTab();
	void BuildAudioTab();
	void BuildControlsTab();

	// -- Factories. Each registers the leaf, wires its change delegate and returns it so the
	//    caller can configure anything unusual.

	UBRSettingsCollection* MakeCollection(UBRSettingsCollection* Parent, FName Id, const FText& Name, const FText& Description);

	UBRSettingsValue_Scalar* MakeScalar(
		UBRSettingsCollection* Parent, FName Id, const FText& Name, const FText& Description,
		float Min, float Max, float Step, EBRScalarDisplayFormat Format,
		TFunction<float()> Getter, TFunction<void(float)> Setter);

	UBRSettingsValue_Discrete* MakeDiscrete(
		UBRSettingsCollection* Parent, FName Id, const FText& Name, const FText& Description,
		TArray<FBRSettingsOption> Options,
		TFunction<FName()> Getter, TFunction<void(FName)> Setter);

	/**
	 * A two-option discrete. Exists so "is a bool a class?" is answered once, here, with no --
	 * see the `BRSettingsValueObjects.h` preamble.
	 */
	UBRSettingsValue_Discrete* MakeToggle(
		UBRSettingsCollection* Parent, FName Id, const FText& Name, const FText& Description,
		TFunction<bool()> Getter, TFunction<void(bool)> Setter);

	/** Bind a leaf's change delegate into `HandleLeafChanged` and remember it. */
	void RegisterLeaf(UBRSettingsCollection* Parent, UBRSettingsDataObject* Leaf);

	void HandleLeafChanged(UBRSettingsDataObject* Changed);

	/**
	 * Re-evaluate every cross-setting dependency. Today that is one rule -- a frame rate limit
	 * means nothing while VSync owns the cadence -- but it is a function rather than an inline
	 * check so the second rule has somewhere to go that is not a widget.
	 */
	void RefreshEditStates();

	/** Fill the resolution list from what the display actually reports. */
	void PopulateResolutionOptions();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRSettingsCollection>> Tabs;

	/** Mirrors `Tabs` for the BlueprintCallable getter. See `UBRSettingsCollection::ChildrenView`. */
	UPROPERTY(Transient)
	TArray<UBRSettingsCollection*> TabsView;

	/** Flat index of every leaf, for `HasUnsavedChanges` and the reset/cancel walks. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRSettingsDataObject>> AllLeaves;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRSettingsValue_KeyRemap>> KeyRemapLeaves;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBRGameUserSettings> GameSettings;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputUserSettings> InputSettings;

	/**
	 * Held so `RefreshEditStates` does not have to search the tree by id every time.
	 *
	 * The frame rate limit is DISCRETE, not a slider. The values that matter are a short list of
	 * refresh rates and "unlimited"; a continuous slider would let a player pick 87 fps, which is
	 * not a thing anyone means to do and reads as a broken control.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UBRSettingsValue_Discrete> FrameRateLimitLeaf;

	UPROPERTY(Transient)
	TObjectPtr<UBRSettingsValue_Discrete> VSyncLeaf;

	UPROPERTY(Transient)
	TObjectPtr<UBRSettingsValue_Discrete> ResolutionLeaf;

	/** See the guard note in `RefreshEditStates`. Not a `UPROPERTY` -- it is control flow, not state. */
	bool bRefreshingEditStates = false;
};
