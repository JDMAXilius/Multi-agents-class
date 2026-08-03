#pragma once

#include "CommonUserWidget.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRPageTitle.generated.h"

class UBRHairlineBorder;
class UCommonTextBlock;
class USizeBox;
class UWidget;

/**
 * REFERENCE-EXTRACTION `Page Title`: variant axis `Type = Drill Down | Single Page` (2 variants).
 * A drill-down title is the one that carries a parent breadcrumb and a back affordance; the back
 * ACTION itself is not here -- back comes from the CommonUI activatable stack, never from a
 * per-screen hack (`ue5-ui-architecture` Sec 1).
 */
UENUM(BlueprintType)
enum class EBRPageTitleType : uint8
{
	SinglePage,

	DrillDown
};

/**
 * REFERENCE-EXTRACTION `Item Title`: variant axis `Rarity = Epic | Legendary | Rare` (3 variants);
 * `Common` is added because COMPONENT-SPECS Sec 5's rarity table has four rows and an item title
 * over a common item must render as something.
 *
 * CONTRACT GAP -- OWNERSHIP: nothing in `Source/Breachpoint/` declares an item rarity today, and
 * `UBRItemTile` (SCREEN-MANIFEST Sec 5 Tier 3) will need the same four values. It is declared here
 * because this is the first component that needs it; the Tier-3 lane must REUSE this enum and the
 * follow-up is to move it to a shared UI types header. Two rarity enums in one module is a defect.
 */
UENUM(BlueprintType)
enum class EBRItemRarity : uint8
{
	Common,

	Rare,

	Epic,

	Legendary
};

