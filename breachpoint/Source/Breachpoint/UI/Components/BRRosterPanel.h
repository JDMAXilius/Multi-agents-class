#pragma once

#include "Blueprint/UserWidgetPool.h"
#include "CommonUserWidget.h"
#include "CommonUserWidget.h"
#include "UI/BRUITypes.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRRosterPanel.generated.h"

class UBRHairlineBorder;
class UCommonTextBlock;
class UImage;
class UOverlay;
class USizeBox;
class UTexture2D;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;

/**
 * COMPONENT-SPECS Sec 4: the microphone glyph is "(Mic / Speaking / Muted x White / Black)".
 * The White/Black half of that axis is NOT a state -- it is the text-tone flip below, applied as
 * a tint, so this enum is only the three real states plus two honest absences.
 *
 * The order IS the authoring contract: `MicSwitcher`'s child index is this enum cast to int32,
 * the same trick `UBRMenuRow::TypeSwitcher` uses to keep an axis from becoming N widget classes.
 */
UENUM(BlueprintType)
enum class EBRRosterMicState : uint8
{
	/**
	 * Nothing has been pushed yet. `ue5-ui-architecture` Sec 7: a roster row that renders
	 * "not speaking" before voice state has arrived is telling the player something false.
	 */
	Unknown,

	/** Known to have no voice input at all. Distinct from Unknown on purpose. */
	None,

	/** COMPONENT-SPECS Sec 4 "Mic": connected, idle. */
	Idle,

	/** COMPONENT-SPECS Sec 4 "Speaking". */
	Speaking,

	/** COMPONENT-SPECS Sec 4 "Muted". */
	Muted
};

/**
 * COMPONENT-SPECS Sec 4: "`Text Color` axis is `Black` | `White` -- the gamertag flips against the
 * team fill's luminance."
 *
 * This is a COMPUTED state, never a hardcoded pair: `UBRRosterRow::ComputeTextTone` derives it
 * from whatever fill was pushed, so a new team colour cannot ship with unreadable text because
 * somebody forgot to also flip a bool.
 */
UENUM(BlueprintType)
enum class EBRRosterTextTone : uint8
{
	/** Fill reads dark -- text and glyphs take `TextPrimary` (white). */
	OnDark,

	/** Fill reads light -- text and glyphs take `TextInverted` (black). */
	OnLight
};

/**
 * One roster line, as the UI sees it. PUSH-ONLY VIEW DATA: every field arrives through
 * `UBRRosterPanel::SetMembers`, and nothing in this family ever reads a `PlayerState`, an ASC,
 * a `PlayerController` or a session.
 *
 * WHY THIS STRUCT EXISTS INSTEAD OF A VIEWMODEL: there is no lobby/roster ViewModel. `UBRVM_Combat`
 * and `UBRVM_Match` are in-match only and neither carries a member list. The roster ViewModel is
 * BP24. Until it lands, the panel's feed is this struct and the gaps are listed in the class
 * comment on `UBRRosterPanel` -- they are C++ gaps to file, not things this widget invents.
 */
USTRUCT(BlueprintType)
struct FBRRosterMemberView
{
	GENERATED_BODY()

	/** Empty renders as the unknown dash, never as a blank line (a blank line reads as "nobody"). */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FText Gamertag;

	/**
	 * COMPONENT-SPECS Sec 4 `Team Fill`: "per-player colour / emblem art".
	 *
	 * The colour is PUSHED rather than resolved from a token because there is no team-colour
	 * token: `EBRUIColorToken` is white/black plus the five measured panel-ground alphas, and
	 * COMPONENT-SPECS Sec 8's accent list has no team entry. Inventing one here would fork the
	 * palette (a `contract_gap` for the styles lane owns `UBRUISettings`, not this file).
	 *
	 * Transparent is the honest default: it composites to the panel ground and the text tone
	 * computes to `OnDark`, which is correct over a dark panel.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FLinearColor TeamFillColor = FLinearColor::Transparent;

	/** Law 3: asset refs in data are SOFT. Null soft ptr collapses the emblem slot. */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	TSoftObjectPtr<UTexture2D> Emblem;

