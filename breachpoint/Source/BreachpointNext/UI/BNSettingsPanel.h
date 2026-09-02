#pragma once

#include "CommonUserWidget.h"
#include "BNSettingsPanel.generated.h"

class UCommonTextBlock;
class UImage;
class UPanelWidget;
class USizeBox;
class UTexture2D;
class UVerticalBox;

/**
 * One label/value line inside a section — "Score to Win", "Bot Count 8".
 *
 * Measured `21:43050` `Details`/`List`: rows are 18 tall at pitch 23, label left-aligned at
 * x0, value RIGHT-aligned to the panel's 349 edge. An empty `Value` renders the label alone,
 * which is exactly what the reference's Details list does — it names the settings without
 * asserting numbers the mode has not been asked for yet.
 */
USTRUCT(BlueprintType)
struct FBNSettingRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	FText Value;
};

/**
 * A titled block of rows — "DETAILS", "OVERRIDES".
 *
 * Measured: a `Decorative Line` header (23 tall: a 20-tall label over a 3px underline, plus a
 * full-width hairline with 3x3 end caps) and then the list at +32.
 */
USTRUCT(BlueprintType)
struct FBNSettingSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	FText Header;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	TArray<FBNSettingRow> Rows;
};

/**
 * `UBNSettingsPanel` — the lobby's centre column, `Game Settings Breakdown` `21:43050`,
 * measured 349 x 332 at (466,76) on `CG_Lobby`.
 *
 * WHY IT IS DATA AND NOT WIDGETS. The founder's ask was that the level details be "dynamic, so
 * it's spacing correctly". The reference panel is a gamemode card over N titled sections of
 * label/value rows — a shape, not a fixed set of six labels. So the panel takes
 * `TArray<FBNSettingSection>` and builds the rows itself at the measured pitch; a mode with
 * three settings and a mode with nine both lay out correctly and neither needs a WBP edit.
 *
 * The header is the SAME `UBNSettingRow`-driven list either way, so "DETAILS" and "OVERRIDES"
 * are two entries in one array rather than two hand-placed panels — which is what keeps this
 * one component instead of two.
 *
 * SOFT REFS ONLY (law 3): the gamemode icon arrives as a `TSoftObjectPtr` pushed in, never a
 * hard pointer and never a `ConstructorHelpers` lookup.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNSettingsPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Measured `21:43050` on CG_Lobby. */
	static constexpr float PanelWidth = 349.0f;
	static constexpr float PanelHeight = 332.0f;

	/** `Gamemode Card` 349 x 100; its `Icon Pane` is 60 wide with a 40 x 40 icon inset 10. */
	static constexpr float CardHeight = 100.0f;
	static constexpr float IconPaneWidth = 60.0f;
	static constexpr float IconSize = 40.0f;
	static constexpr float IconInset = 10.0f;

	/** `Frame 105` — the title block — starts at x80, i.e. 20 past the icon pane. */
	static constexpr float CardTextX = 80.0f;

	/** `Decorative Line` is 23 tall and the list starts 32 below the section's top. */
	static constexpr float SectionHeaderHeight = 23.0f;
	static constexpr float SectionListOffsetY = 32.0f;

	/** Rows are 18 tall at pitch 23, so the gap is 5. */
	static constexpr float RowHeight = 18.0f;
	static constexpr float RowPitch = 23.0f;

	/** The gamemode card: icon, version chip, title, description. All optional. */
	UFUNCTION(BlueprintCallable, Category = "BN|UI")
	void SetGamemode(const FText& InTitle, const FText& InDescription,
		const TSoftObjectPtr<UTexture2D>& InIcon, const FText& InVersion);

	/** The whole body. Rebuilds the row widgets; safe to call every time the mode changes. */
	UFUNCTION(BlueprintCallable, Category = "BN|UI")
	void SetSections(const TArray<FBNSettingSection>& InSections);

protected:
	virtual void NativeOnInitialized() override;

	/** Builds one section's header + rows into `SectionBox`. */
	void BuildSection(const FBNSettingSection& InSection);

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_BNSettingsPanel`. Figma layer -> UMG name:
	//   `Gamemode Card`/`CTF` -> ModeIcon      `v1.0`   -> VersionText
	//   `ARENA: ONE FLAG CTF` -> ModeTitle     body     -> ModeDescription
	//   `Details` + `Adjustments` -> SectionBox (C++ fills it; the WBP ships it EMPTY)
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UImage> ModeIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UCommonTextBlock> VersionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UCommonTextBlock> ModeTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UCommonTextBlock> ModeDescription;

	/** Ships EMPTY and that is correct, not unfinished — C++ owns every child. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UVerticalBox> SectionBox;

private:
	UPROPERTY(Transient)
	TArray<FBNSettingSection> Sections;
};
