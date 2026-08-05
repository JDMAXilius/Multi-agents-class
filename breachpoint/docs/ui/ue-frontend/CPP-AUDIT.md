# C++ audit — the main-menu classes, judged the way the widgets were


> **NAMES CHANGED 4 Aug 2026, after this document was written.** `UBRMenuRow` is now
> `UBRButton`, `EBRMenuRowType` is `EBRButtonType`, and `BRMenuRow.h` / `BRSettingsRow.h` /
> `BRButtonStyles.h` / `BRHighlightButton.h` are one file: `Components/BRButton.h`. The old
> names are left below **deliberately** — this is a dated record and rewriting its findings
> would make it a different document. See `BUTTON-MODULE-LEDGER.md`.
**Status:** v1, 3 Aug 2026. Three parallel read-only audits over all 22 main-menu translation
units (~7,500 lines) plus the styles, tokens and both new ViewModels. Every finding below carries
a file:line; nothing here is a style opinion. This is the C++ half of the pass that produced
`MAIN-MENU-INVENTORY.md` and `MCP-BUILD-PLANS.md`.

**The headline: the architecture is right, the code is unfinished in specific, findable ways.**
Zero Tick violations, zero hex in code, correct CommonUI bases everywhere that matters, textbook
pooling — and eleven real defects, one of which makes the entire UI stack inert, plus roughly
**700 lines of dead surface** that the less-is-more rule says to delete.

---

## 1. Verdicts

| Class / unit | Verdict | One line |
|---|---|---|
| `UBRHairlineBorder` / `UBRRule` / `SBRHairlineBorder` | **KEEP** | Best-justified class in the set; only one with correct `SynchronizeProperties` + `ReleaseSlateResources` |
| `UBRMenuRow` | **KEEP** | The reference implementation — all state through Native overrides, one inversion path, zero BIEs |
| `UBRButtonPrompt` | **KEEP** | `SetEnhancedInputAction` is C++-only work with live callers; strip 6 of 8 dead constants |
| `BRComponentTokens.h` | **KEEP** | Names-only authority holds; zero hex; four small duplications to forward |
| `UBRActivatableWidget` | **KEEP** | Correct tiny base; the `InputMode` enum knob is bypassed by 4 of 8 subclasses — cut the knob |
| `UBRRootLayout` | **KEEP** | Four stacks registered by native `FUITag` correctly; cleanest file in the set |
| `BRUITypes` | **KEEP** | Move `FBRCombatAttributeBindings` out so `AttributeSet.h` stops riding into every UI TU |
| `BRUISettings.h` | **KEEP** | Every property has readers — **but zero ini config exists, so all six soft classes resolve null** |
| `UBRVM_FrontEnd` | **FIX** | Correct MVVM, one consumer, constructed by nobody; ~40% of its surface has no producer *or* consumer |
| `UBRVM_Player` | **FIX** | Textbook MVVM, **zero consumers and zero constructors** — correct and unplugged at both ends |
| `UBRPanel` | **FIX** | Two of three setters dead; `EditAnywhere` props never preview (no `NativePreConstruct`) |
| `UBRNavBar` / `UBRNavTab` | **FIX** | **Highest-severity file: three lifecycle bugs**, each killing the bar after one screen pop |
| `UBRLeftRail` | **FIX** | One-clause height bug on the exact empty paths the screen exercises; 6 of 11 public methods dead |
| `UBRFeatureCard` | **FIX** | Best async pattern in the module, but art never re-requests after re-construct |
| `UBRRosterPanel` / `UBRRosterRow` / `UBRRosterHeader` | **FIX** | Pooling textbook; the row is a button with no button behaviour; the entire feed has zero callers |
| `UBRUIManagerSubsystem` | **FIX** | Ownership correct per-LocalPlayer; unpublish is not split-screen-safe; mid-match sync loads |
| `UBRScreen_FrontEnd` | **FIX** | VM/route/focus core is right; ~125 lines of tab-layout scaffolding hold the CanvasPanel mandate hostage |
| `UBRProfileBar` | **CUT** | No class in the module references it; `SetIdentity` has 0 callers; re-add **with its caller** when the chrome packet lands |
| `BRBorderStyles.h/.cpp` (4 classes) | **CUT** | Unreachable: no widget in the module derives from `UCommonBorder` at all |
| `UBRTextStyle_Body` / `_Numeral` / `_Flavor` | **CUT** | Zero references each, and all three drift from `figma_tokens.json` — a second source of type truth |
| 9 style classes (`_Tab`, `_MenuRow`, `_MenuRowInverted`, `_Caption`, 3 button + 2 bases) | **KEEP** | `UCommonButtonStyle`'s `TSubclassOf` slots demand classes; the inversion is not expressible as data |