	/** COMPONENT-SPECS Sec 4 `Rank`: the 26x26 insignia inside the 30x26 frame. Soft, may be null. */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	TSoftObjectPtr<UTexture2D> RankInsignia;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	EBRRosterMicState MicState = EBRRosterMicState::Unknown;

	/** COMPONENT-SPECS Sec 4 `External Icons` -> `Party Leader Frame`. */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	bool bIsPartyLeader = false;

	/**
	 * COMPONENT-SPECS Sec 4 `External Icons` -> `Current Player Icon`. PUSHED, not derived: the
	 * "is this me" test is a `PlayerState` comparison and this widget is forbidden from making it.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	bool bIsLocalPlayer = false;
};

/**
 * `UBRRosterHeader` -- COMPONENT-SPECS Sec 6 `Party List`, child node `0:671 'Roster Group Header'`.
 *
 * 317 x 31 at (16, 16) inside the 349-wide panel. White fill, dark text -- i.e. it is permanently
 * in the inversion COMPONENT-SPECS Sec 1 describes for a hovered row, which is why it reuses the
 * same two tokens rather than declaring a second white.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRRosterHeader : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Measured: `Tools/gen_ui/figma_geometry.json` UBRRosterPanel.children.header (node 0:671). */
	static constexpr float HeaderHeight = 31.0f;

	/**
	 * Push the group label and, optionally, an occupancy count.
	 *
	 * Negative `InMemberCount` or `InCapacity` means UNKNOWN and renders the dash. Capacity is a
	 * PARAMETER and never a constant in this file: fireteam size is a gameplay number and lives in
	 * a CSV under `Content/Data/` (law 3), not in a widget.
	 * (Written this way on purpose: the glob form opens a nested comment inside this
	 * block, and the module builds with -Werror,-Wcomment. It cost two builds - the
	 * first fix explained the problem using the very sequence that causes it.)
	 *
	 * The defaults are literal -1 rather than INDEX_NONE because UHT copies a UFUNCTION default
	 * argument verbatim into generated code, and this packet cannot compile to prove it resolves
	 * the macro. Both mean the same thing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetHeader(const FText& InLabel, int32 InMemberCount = -1, int32 InCapacity = -1);

	/** Explicit honest-unknown state. Called on construct and whenever the feed goes away. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearToUnknown();

protected:
	virtual void NativeOnInitialized() override;

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_RosterHeader`. Figma layer -> UMG name:
	//   `Roster Group Header` -> (this widget)   `Label` -> Label   `Count` -> Count
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/** The solid white plate. Tinted from a token so no hex reaches the WBP. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Ground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Count;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken GroundToken = EBRUIColorToken::SurfaceInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken TextToken = EBRUIColorToken::TextInverted;
};

/**
 * `UBRRosterRow` -- COMPONENT-SPECS Sec 4 `Player Buttons`.
 *
 * BASE: `UCommonUserWidget`, DEMOTED from `UCommonButtonBase` (CPP-AUDIT PKT-D). The button base
 * was aspiration wearing a class: the row overrode no Native hook, sat in no button group, and
 * rendered no focus state -- so a gamepad could land on a "button" that responded with nothing,
 * which is a live routing defect, not a future feature. The reference component IS named "Player
 * Buttons" and the lobby variant IS the mute / view-profile / kick target -- so the promotion
 * back to `UCommonButtonBase` happens in BP24, together with the hover/focus treatment
 * (`UBRMenuRow::ApplyInvertedState` is the template), a `UCommonButtonGroupBase` on the panel,
 * and the click that gives the base class a job.
 *
 * THIS ROW NEVER RAISES INTENT. It has no click handler and no delegate to a controller: when the
 * lobby needs "mute this player", that intent goes through the owning `PlayerController`'s UI
 * interface (BP24 / netcode-builder), never from here.
 *
 * WIDTH: the row fills its container. It does not pin itself to 317, 349 or 390 -- see the width
 * note on `UBRRosterPanel`.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRRosterRow : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Measured: figma_geometry.json UBRRosterPanel.children.row (node 0:687). */
	static constexpr float RowHeight = 30.0f;

	/** Measured: pitch 35 on a 30-tall row, so the gap between rows is 5. */
	static constexpr float RowPitch = 35.0f;
	static constexpr float RowGap = RowPitch - RowHeight;

