#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UITag.h"
#include "BNUITypes.generated.h"

class UBRButton;
class UTexture2D;

/** The CommonUI layer tags — a SECOND registrar beside BNTags, because it has to be: the stacks
 *  key on CommonUI's FUITag, and BNTags' UE_DEFINE_GAMEPLAY_TAG macros produce plain
 *  FGameplayTags.
 *
 *  THE BODIES ARE ROOT-RELATIVE (critic, R7 W0): FUITag::AddNativeTag PREPENDS its "UI." root,
 *  so TEXT("Layer.Game") registers as UI.Layer.Game — passing a full "UI.Layer.Game" here would
 *  register the garbage UI.UI.Layer.Game. These therefore land on the SAME strings the old
 *  module registers, which is fine and deliberate: same-string native registration dedupes
 *  without complaint (the original collision fear was wrong twice — wrong strings, wrong
 *  hazard). Shape transcribed from the compiled reference (ROADMAP-7 §API). */
struct FBNUITags : public FGameplayTagNativeAdder
{
	FUITag Layer_Game;
	FUITag Layer_GameMenu;
	FUITag Layer_Menu;
	FUITag Layer_Modal;

	virtual void AddTags() override
	{
		Layer_Game     = FUITag::AddNativeTag(TEXT("Layer.Game"));
		Layer_GameMenu = FUITag::AddNativeTag(TEXT("Layer.GameMenu"));
		Layer_Menu     = FUITag::AddNativeTag(TEXT("Layer.Menu"));
		Layer_Modal    = FUITag::AddNativeTag(TEXT("Layer.Modal"));
	}

	static const FBNUITags& Get() { return Singleton; }

private:
	static BREACHPOINTNEXT_API FBNUITags Singleton;
};

/** Presentation timing shared by the ViewModel (expiry) and the director (the join-in-progress
 *  age filter): a joiner must not receive last-few-minutes kills as a burst of fresh lines, so
 *  the director skips ring entries older than this — the same number that would have expired
 *  them had the client been present. */
namespace BNUITiming
{
	inline constexpr double KillfeedLingerSeconds = 6.0;
}

/** Honest-unknown, the doctrine's load-bearing enum: a ViewModel field is Unknown until its
 *  DENOMINATORS are known (MaxHealth landed, the match state read once), and a widget renders
 *  dashes at Unknown — never a confident 0/100. The frames a mid-match joiner sees are the ones
 *  nobody tests; this enum is how they stay honest. */
UENUM()
enum class EBNUIDataState : uint8
{
	Unknown,
	Live,
	Stale
};

/** TEAMS (BN16): who someone is TO THE PLAYER READING THIS SCREEN — the one team fact any
 *  widget ever renders. No absolute team id or team color reaches a widget: the packet's
 *  law is relative presentation (my side blue, their side red, whichever side I am on), so
 *  the director composes this from the two TeamIds and the widget maps relation → tint.
 *  None is FFA AND the honest-unknown window (either TeamId still NoTeam on a joining
 *  client) — a widget renders None exactly as today's FFA colors, which is what makes the
 *  teams-OFF HUD untouched by construction. */
UENUM()
enum class EBNUITeamRelation : uint8
{
	None,
	Self,
	Ally,
	Enemy
};

/** How the match ended FOR THE PLAYER READING THIS SCREEN. Not "who won" — the scoreboard
 *  already carries that. A win screen has to say VICTORY or DEFEAT, and only the director
 *  knows which, because only it knows which PlayerState is mine. Undecided is the live match:
 *  the outcome band is absent, not blank. */
UENUM()
enum class EBNMatchOutcome : uint8
{
	Undecided,
	Victory,
	Defeat,
	Draw
};

/** One killfeed line as the VIEW knows it. The wording is composed ONCE, where the relationship
 *  facts are in hand — the widget renders text and a tint and decides nothing. ExpiryTime is
 *  LOCAL world seconds, stamped on the entry itself (a parallel array desyncs — the old module
 *  proved it), and deliberately not server-synced: how long a line lingers is presentation. */
