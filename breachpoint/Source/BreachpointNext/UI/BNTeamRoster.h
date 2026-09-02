#pragma once

#include "CommonUserWidget.h"
#include "UI/Components/BRRosterPanel.h"
#include "BNTeamRoster.generated.h"

class UBRRosterHeader;
class UBRRosterRow;
class UBRScrollBar;
class UImage;
class UTexture2D;
class USizeBox;
class UVerticalBox;

/**
 * One team block — a coloured label and the players under it.
 *
 * Measured `21:43056` Contents: each block is 311 x 108 — `Team Label` 311 x 32 at +0, then
 * player rows 311 x 30 at +37 and +72 (so pitch 35, the same as the front end's roster). The
 * SPECTATORS block is 73 tall because it carries one row instead of two; the height falls out
 * of the member count rather than being a second constant.
 */
USTRUCT(BlueprintType)
struct FBNRosterTeam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	FText Name;

	/** The label plate's fill — COBRA red, EAGLE blue, HADES green in the reference. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	FLinearColor Color = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	TArray<FBRRosterMemberView> Members;
};

/**
 * One nameplate and the tone its art demands.
 *
 * WHY THE BOOL. `UBRRosterRow::ComputeTextTone` decides black-or-white from the fill COLOUR,
 * which works when the fill is a flat tint and cannot work when it is a photograph — a texture
 * has no single colour to read. The reference's own sheet shows both cases: dark plates carry
 * white gamertags, light plates carry black ones. So the plate declares its own tone here
 * rather than the row guessing, and `TeamFillColor` becomes the hint that feeds ComputeTextTone
 * instead of a literal tint.
 */
USTRUCT(BlueprintType)
struct FBNRosterPlate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	TSoftObjectPtr<UTexture2D> Texture;

	/** True when the art is light enough that the gamertag has to go black to be read. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BN|UI")
	bool bLightPlate = false;
};

/**
 * `UBNTeamRoster` — the lobby's third column, `Menu in Border` `21:43056`, measured
 * 349 x 599 at (863,38) on `CG_Lobby`.
 *
 * WHY IT REUSES RATHER THAN REBUILDS. The player row here is the SAME 311 x 30 row the front
 * end's roster already draws, so this creates `WBP_RosterRow` instances (emblem, gamertag,
 * rank frame, mic switcher, party-leader icons — all of it) rather than authoring a second
 * row. What is genuinely new is only the team GROUPING: a coloured label plate over each
 * team's members, which no existing class models. That grouping is built in C++ from data,
 * so five teams and two teams both lay out without touching the WBP.
 *
 * `UBRRosterPanel` was NOT reused: it is a fixed 349 x 273 six-row panel with no concept of
 * teams, and stretching it to 599 with grouping would have meant editing a shared component
 * that the front end depends on.
 *
 * THE ROW CLASS IS `UPROPERTY(Config)` ON A `Config = Game` CLASS, deliberately. Four `BR`
 * classes in this project set a soft row/tab class from ini with `EditAnywhere` /
 * `EditDefaultsOnly` instead — so their ini lines are inert and the component silently draws
 * nothing (filed in this ticket). UHT refuses to compile `UPROPERTY(Config)` without
 * `Config = Game`, so declaring it this way makes the compiler enforce what those four missed.
 */
UCLASS(Abstract, Config = Game, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNTeamRoster : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Measured `21:43056` on CG_Lobby. */
	static constexpr float PanelWidth = 349.0f;
	static constexpr float PanelHeight = 599.0f;

	/** Menu List sits at inset 3; Contents at a further 16, so the content column is 311. */
	static constexpr float BorderInset = 3.0f;
	static constexpr float ContentInset = 16.0f;
	static constexpr float ContentWidth = PanelWidth - 2.0f * (BorderInset + ContentInset);

	/** `Roster Group Header` 311 x 31, then the first team block 5 below it (y31 -> y36). */
	static constexpr float HeaderHeight = 31.0f;
	static constexpr float HeaderGap = 5.0f;

	/** `Team Label` 311 x 32; the first member row starts at +37, i.e. a 5 gap. */
	static constexpr float TeamLabelHeight = 32.0f;
	static constexpr float TeamLabelGap = 5.0f;

	/** Rows are 30 tall at pitch 35 — the same numbers `UBRRosterRow` already encodes. */
	static constexpr float RowHeight = UBRRosterRow::RowHeight;
	static constexpr float RowPitch = UBRRosterRow::RowPitch;

	/** Blocks are 113 apart (y36 -> y149) on a 108-tall block, so teams are 5 apart. */
	static constexpr float TeamGap = 5.0f;

	/** `Vertical Scroll Bar` is 13 wide at x351 — `EBRScrollBarWeight::Wide`. */
	static constexpr float ScrollBarWidth = 13.0f;
	static constexpr float ScrollBarX = 351.0f;

	/** The whole feed. Rebuilds the blocks; safe to call whenever the lobby changes. */
	UFUNCTION(BlueprintCallable, Category = "BN|UI")
	void SetTeams(const TArray<FBNRosterTeam>& InTeams);

	UFUNCTION(BlueprintCallable, Category = "BN|UI")
	void SetHeaderText(const FText& InLabel, int32 InCapacity = -1);

protected:
	virtual void NativeOnInitialized() override;

	/** Builds one team's label plate and its member rows into `TeamBox`. */
	void BuildTeam(const FBNRosterTeam& InTeam, bool bIsLast);

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_BNTeamRoster`. Figma layer -> UMG name:
	//   `Menu in Border`      -> RootSizeBox     `Background`/`Rectangle 257` -> Ground
	//   `Roster Group Header` -> Header          `Contents`                   -> TeamBox
	//   `Vertical Scroll Bar` -> ScrollBar
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UImage> Ground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBRRosterHeader> Header;

	/** Ships EMPTY and that is correct — C++ owns every child. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UVerticalBox> TeamBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UBRScrollBar> ScrollBar;

	/** SOFT (law 3) and CONFIG — see the class note on why the specifier matters here. */
	UPROPERTY(Config)
	TSoftClassPtr<UBRRosterRow> RowWidgetClass;

	/**
	 * Per-row art, cycled by row index so no two adjacent players share a plate.
	 *
	 * `Nameplates` has to be applied by hand rather than pushed through the view struct:
	 * `FBRRosterMemberView` carries an `Emblem` and a `RankInsignia` but NO nameplate, and
	 * `UBRRosterRow::ApplyMember` only ever TINTS `TeamFill` — it never gives it a brush. So the
	 * struct cannot express "this row's background is this texture". Adding the field is a
	 * `Source/Breachpoint/` change, outside this packet's owner path, so it is filed and the
	 * brush is set on the row's own `TeamFill` here instead. Delete this the day the struct
	 * grows the field.
	 */
	UPROPERTY(Config)
	TArray<FBNRosterPlate> Nameplates;

	UPROPERTY(Config)
	TArray<TSoftObjectPtr<UTexture2D>> Emblems;

	/** One crest for everyone: this project has no per-player rank, and six different ones
	 *  would be fiction. */
	UPROPERTY(Config)
	TSoftObjectPtr<UTexture2D> RankInsignia;

private:
	/** Runs across ALL teams, so the art cycle does not restart at every team label. */
	int32 RowCounter = 0;

	UPROPERTY(Transient)
	TSubclassOf<UBRRosterRow> ResolvedRowClass;

	UPROPERTY(Transient)
	TArray<FBNRosterTeam> Teams;
};