	/** COMPONENT-SPECS Sec 4: `Team Fill` and `Content` both sit at (2,2), i.e. a 2px inset. */
	static constexpr float ContentInset = 2.0f;

	/**
	 * COMPONENT-SPECS Sec 4 anatomy (content gap/padding, emblem, rank frame, mic, external
	 * icons) lives in `MCP-BUILD-PLANS.md` Sec A5 with the rest of the WBP measurements —
	 * CPP-AUDIT cut the thirteen constants that mirrored it here unread. What survives below is
	 * what code actually consumes.
	 */

	/** COMPONENT-SPECS Sec 4 `External Icons`: the overlap arithmetic, consumed by
	 *  `ApplyExternalIconLayout`. */
	static constexpr float PartyLeaderIconSize = 30.0f;

	/**
	 * THE -5. COMPONENT-SPECS Sec 4 measures the `External Icons` auto-layout gap as **-5**: a
	 * deliberate overlap of the current-player icon onto the party-leader frame.
	 *
	 * A NEGATIVE GAP HAS NO `UHorizontalBox` EQUIVALENT -- an HBox lays children out end to end and
	 * cannot make two children share pixels, and a negative `FMargin` on an HBox slot shrinks the
	 * slot rather than moving the child backwards over its sibling. So `ExternalIcons` is a
	 * `UOverlay`, and C++ places the second icon at an absolute left offset inside it:
	 *
	 *     CurrentPlayerIconOffsetX = PartyLeaderIconSize + ExternalIconsGap = 30 + (-5) = 25
	 *
	 * which reproduces "gap -5" exactly, in the correct direction, with the overlap visible.
	 * Driven from C++ so the sign cannot be silently lost in a details panel.
	 *
	 * UNVERIFIED (figma_geometry.json `_pending_measurement.UBRRosterRow._verify` item 1): the sign
	 * is a doc claim, and whether it is a gap or a negative right-padding is still open. If the
	 * re-measure says "negative right padding", only this one constant changes.
	 */
	static constexpr float ExternalIconsGap = -5.0f;
	static constexpr float CurrentPlayerIconOffsetX = PartyLeaderIconSize + ExternalIconsGap;

	/** COMPONENT-SPECS Sec 4: the row `Border` FRAME is the whole four-line set at opacity 0.3. */
	static constexpr float BorderOpacity = 0.3f;

	/**
	 * COMPONENT-SPECS Sec 4 `Rank`: fill `#ffffff@0.5`. There is no white@0.5 token, so this is
	 * the full-white `SurfaceInverted` token composited at a measured render opacity -- the same
	 * thing Figma's own layer opacity does. NOT a palette fork and NOT a hex literal; if the styles
	 * lane ever adds a white@0.5 token this collapses to a token swap.
	 */
	static constexpr float RankFrameFillOpacity = 0.5f;

	/**
	 * The luminance at which the text tone flips. 0.5 on `FLinearColor::GetLuminance()`
	 * (0.3R + 0.59G + 0.11B), composited against a dark panel ground -- see `ComputeTextTone`.
	 */
	static constexpr float TextToneLuminanceThreshold = 0.5f;

	/** The whole feed. Push-only; there is no getter into game state anywhere in this family. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetMember(const FBRRosterMemberView& InMember);

	/** Honest unknown: dash for the name, no emblem, no rank, no mic verdict, no icons. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearToUnknown();


	/**
	 * The computed half of COMPONENT-SPECS Sec 4's `Text Color` axis.
	 *
	 * Alpha is part of the sum: a fill at alpha 0.3 is mostly the dark panel ground showing
	 * through, so the effective luminance is the fill's luminance scaled by its alpha (the ground
	 * is `PanelGround40` over a dark gradient -- near enough to black to composite against).
	 * Static and pure so it is testable without a widget tree.
	 */
	static EBRRosterTextTone ComputeTextTone(const FLinearColor& InFillColor);

protected:
	virtual void NativeOnInitialized() override;

	void ApplyMember();
	void ApplyTextTone();
	void ApplyExternalIconLayout();