USTRUCT()
struct FBNKillfeedViewEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FText Line;

	/** The server's monotonic id — the dedupe key when a ring re-replicates. */
	UPROPERTY()
	int32 Sequence = INDEX_NONE;

	UPROPERTY()
	double ExpiryTime = 0.0;

	/** White row: the local player is in this line (the one colour that means "you" in a list). */
	UPROPERTY()
	bool bInvolvesSelf = false;

	/** R7.6, gap 6 — the line's PARTS, so a WBP can lay out [Killer][glyph][Victim] at the
	 *  measured x positions instead of centring one composed string. Both empty for the wordings
	 *  that have no killer ("X died", "X eliminated themselves"); the row falls back to Line,
	 *  which stays the single source for those and for any WBP that binds no part widgets. */
	UPROPERTY()
	FText KillerText;

	UPROPERTY()
	FText VictimText;

	/** R7.3 — the design's 22×8 weapon glyph, resolved by the director from the kill's source
	 *  name. Unset for a cause with no weapon row (melee, a grenade, world damage) and for a row
	 *  whose Icon column is empty: the row then draws the line alone rather than a placeholder.
	 *  The NAME is deliberately not carried here — the feed shows a glyph, the death screen shows
	 *  the words, and a feed line that grows a text weapon stops fitting its measured 340px. */
	UPROPERTY()
	TSoftObjectPtr<UTexture2D> WeaponIcon;

	/** TEAMS (BN16): the two parties' relations to the reader, composed by the director at
	 *  push time (the relationship facts are in hand exactly once — the same reasoning as
	 *  Line). None on both in FFA, which renders today's palette untouched. bInvolvesSelf
	 *  stays the white-row authority; these tint the PARTS. */
	UPROPERTY()
	EBNUITeamRelation KillerRelation = EBNUITeamRelation::None;

	UPROPERTY()
	EBNUITeamRelation VictimRelation = EBNUITeamRelation::None;
};

/** One scoreboard row as the VIEW knows it — built by the director (the one gameplay-aware
 *  file) so the scoreboard widget never reaches into PlayerArray itself. Sorted before it
 *  arrives; the widget renders rows in order and decides nothing. */
USTRUCT()
struct FBNScoreRowView
{
	GENERATED_BODY()

	UPROPERTY()
	FString PlayerName;

	UPROPERTY()
	int32 Kills = 0;

	UPROPERTY()
	int32 Deaths = 0;

	UPROPERTY()
	bool bIsSelf = false;

	UPROPERTY()
	bool bIsWinner = false;

	/** TEAMS (BN16): this row's relation to the reader — the scoreboard's two blocks group
	 *  on it (Self+Ally above, Enemy below) and its name tints from it. None = FFA or the
	 *  joining client's honest-unknown frame: the row sits in today's single block with
	 *  today's colors until the TeamId lands and the deferred subscription rebuilds. */
	UPROPERTY()
	EBNUITeamRelation Relation = EBNUITeamRelation::None;
};

/** The palette, as SEMANTICS — one hue, one meaning, everywhere. C++ constants because a WBP
 *  never types a hex (twelve hand-typed copies is twelve places a rebrand breaks silently and
 *  the critic can diff none of them). Inherited from the old module's VISR-derived system:
 *  red is THREAT and is never spent on a warning that is not lethal; health is yellow, never
 *  green — green reads "fine" at exactly the moment the player is one shot from dead. */