**Counts: 12 KEEP · 8 FIX · 3 CUT groups (1 widget class + 7 style classes).**

**The retention rule the styles audit yields, to be written into `BRTextStyles.h`:**
*a text style class exists only to fill a `UCommonButtonStyle` `TSubclassOf` slot; every other
piece of type data is `figma_tokens.json` and the generator.* That kills the drift table
(`_Caption` 11/50 vs token 12/100, `_Body` wrong family) at the root.

---

## 2. The defect ledger — eleven real bugs, ranked

| # | Sev | Where | Defect |
|---|---|---|---|
| D1 | **P0** | `Config/DefaultGame.ini` | **No `[/Script/Breachpoint.BRUISettings]` section exists.** `BRUIManagerSubsystem.cpp:254` returns at `RootLayoutClass.IsNull()`, so no root layout is ever created and every `Show*` entry point is unreachable. **The entire UI stack is inert**, and this gates every rung-1 claim of everything else |
| D2 | **P0** | `BRNavBar.cpp:154` vs `:161` | Tab-group delegate `RemoveAll` on destruct, re-`AddUObject` only inside `if (!TabGroup)` — which is false forever after (the `Transient` UPROPERTY survives). **One screen pop → every tab click silently dead** |
| D3 | **P0** | `BRNavBar.cpp:139` | `RegisterBumperActions()` only in `NativeOnInitialized`; CommonUI unregisters handles on destruct. **LB/RB tab cycling works exactly once per widget object** — a gamepad-only failure, invisible in a single-push PIE test |
| D4 | **P1** | `BRScreen_FrontEnd.cpp:317/322` | `SetTabs` broadcasts a selection **outside** the `TGuardValue`; every `RefreshAll` fires a spurious `OnTabChangeRequested(0)` → `SetNavTab` → FieldNotify → `RefreshAll` again — self-navigation to tab 0 plus unbounded recursion tearing down rows whose delegates are on the stack |
| D5 | **P1** | `BRNavBar.cpp:215-266` | `SetTabs` has no re-entry guard; the mid-loop selection broadcast can re-enter it and leave orphaned tabs in `Tabs` and the group that are not in the container |
| D6 | **P1** | `BRUIManagerSubsystem.cpp:202-225` | `Unpublish` is not owner-aware: **player 2 leaving split-screen nulls player 1's `BRCombat`/`BRMatch` global contexts** with no re-publish path. `FBRLocalPlayerUI` doesn't record its owner, so it can't tell |
| D7 | **P1** | `BRScreen_FrontEnd.cpp:77-80` | `SetFrontEndViewModel` on an activated screen calls `BindViewModels()` without `UnbindViewModels()` — `AddDynamic`/`AddUObject` don't dedupe, so **every click fires its handler twice**. This is the *normal* path once the VM is pushed from `ShowMainMenu` |
| D8 | **P2** | `BRLeftRail.cpp:134` | `GetChildrenCount() > 0` keeps the stale row count when rows are cleared — the rail renders at 5-row height with zero rows and the caret can park on a row that doesn't exist. Hit on both empty paths the screen exercises (`BRScreen_FrontEnd.cpp:348`, `:359`) |
| D9 | **P2** | `BRFeatureCard.cpp:55-59` | `NativeDestruct` cancels the streaming handle, nothing re-requests on re-construct — pop mid-stream, re-push, and a static carousel shows an empty 349×196.7 band forever |
| D10 | **P2** | `BRScreen_FrontEnd.h:335` | `ContentWidgets` map holds `CreateWidget` products never released in `ReleaseSlateResources` (dies with the PKT-C cut) |
| D11 | **P2** | `BRVM_FrontEnd.cpp:62` + `BRScreen_FrontEnd.cpp:563` | `SetSeasonKeyArt` flips the whole `MenuState` to `Live`; the screen gates the rail on exactly that field — **splash art arriving alone renders a "live" rail with no rows** |