	static FText GetUnknownValueText();

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_RosterRow`. Figma layer -> UMG name:
	//   `Team Fill`            -> TeamFill          `Border`               -> Border
	//   `Content`              -> Content           `Emblem`               -> Emblem
	//   `Gamertag`             -> Gamertag          `Rank`                 -> RankFrame
	//   `Rank`/`insignia`      -> RankInsignia      `Microphone`           -> MicSwitcher
	//   `External Icons`       -> ExternalIcons     `Party Leader Frame`   -> PartyLeaderIcon
	//   `Current Player Icon`  -> CurrentPlayerIcon
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/** The per-player colour plate. Tinted from the pushed struct, never from a literal. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> TeamFill;

	/** COMPONENT-SPECS Sec 4: four lines, whole frame at opacity 0.3. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Border;

	/** The horizontal content strip. C++ does not build it; it reads its geometry from above. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> Content;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Emblem;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Gamertag;

	/** The 30x26 plate behind the insignia -- white at `RankFrameFillOpacity`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> RankFrame;

	/** COMPONENT-SPECS Sec 7 puts the "Medal 3D" effect stack on this image. Art, not C++. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> RankInsignia;

	/** One child per `EBRRosterMicState`, in enum order. Index is the enum cast to int32. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidgetSwitcher> MicSwitcher;

	/** MUST be a `UOverlay`, not a `UHorizontalBox` -- see `ExternalIconsGap`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UOverlay> ExternalIcons;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> PartyLeaderIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> CurrentPlayerIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken RankFrameFillToken = EBRUIColorToken::SurfaceInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken ToneOnDarkToken = EBRUIColorToken::TextPrimary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken ToneOnLightToken = EBRUIColorToken::TextInverted;

private:
	UPROPERTY(Transient)
	FBRRosterMemberView Member;

	/** Starts UNKNOWN-safe: nothing pushed means the dash, never a confident empty gamertag. */
	UPROPERTY(Transient)
	EBRUIDataState DataState = EBRUIDataState::Unknown;

	UPROPERTY(Transient)
	EBRRosterTextTone TextTone = EBRRosterTextTone::OnDark;
};

/**
 * `UBRRosterPanel` -- COMPONENT-SPECS Sec 6 `Party List`, measured node `527:3515`.
 *
 * 349 x 273 at (862, 397). Fill linear gradient @0.5; background BOOLEAN `#000000@0.4` inset 3;
 * stroke `#ffffff@0.2` 1px align INSIDE. Header at (16,16) 317x31; first row at y=52, pitch 35.
 *
 * ANCHOR RIGHT, AND THIS IS THE WHOLE ULTRAWIDE STORY
 * --------------------------------------------------
 * 862 + 349 = 1211, and 1280 - 1211 = 69 -- exactly the grid's side margin. This is column 3 of the
 * 3-column grid (origin 863; the 1px difference is authoring drift, not a second grid). Anchored to
 * the RIGHT edge in `WBP_RosterPanel`'s canvas slot, the panel stays 69 from the right on any
 * aspect ratio with zero extra code, and the 3D subject in the middle gets the extra width. Anchor
 * it LEFT or CENTER and ultrawide silently breaks. `PanelRightMargin` below is static_asserted
 * against the measured origin so the relationship cannot be edited apart.
 *
 * THE 349 / 310 WIDTH CONFLICT, UNRESOLVED
 * ---------------------------------------
 * 349 is measured (main menu, node 527:3515). 310 is an unmeasured claim for the lobby variant.
 * 390 is the STANDALONE `Player Buttons` art-board and is not an in-screen width at all. This class
 * ships 349 and exposes `PanelWidth` as a layout property, so the lobby is a VALUE CHANGE on one
 * instance -- not a second class, not a second WBP, not a forked row. Content width is DERIVED
 * (`PanelWidth - 2 * ContentInset`), so 349 -> 317 and 310 -> 278 with nothing else touched.
 *
 * PUSH-ONLY, AND STARTS UNKNOWN (`ue5-ui-architecture` Sec 7)
 * ---------------------------------------------------------
 * There is NO roster ViewModel. `UBRVM_Combat` and `UBRVM_Match` are in-match only and carry no
 * member list; the roster VM is BP24. So this panel has no ViewModel binding, no delegate, no
 * timer and no Tick: it renders exactly what `SetMembers` last pushed, and until something pushes
 * it sits in `EBRUIDataState::Unknown` showing dashes. It NEVER touches `PlayerState`, an ASC, a
 * `PlayerController` or a session -- which is also why it is correct during travel and during the
 * frames after a mid-match join when all three of those are null.
 *
 * FIELDS NO VIEWMODEL PROVIDES (C++ gaps to file, listed here so they cannot be quietly faked):
 * gamertag, team id / team colour, emblem, rank + insignia, voice state, party-leader flag,
 * is-local-player flag, group label, occupancy count, per-member connection state.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRRosterPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBRRosterPanel(const FObjectInitializer& ObjectInitializer);