namespace BNUIColors
{
	/** You: your shields, your bars, your reticle at rest. */
	// 0.208, not 0.043 — every other colour in this block is the raw 0-1 fraction of the hex
	// beside it (0x4A -> 0.290, 0xC5 -> 0.773, 0x83 -> 0.514) and this red channel was the one
	// that did not match its own comment. Invisible so far only because shields are off.
	inline const FLinearColor Shield  { 0.208f, 0.816f, 0.949f };  // #35D0F2
	/** Health beneath shields. Yellow, never green. */
	inline const FLinearColor Health  { 0.961f, 0.773f, 0.259f };  // #F5C542
	/** A clock is running: respawn countdown, match clock in its last stretch. */
	inline const FLinearColor Amber   { 1.000f, 0.639f, 0.200f };  // #FFA333
	/** Threat, only: incoming damage, the enemy. Never low-ammo, never UI errors. */
	inline const FLinearColor Threat  { 1.000f, 0.290f, 0.239f };  // #FF4A3D
	/** You, in a list of people: your killfeed lines, your scoreboard row. */
	inline const FLinearColor Self    { 1.000f, 1.000f, 1.000f };  // #FFFFFF

	/** Your SIDE, in a list of people (BN16): ally rows and ally names in the feed. A blue
	 *  deliberately apart from Shield's cyan — Shield means YOUR bars, Ally means people.
	 *  The enemy side has no new hue: red already means THREAT, and the enemy is the one
	 *  thing Threat was always allowed to name (one hue, one meaning — held, not bent). */
	inline const FLinearColor Ally    { 0.290f, 0.608f, 1.000f };  // #4A9BFF

	/** Everyone else's history: killfeed lines you are not in, other rows. */
	inline const FLinearColor InkDim  { 0.514f, 0.592f, 0.663f };  // #8397A9
	/** Dead/dimmed: the Unknown state's dashes, a departed player's row. */
	inline const FLinearColor Dead    { 0.290f, 0.353f, 0.420f };  // #4A5A6B
}

/**
 * The button EDGE state — the measured Idle→Hover transition, applied from BN because it is
 * missing from the shared component.
 *
 * THE BUG THIS EXISTS FOR. `WBP_ButtonDefault` (BP80, `/Game/UI/Components/Buttons/`) draws its
 * four rules TWICE: once as the inherited `Border` (`UBRHairlineBorder`, whose Edges/DimmedEdges
 * masks C++ already drives — bottom 0.3 → 1.0 on hover), and again as four `EdgeTop/Bottom/
 * Left/Right` UImages carrying the Figma line textures. The four images sit in OverlaySlots 4-7,
 * ABOVE the border at slot 2, at a hard-coded full-white tint that nothing ever moves. They
 * therefore MASK the transition underneath them: measured in the editor 2 Sep, every button in
 * the game renders an identical box in idle, hover, pressed and selected.
 *
 * WHERE THE FIX BELONGS. In `UBRButton::ApplyInvertedState`, beside the `Border->SetEdgeDimmed`
 * call it already makes — one place, every button, every screen. That is `Source/Breachpoint/`,
 * outside this packet's owner path (law 5), and the claim could not be widened, so the fix is
 * filed as a contract_gap on BN42 and this is the BN-side stand-in until it lands. It is NOT a
 * second state machine: it reads CommonUI's own hover/selected state and moves opacity only.
 *
 * Numbers are COMPONENT-SPECS §2, the same ones `FBRHairlineStyle`'s defaults encode: top line
 * at 1.0, bottom line and both side ticks at 0.3, and hover lifts the BOTTOM line alone to 1.0.
 */
namespace BNButtonEdges
{
	/**
	 * How a button's four edge lines rest. Both hover to a full bright box; they differ ONLY at
	 * idle, and the difference is measured, not stylistic:
	 *
	 *   Boxed    the standalone button (`12:725`): top 1.0, bottom 0.3, side ticks 0.3.
	 *            The nav tabs.
	 *   MenuRow  the same button inside a Menu List (`I21:43047;7:7383`, sampled off Figma's
	 *            own render): the top edge reads as PLATE — no top line, no side ticks — and
	 *            only the bottom rule shows. The rail rows on the front end and the lobby.
	 */
	enum class EChrome : uint8 { Boxed, MenuRow };

	/** Wires hover/unhover and applies the idle state once. Call AFTER SetIsSelected. */
	BREACHPOINTNEXT_API void Bind(UBRButton* Button, EChrome Chrome = EChrome::Boxed);
}