**P3 batch (cheap, real):** `EditAnywhere` props with no `NativePreConstruct` preview path
(`BRPanel`, `BRMenuRow`, `BRRosterPanel::PanelWidth` — the most designer-facing knob in the file);
function-local `static FSlateColorBrush` in the hottest `OnPaint` (`BRHairlineBorder.cpp:72`);
`ClearPrompt` leaves a stale glyph action (`BRButtonPrompt.cpp:42`); mid-match `LoadSynchronous`
in `ShowDeathOverlay`/`ShowCarnageReport`/`ShowHUD` — a hitch at the death moment (preload once);
`UBRRosterRow` is focusable with zero focus rendering and no button group; nav tab still ships
`EBRStrokeWeight::Emphasis` for both states though the 3px `Focus` token landed
(`BRNavBar.cpp:51`); killfeed + match band derive `UBRActivatableWidget` inside the HUD (HUD
lane, not this packet set).

---

## 3. The cut ledger — what less-is-more deletes

| Cut | Size | Evidence |
|---|---|---|
| `FBRFrontEndTabLayout` + `ApplyTabLayout` + `ApplyContentWidget` + `ContentWidgets` + the `BRFrontEnd` constants block | ~125 h + ~55 cpp lines | The shell audit's surgical map lists every line. `TabLayouts` is empty on every tab; `ApplyContentWidget` always takes the collapse-return; **`.h:268-271` is the entire CanvasPanel mandate and nothing else requires one.** Full cut (option A) — the swappable content region returns *with data* if a tab ever differs |
| `BRBorderStyles.h/.cpp` | 81 lines, 4 classes | No `UCommonBorder` subclass exists in the module — unreachable by construction |
| `UBRTextStyle_Body`, `_Numeral`, `_Flavor` | ~60 lines | Zero references; drifting duplicates of token data the generator already writes |
| `UBRProfileBar` | 144 lines | Zero referencing classes; root layout has **no chrome slot for it to live in yet**. Re-add class + slot + caller in one chrome packet |
| `UBRVM_FrontEnd` dead third | ~70 lines | `SeasonKeyArt`, `PressToStartState` + enum, `bStatusBandPersistent`, `FBRFeatureCardEntry::Body` — no producer, no consumer. (Keep `GetFocusedRowDescription` — the Description Strip is a manifest element and its broadcast wiring is the subtle bit) |
| Dead API | ~40 lines | `GetPrimaryCombatViewModel`, `GetPrimaryMatchViewModel`, `GetActiveWidgetOnLayer`, `bWasResident`, `EBRWidgetInputMode` + its dead assignment (`BRScreen_Scoreboard.cpp:17`), `UBRRule::SetOrientation`, `UBRHairlineBorder::SetFillToken`, `UBRPanel::SetGroundToken`/`SetStrokeWeight` |
| ~60 zero-reference `static constexpr` | ~120 lines | Figma documentation wearing a type — 18 in `BRNavBar` alone, 13 on `UBRRosterRow`, 7 in `BRMenuRow`, 6 copies of `1280/720`. Several have **already drifted** from the tokens they shadow (`InactiveOpacity`, `ActiveStrokeWeightPx`, `MenuRowHeight`) |
| Five `GetUnknownValueText` forks | ~25 lines | `BRUI::UnknownValueText()` is the owner, in scope in all five files; every "blocked by owner-path" comment claiming otherwise is stale |

**Total: ~700 lines deleted, 12 classes → and zero behaviour lost — every cut is a value with
no reader or a mechanism with no data.**