	/** COMPONENT-SPECS Sec 6 / figma_geometry.json node 527:3515: the MEASURED main-menu width.
	 *  The lobby's claimed 310 is UNMEASURED and lives in the DECIDE ledger, not here. */
	static constexpr float PanelWidthMainMenu = 349.0f;

	static constexpr float PanelHeight = 273.0f;

	/** Measured origin on the 1280x720 design canvas. */
	static constexpr float PanelOriginX = 862.0f;
	static constexpr float PanelOriginY = 397.0f;
	static constexpr float DesignCanvasWidth = 1280.0f;

	/** The grid side margin. The reason this panel anchors RIGHT. */
	static constexpr float PanelRightMargin = 69.0f;

	/** COMPONENT-SPECS Sec 6: "Background boolean `#000000@0.4` inset 3". */
	static constexpr float BackgroundInset = 3.0f;

	/** Measured: header and first row both start 16 in from the left; 349 - 2*16 = 317. */
	static constexpr float ContentInset = 16.0f;

	/** Measured: header at y=16 (h31), first row at y=52. */
	static constexpr float HeaderY = 16.0f;
	static constexpr float FirstRowY = 52.0f;

	/** COMPONENT-SPECS Sec 6: the panel's own gradient fill sits at 0.5. */
	static constexpr float GradientOpacity = 0.5f;

	/**
	 * COMPONENT-SPECS Sec 6: stroke `#ffffff@0.2`. There is no white@0.2 token (`ChromeStrokeDim`
	 * is white@0.3), so the full-white token is composited at this measured opacity -- see the
	 * identical note on `UBRRosterRow::RankFrameFillOpacity`. One number, one place, no hex.
	 */
	static constexpr float StrokeOpacity = 0.2f;

	/**
	 * (273 - 52) / 35 = 6.3 -> six rows fit inside the measured panel. A 4v4 fireteam is four, so
	 * this is headroom, not a squeeze -- but overflow is DROPPED AND LOGGED rather than silently
	 * clipped (`ue5-ui-architecture` Sec 5: a silent cap reads as "covered everything" when it
	 * did not).
	 */
	static constexpr int32 MaxVisibleRows = 6;