/**
 * `UBRPageTitle` -- the 1280 x 75 title band (SCREEN-MANIFEST Sec 5 Tier 1: 15 of 31 screens).
 *
 * WHY THIS CLASS AND `UBRItemTitle` SHARE A FILE, WHICH IS ALSO THE WHOLE POINT OF THE PAIR
 * -----------------------------------------------------------------------------------------
 * SCREEN-MANIFEST Sec 5 lists them separately (75 vs 105) but they are one band with two heights.
 * The 30 px difference is not decoration -- SCREEN-BUILD-SPEC Sec 3.5 and SCREEN-MANIFEST Sec 4.3
 * both say the Item Title REPLACES the Page Title and **everything below shifts by 40 px**, and
 * the nav bar moves with it:
 *
 *     Page Title (75)  ->  nav bar at y = 45   (root level, COMPONENT-SPECS Sec 6, node 124:1179)
 *     Item Title (105) ->  nav bar at y = 110  (sub level; y = 75 is the other authored value)
 *
 * That coupling is the reason both classes exist, and it is why they are one inheritance pair
 * rather than two unrelated widgets: a screen that swaps the title band and forgets the shift is
 * the defect this file is shaped to prevent. `UBRItemTitle` is `UBRPageTitle` plus a height, a
 * rarity treatment and an equipped check -- nothing else.
 *
 * BASE: `UCommonUserWidget`. The title band is chrome inside a screen; the SCREEN is the
 * activatable that pushes, pops and owns back.
 *
 * The reference gives the band's OUTER size only. Its internal offsets (title x/y, breadcrumb
 * position, back-caret box) are NOT measured anywhere in COMPONENT-SPECS or SCREEN-MANIFEST, so
 * none are declared here -- the WBP author places them and the numbers get written down when a
 * node is read. Inventing them here would look like measurement.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRPageTitle : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** SCREEN-BUILD-SPEC Sec 1 / SCREEN-MANIFEST Sec 5: the title band is full-bleed 1280 wide. */
	static constexpr float BandWidth = 1280.0f;

	/** REFERENCE-EXTRACTION `Page Title`: 1280 x 75. */
	static constexpr float BandHeight = 75.0f;

	/** COMPONENT-SPECS Sec 6, node 124:1179: the nav bar sits at y = 45 under a Page Title. */
	static constexpr float NavBarOffsetYUnderPageTitle = 45.0f;

	/**
	 * REFERENCE-EXTRACTION type ramp: PAGE TITLE is 24 px at 10% tracking. UMG Letter Spacing is
	 * 1/1000 em, so 10% is 100. RECORDED, NOT APPLIED -- type comes from the shared text style
	 * asset (SCREEN-MANIFEST Sec 7.4); setting it here would fork the style per screen.
	 */
	static constexpr float TitleFontSizePx = 24.0f;
	static constexpr int32 TitleLetterSpacingPerMille = 100;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetTitleText(const FText& InText);

	/**
	 * The parent level on a drill-down title. Empty COLLAPSES rather than leaving a stale
	 * breadcrumb -- a wrong breadcrumb tells the player they are somewhere they are not.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetBreadcrumbText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetTitleType(EBRPageTitleType InType);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRPageTitleType GetTitleType() const { return TitleType; }

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	/** The one number `UBRItemTitle` changes. Everything else about the band is shared. */
	virtual float GetBandHeight() const { return BandHeight; }

	void ApplyTitleType();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_PageTitle`. Figma layer -> UMG name:
	//   the 1280x75 band root -> RootSizeBox
	//   `Title`               -> Title
	//   the parent level      -> Breadcrumb        (Drill Down only)
	//   the back caret group  -> BackAffordance    (Drill Down only; DECORATION, not the action)
	// ---------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Title;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Breadcrumb;

	/**
	 * The caret/rule that marks a drill-down. It is a decoration: back is handled by the
	 * activatable stack's back action, so this must never be a button.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> BackAffordance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRPageTitleType TitleType = EBRPageTitleType::SinglePage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken TitleTextToken = EBRUIColorToken::TextPrimary;
};

/**
 * `UBRItemTitle` -- the 1280 x 105 title band (SCREEN-MANIFEST Sec 5 Tier 1: 6 of 31 screens).
 *
 * It IS a page title, 30 px taller, carrying the item name plus a rarity tag and an equipped
 * check (REFERENCE-EXTRACTION `Item Title`; SCREEN-BUILD-SPEC Sec 3.5). Read the pair's comment on
 * `UBRPageTitle` before using either: swapping to this class shifts every element below by
 * `ContentShiftPx` and moves the nav bar off y = 45.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRItemTitle : public UBRPageTitle
{
	GENERATED_BODY()

public:
	/** REFERENCE-EXTRACTION `Item Title`: 1280 x 105. */
	static constexpr float ItemBandHeight = 105.0f;

	/**
	 * SCREEN-BUILD-SPEC Sec 3.5 / SCREEN-MANIFEST Sec 4.3: "everything below shifts up 40px".
	 * Note it is 40, not the 30 the two band heights differ by -- the reference shifts content by
	 * 40, so this is a measured layout number and NOT `ItemBandHeight - BandHeight`. Writing it as
	 * that subtraction would silently ship a 10 px error on 6 screens.
	 */
	static constexpr float ContentShiftPx = 40.0f;

	/**
	 * The two authored nav-bar y values under an Item Title. SCREEN-BUILD-SPEC Sec 1 lists both
	 * ("44,75 / 44,110") for the 516-wide sub-level bar; SCREEN-MANIFEST Sec 4.3 uses 110 on
	 * `OP_Item_*`. Both are kept because the sources disagree and neither has a node id --
	 * SCREEN-MANIFEST Sec 9 Q1 explicitly leaves the sub-level bar's origin unverified. The x in
	 * those quotes is the retracted 44; the root bar's confirmed x is `UBRNavBar::BarOriginX`.
	 */
	static constexpr float NavBarOffsetYSubLevel = 110.0f;
	static constexpr float NavBarOffsetYSubLevelAlt = 75.0f;

	/** REFERENCE-EXTRACTION type ramp: Item Title is Regular 48 at 10%. Recorded, not applied. */
	static constexpr float ItemTitleFontSizePx = 48.0f;

	/**
	 * Name plus rarity in one call, so a title can never show one item's name over another
	 * item's rarity. The label is TEXT from data, not derived from the enum: rarity names are
	 * localized content and an enum-to-string here would ship English into 6 screens.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetItem(const FText& InItemName, EBRItemRarity InRarity, const FText& InRarityLabel);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetEquipped(bool bInEquipped);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRItemRarity GetRarity() const { return Rarity; }

	/**
	 * CONTRACT GAP -- MISSING TOKENS. COMPONENT-SPECS Sec 5 gives eight rarity hexes (stroke +
	 * gradient tint for rare/epic/legendary/common) and `BRComponentTokens.h` deliberately carries
	 * NONE of them: its own comment says accent colours are excluded because no Tier-0 component
	 * used one. This is the first component that does. Until the tokens land, every rarity
	 * resolves to `ChromeStroke` -- white -- which is correct for `Common` and visibly wrong for
	 * the other three. That is stated loudly rather than fixed with a hex literal here, which
	 * would fork the palette in exactly the way the tokens header exists to prevent.
	 *
	 * Needed: `RarityCommon/Rare/Epic/Legendary` stroke tokens (and their gradient tints, which
	 * `UBRItemTile` will also need). Filed as a contract_gap against the styles lane.
	 */
	static EBRUIColorToken ResolveRarityToken(EBRItemRarity InRarity);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	virtual float GetBandHeight() const override { return ItemBandHeight; }

	void ApplyRarity();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_ItemTitle`. It ALSO inherits every bind on `UBRPageTitle`
	// (RootSizeBox / Title / Breadcrumb / BackAffordance) -- an item title has a name too.
	//   the rarity tag frame -> RarityTagBorder
	//   the rarity word      -> RarityLabel
	//   the equipped tick    -> EquippedCheck
	// ---------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> RarityTagBorder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> RarityLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> EquippedCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRItemRarity Rarity = EBRItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	bool bEquipped = false;
};