**Doc consequence:** `MCP-BUILD-PLANS.md` §C2's `ContentSlot` bind dies with option A — column 2
becomes the unbound `Col2_Subject` reserve. §B5 (`WBP_ProfileBar`) defers to the chrome packet.
Amended in the same commit as this file.

---

## 4. What was clean — stated so nobody re-litigates it

- **Zero Tick, zero polling, zero timers in components.** Every class carries
  `DisableNativeTick`; the Slate leaf adds `SetCanTick(false)`. Law 4 holds without exception.
- **Zero hex in code.** Four hits module-wide, all comments. `BRUITokens.h` matches
  `figma_tokens.json` byte-for-byte on all 25 checked values.
- **Delegate hygiene on the screen is right** — every bind has its unbind, both pools release in
  `ReleaseSlateResources` (`UBRRosterPanel` and the screen's `MenuRowPool` are textbook).
- **Focus is architected, not improvised:** `GetFirstFocusTarget()` →
  `NativeGetDesiredFocusTarget()` is the cleanest handoff in the codebase; tab exclusivity is
  `UCommonButtonGroupBase`, not hand-rolled.
- **The MVVM discipline in both new VMs is complete** — every bindable property has FieldNotify,
  every mutation broadcasts, and the unconditional description broadcast on tab swap
  (`BRVM_FrontEnd.cpp:39`) is exactly the subtlety naive code misses.
- **Ownership is split-screen-correct at the root:** VMs per `ULocalPlayer` on a GameInstance
  subsystem, nothing owned by World or GameMode, `Deinitialize` drains cleanly.

---

## 5. The packets — four, ordered, each one ticket

### PKT-A — "Make it boot" *(terminal lane · unblocks everything · no asset work)*

1. Write `[/Script/Breachpoint.BRUISettings]` into `Config/DefaultGame.ini` — six soft paths
   (D1). The classes they name are the `MCP-BUILD-PLANS.md` assets; paths exist the moment the
   editor lane builds them, and the ini can land first.
2. Wire `UBRVM_FrontEnd` + `UBRVM_Player` per the shell audit's 9-site diff: two context names in
   `BRUISettings` (`"BRFrontEnd"`, `"BRPlayer"` — the established pattern), two members + `Owner`
   on `FBRLocalPlayerUI`, construct/clear/publish/unpublish copies of the adjacent blocks, two
   getters, and the `ShowMainMenu` cast-and-push.
3. Fix D7 in the same packet (`UnbindViewModels(); BindViewModels();`) — wiring #2 is what makes
   that path hot.
4. Fix D6 while `FBRLocalPlayerUI` is open: `Owner` early-return in unpublish.
5. Fix D11: `SetSeasonKeyArt` stops writing `MenuState` (or season art gets its own state field,
   matching `UBRVM_Player`'s three-state precedent).

**Done when:** PIE boots to `WBP_RootLayout`, `ShowMainMenu` pushes the screen, and both new VMs
appear in the MVVM binding dropdown. Rung 2 ceiling.

### PKT-B — "NavBar lifecycle" *(terminal lane · D2, D3, D4, D5)*

1. Rebind `NativeOnSelectedButtonBaseChanged` unconditionally (`RemoveAll` + `AddUObject`) or
   move binding to `NativeConstruct` (D2).
2. `RegisterBumperActions()` → `NativeConstruct` (D3).
3. `bRebuildingTabs` guard in `SetTabs`; defer the selection broadcast until after the loop (D5).
4. Screen side: hoist the `TGuardValue` above `SetTabs` and add a `bRefreshing` guard to
   `RefreshAll` (D4).
5. While the file is open: `EBRStrokeWeight::Focus` on the active tab, delete the stale
   "three weights" comments, delete the 18 dead constants.

**Done when:** push → pop → re-push the front end in PIE; tabs still click, LB/RB still cycle,
and a data refresh does not navigate to tab 0. That sequence IS the regression test — it is the
exact sequence that fails today.

### PKT-C — "The tab-layout cut" *(terminal lane · D8, D9, D10 + the CanvasPanel mandate)*

1. Execute the surgical map: delete `FBRFrontEndTabLayout`, `ApplyTabLayout`,
   `ApplyContentWidget`, `ContentWidgets`, the `BRFrontEnd` constants block, the canvas-slot
   helpers, and the layout-contract comment (option A — the content region returns with data,
   not before). D10 dies with the map.
2. Retype `ProgressionButton` → `TObjectPtr<UBRFeatureCard>` — after step 1 it has zero C++
   readers, so the retype is purely a contract change. It also makes the rank panel focusable,
   which a 334×115 `UWidget` never was.
3. `BRLeftRail.cpp:134`: delete the `> 0` (D8).
4. `BRFeatureCard`: re-request `RequestedImage` in `NativeConstruct` if set and unloaded (D9).

**Done when:** the module compiles with no `CanvasPanelSlot` include in the screen, and
`WBP_Screen_FrontEnd`'s Kickoff `requires:` line flips satisfied. **This is the gate for the
editor lane's screen build.**

### PKT-D — "Dead surface sweep" *(terminal lane · pure deletion + P3 batch)*

Delete: `BRBorderStyles.h/.cpp` · 3 text styles (+ write the retention rule into
`BRTextStyles.h`) · `UBRProfileBar` (note in `MCP-BUILD-PLANS.md` §B5) · the `UBRVM_FrontEnd`
dead third · dead API row from §3 · the ~60 constants · collapse the five em-dash forks.
Fix the P3 batch: `NativePreConstruct` on the three designer-facing classes, the static brush,
`ClearPrompt`'s glyph, preload the three mid-match classes, `UBRRosterRow` demoted to
`UCommonUserWidget` until BP24 gives it a click to handle.

**Done when:** module compiles, `grep` finds zero references to every deleted symbol, and the
UI folder is ~700 lines lighter with identical behaviour.

**Decided and deliberately NOT in any packet:** the button styles stay AND `UBRMenuRow` keeps its
imperative inversion *for now* — two owners for one interaction is real drift risk, but which one
wins depends on whether the style assets are ever authored (editor lane). Filed as an open
question, not silently resolved. Same for re-basing killfeed/match band: HUD lane, not menu.

---

## 6. Execution record — all four packets landed 3 Aug 2026

Commits: PKT-A `2a190df` · PKT-B `4d2f2f5` · PKT-C `35bac64` · PKT-D (this commit).
**All four are RUNG 0: no UE toolchain exists in the authoring container, so nothing compiled.**
Every deletion was grep-verified for zero references before AND after; the regression sequence
for the first session with an editor is in PKT-B (push → pop → re-push, click a tab, press LB).

**One audit finding corrected during execution:** the styles auditor marked all three
`BR::Tokens` geometry ints dead; the post-cut grep caught `MenuRowHeight` and `NavBarHeight`
live in `BRButtonStyles.cpp` as `MinHeight`. Both restored; only `MenuRowPitch` was truly dead.
A cut list is a hypothesis until the after-grep agrees.

**PKT-D residue — verified dead but left in place, sized against the compile risk of blind
editing a working file** (each is a one-line cut for whoever next opens the file with a
compiler):
- `UBRLeftRail`: dead constants (`MeasuredRailHeight`, the two alternate origin Ys) and dead
  methods (`SetRailOriginY`, `SetRowCount`, `SetRailOpen`/`IsRailOpen`, `GetFocusedRowIndex`)
  plus the un-invoked reveal-notch mechanism — kept because the rail's geometry derivation is
  interlocked and the file is live in PKT-C's changes.
- The remaining `DesignCanvasWidth/Height` copies on `BRScrim`, `BRTableRow`, `BRModal_Warning`
  (three of the original six; `BRPanel`, `BRScreen_FrontEnd`, `BRProfileBar` copies died in the
  packets).
- `EBRWidgetInputMode` stays: the audit called the knob cuttable, but `UBRHUDLayout` genuinely
  consumes it through the base `GetDesiredInputConfig` — removing it is a HUD-lane change, not
  a dead-code cut. Only the scoreboard's dead assignment was removed.
- `MarkStale()` on both new VMs stays: a third staleness pattern, but a plausible lifecycle
  verb whose callers arrive with the producers.