	/**
	 * THE feed. Pushing an empty array is a LIVE statement ("this roster is empty"), which renders
	 * differently from never having been pushed at all ("we do not know yet") -- see
	 * `ClearToUnknown`. Collapsing those two is the exact join-in-progress lie
	 * `ue5-ui-architecture` Sec 7 forbids.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetMembers(const TArray<FBRRosterMemberView>& InMembers);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetHeaderText(const FText& InLabel, int32 InCapacity = -1);

	/** Back to honest unknown. Called on construct, and on every travel / feed teardown. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearToUnknown();

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRUIDataState GetDataState() const { return DataState; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	void ApplyPanelGeometry();
	void RebuildRows();
	void ApplyEmptyState();

	static FText GetUnknownValueText();

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_RosterPanel`. Figma layer -> UMG name:
	//   `Party List`  -> (this widget)         fill gradient  -> GradientFill
	//   Background BOOLEAN -> Ground           stroke         -> PanelBorder
	//   `Roster Group Header` -> Header        the row column -> RowContainer
	//
	// `Ground` should sit in an Overlay slot: C++ pushes the measured inset-3 into that slot, so
	// the 3 is not retyped in the WBP.
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/**
	 * COMPONENT-SPECS Sec 6: "fill linear gradient @0.5". A gradient is Tier 4 (a material /
	 * brush), so C++ owns only the measured 0.5 and never the gradient itself.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> GradientFill;

	/** The `#000000@0.4` boolean background, inset 3. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Ground;

	/**
	 * The 1px `#ffffff@0.2` stroke. Unlike a menu row's border this IS a closed box: all four
	 * edges, full length, uniform.
	 *
	 * STROKE ALIGNMENT IS UNEXPRESSIBLE TODAY: the measurement says align INSIDE, and
	 * `FBRHairlineStyle` has no alignment field (it draws CENTER). On a 1px stroke that is a 0.5px
	 * offset. Filed as a `contract_gap` against `BRHairlineBorder.h` -- not worked around here, and
	 * not this packet's file to edit. COMPONENT-SPECS Sec 3 needs the same field for the nav tab's
	 * 3px OUTSIDE stroke, so it is a shared gap and not a roster-only one.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> PanelBorder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRRosterHeader> Header;

	/** A `UVerticalBox` so C++ can drive the measured 35 pitch through slot padding. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UVerticalBox> RowContainer;

	/**
	 * The honest empty state. Renders the dash while `Unknown` and a "nobody here" line when the
	 * feed says LIVE-but-empty. Two different sentences, because they are two different facts.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> EmptyStateLabel;

	/**
	 * Law 3: SOFT. Set on the WBP's defaults. This belongs on `UBRUISettings` next to
	 * `KillfeedEntryClass`, but `UI/BRUISettings.h` is not this packet's owner path -- filed as a
	 * `contract_gap`, and the move is a one-line change here when it lands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	TSoftClassPtr<UBRRosterRow> RowWidgetClass;

	/** COMPONENT-SPECS Sec 6: the `#000000@0.4` ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken GroundToken = EBRUIColorToken::PanelGround40;

	/** Full white, composited to 0.2 by `StrokeOpacity`. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken StrokeToken = EBRUIColorToken::ChromeStroke;

	/** THE 349/310 knob. Defaults to the measured main-menu width. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI", meta = (ClampMin = "1.0"))
	float PanelWidth = PanelWidthMainMenu;

private:
	UPROPERTY(Transient)
	TArray<FBRRosterMemberView> Members;

	UPROPERTY(Transient)
	EBRUIDataState DataState = EBRUIDataState::Unknown;

	/** No per-frame widget churn: rows are claimed and released, never created per rebuild. */
	UPROPERTY(Transient)
	FUserWidgetPool RowPool;

	UPROPERTY(Transient)
	TSubclassOf<UBRRosterRow> ResolvedRowClass;

	UPROPERTY(Transient)
	FText HeaderLabel;

	UPROPERTY(Transient)
	int32 HeaderCapacity = INDEX_NONE;
};

// The measured numbers are not independent -- these assert the relationships between them, so
// editing one without the others fails the build instead of shipping a silent 1px drift. They sit
// at file scope rather than inside the UCLASS body deliberately: UHT parses class bodies, and this
// packet cannot run a compile to prove it tolerates a class-scope static_assert.
static_assert(
	UBRRosterPanel::PanelOriginX + UBRRosterPanel::PanelWidthMainMenu + UBRRosterPanel::PanelRightMargin
		== UBRRosterPanel::DesignCanvasWidth,
	"Party List no longer lands 69 from the right edge -- the RIGHT anchor is what makes ultrawide "
	"correct for free, so re-derive it before changing any of these four numbers.");

static_assert(
	UBRRosterPanel::HeaderY + UBRRosterHeader::HeaderHeight + UBRRosterRow::RowGap
		== UBRRosterPanel::FirstRowY,
	"Header top (16) + header height (31) + row gap (5) must equal the measured first row y (52).");

static_assert(
	UBRRosterRow::RowPitch - UBRRosterRow::RowHeight == UBRRosterRow::RowGap,
	"Row pitch 35 on a 30-tall row is a 5px gap.");

static_assert(
	UBRRosterRow::PartyLeaderIconSize + UBRRosterRow::ExternalIconsGap
		== UBRRosterRow::CurrentPlayerIconOffsetX,
	"The External Icons gap is NEGATIVE (-5) -- the current-player icon overlaps the party-leader "
	"frame. If this ever goes non-negative the UOverlay can become a UHorizontalBox.");
