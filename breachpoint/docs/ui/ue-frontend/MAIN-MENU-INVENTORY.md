# Main menu — the necessary-file audit

**Status:** v1, 3 Aug 2026. Answers one question: **is every class and every asset the main menu
needs actually necessary, and can the same result come from fewer?**

**The answer is yes, and the cut is large — but almost none of it comes from the engine code.**
`COMPONENT-BREAKDOWN.md` specified 17 widgets. Nine of them are classes that **should not exist**,
because the C++ in `Source/Breachpoint/UI/` already solves each one better than the spec did. This
file is that reconciliation. It supersedes `COMPONENT-BREAKDOWN.md` §2's inventory.

**Nothing below reduces the layout, the grid, or the CommonUI behaviour.** Every cut is a class or
an asset that was doing a job something else already does.

---

## 1. The headline

| | Specified | Necessary | Cut |
|---|---|---|---|
| **New C++ classes** for the main menu | 9 | **0** | **−9** |
| **WBP assets** for the main menu | 17 | **11** | **−6** |
| C++ classes it uses (all pre-existing) | — | 12, in **9 headers** | — |

**The main menu needs no new C++ at all.** It needs eleven layout assets and one ViewModel wired
up — and the ViewModel gap is the only thing standing between the code as it is and a menu on
screen (§6).

---

## 2. The nine classes that should not exist

Each row is something `COMPONENT-BREAKDOWN.md` asked for and the codebase already answers.

| Specified | Already solved by | Why the spec was wrong |
|---|---|---|
| `UBRScreen_MainMenu` | **`UBRScreen_FrontEnd`** | It *is* the main menu — one screen for `FE_Play`, `FE_Create` and `FE_Community`. Its class comment reads the three frames instance by instance and proves they are identical |
| `UBRTabPage` ×3 + a `CommonVisibilitySwitcher` | **nothing — the mechanism is data** | `UBRVM_FrontEnd::GetMenuRows()` returns **the active tab's rows only**. Tabs swap the rail's *data*, not the widget. Three tab-page assets and a switcher were three assets and a widget to express an array index |
| `UBRMenuList` | **`UBRLeftRail::MenuRowSlot`** (`UPanelWidget`) + `FUserWidgetPool` on the screen | Rows are claimed and released from a pool, never created per tab change. A list class would own nothing the rail and the pool do not |
| `UBRDescriptionStrip` | **`UBRLeftRail::DescriptionSlot`** (`UPanelWidget`) | A slot, not a class. Nothing behavioural was ever attached to it |
| `UBRCarouselDots` | **`UBRFeatureCard::DotsContainer`** (`UPanelWidget`) | Same — the card owns its own dot rail |
| `UBRMicIcon` | **`UBRRosterRow::MicSwitcher`** (`UWidgetSwitcher`) | Mic / Speaking / Muted is three states of one switcher, which is what a switcher is for |
| `UBRRankInsignia` | **`UBRRosterRow::RankInsignia`** (`UImage`) | An image with a soft path. A class would add a member and a setter over `SetBrushFromTexture` |
| `UBRPanelBorder` | **`UBRHairlineBorder`** | Already corrected in `COMPONENT-BREAKDOWN.md` §4 — a `UWidget` over a Slate leaf, not a four-`UImage` composite |
| `UBRProgressionButton` | **`UBRFeatureCard`** (recommended — see §4) | The 334×115 rank panel is the same shape as the 349×222 news card: a clickable ground + image + caption + border + dot rail |

**The pattern in all nine:** the spec reached for a *class* where the codebase had already reached
for a **named slot on a class that exists**. A `UPanelWidget` slot costs one member and lets the
WBP decide what goes in it; a class costs a header, a cpp, a `.uasset`, a `BindWidget` contract and
a place in the build order.

---

## 3. What the main menu actually uses — 12 classes in 9 headers

The codebase already groups related classes per header, which is the file-count answer: **classes
and files are not the same number, deliberately.**

| Header | Classes | What each does |
|---|---|---|
| `Screens/BRScreen_FrontEnd.h` | `UBRScreen_FrontEnd` | The screen. Owns the tab spine, pools menu rows, routes clicks out as route names. Reads one ViewModel and nothing else |
| `Components/BRNavBar.h` | `UBRNavTab`, `UBRNavBar` | Tab (button) and bar. The bar holds a `UCommonButtonGroupBase` and registers LB/RB as **input actions** — tab cycling is CommonUI's, not ours |
| `Components/BRLeftRail.h` | `UBRLeftRail` | The 349×510 `Menu Combo`: feature-card slot, row slot, description slot, selection caret |
| `Components/BRButton.h` | `UBRButton` | The atom. 27 reference variants → 10-value `Type` enum × CommonUI's own Status × 2 alignments |
| `Components/BRFeatureCard.h` | `UBRFeatureCard` | Clickable card: ground, image, caption, border, dot rail |
| `Components/BRRosterPanel.h` | `UBRRosterHeader`, `UBRRosterRow`, `UBRRosterPanel` | The 349×273 party list and its parts |
| `Components/BRProfileBar.h` | `UBRProfileBar` | 1280×50 footer. Lives in the **root layout**, not the screen |
| `Components/BRButtonPrompt.h` | `UBRButtonPrompt` | Glyph + verb. The nav bar's bumpers |
| `Components/BRHairlineBorder.h` | `SBRHairlineBorder`, `UBRHairlineBorder`, `UBRRule` | Every stroke in the game, in one Slate leaf |

Plus the shell it hangs off, which is not main-menu-specific: `BRRootLayout.h`,
`BRActivatableWidget.h`, `BRUIManagerSubsystem.h`, `BRUITypes.h`, `BRUISettings.h`,
`Components/BRComponentTokens.h`, `Styles/BR{Text,Button,Border}Styles.h`,
`ViewModels/BRVM_FrontEnd.h`, `ViewModels/BRVM_Player.h`.

### Are they on the right CommonUI classes?

Checked class by class. **Yes, with one recommendation and one correction.**

| Class | Base | Correct? |
|---|---|---|
| `UBRScreen_FrontEnd` | `UBRActivatableWidget` → `UCommonActivatableWidget` | ✅ It is the screen. The only activatable in the menu |
| `UBRNavTab`, `UBRButton`, `UBRFeatureCard`, `UBRRosterRow` | `UCommonButtonBase` | ✅ All four are clickable **and focusable**. This is the class that owns focus, navigation, held-action progress and the input glyph |
| `UBRNavBar`, `UBRLeftRail`, `UBRRosterPanel`, `UBRRosterHeader`, `UBRProfileBar`, `UBRButtonPrompt` | `UCommonUserWidget` | ✅ Passive containers. None takes focus |
| `UBRHairlineBorder`, `UBRRule` | `UWidget` over `SLeafWidget` | ✅ **Correctly *not* CommonUI.** A decoration primitive drawn ~900 times has no business being a UserWidget |
| `UBRPanel` | `UCommonUserWidget` | ✅ Collapses the Figma `Panels & Cards` page's **45 variants** to one class via two enums |
| `UBRRosterPanel` | `UCommonUserWidget` | ⚠️ **Could be `UBRPanel`.** It re-declares `Ground`, `GradientFill` and `PanelBorder` — the three things `UBRPanel` exists to own. See §4 |

**Nothing in the menu is over-activatable**, which is the pitfall worth checking for. The one place
activatables appear outside a screen is the HUD (`UBRMatchBand`, `UBRKillfeed`), and that is
deliberate and correct: both set `bAutoActivate = true` in their constructors, so
`NativeOnActivated` → `BindViewModels()` actually fires. **Verified — this was the first thing I
suspected and it is handled.**

---

## 4. The two real reductions left, and one that is not worth taking

### 4.1 TAKE — delete `FBRFrontEndTabLayout`, and the screen stops needing a `CanvasPanel`

`BRScreen_FrontEnd.h` carries a **layout contract that contradicts the grid**:

> *"`ProgressionButton` and `PartyList` must be direct children of the screen's ROOT CanvasPanel,
> because `ApplyTabLayout` writes their canvas-slot y in SCREEN-LOCAL design coordinates."*

`LAYOUT-DOCTRINE.md` §6 rules **no `CanvasPanel` on a front-end screen**. The two cannot both hold.

**They do not have to.** The same header says `TabLayouts` is **empty on every tab today**, and the
class comment proves why: the three frames are identical. So `ApplyTabLayout` moves nothing, and the
`CanvasPanel` requirement exists entirely to serve a feature with no data.

**Cut:** one `USTRUCT`, one method, two members, two `#include`s — and the screen becomes
bands-and-columns compatible with no layout compromise. `ProgressionButton` and `PartyList` keep
their binds; only the y-writing goes. If a tab ever genuinely needs different geometry, the honest
answer is a different WBP in `ContentSlot`, which already exists.

### 4.2 TAKE — `UBRProgressionButton` is `UBRFeatureCard` with a different WBP

The screen binds `ProgressionButton` as a bare `UWidget` and comments that no component exists.
**Do not write one.** Measured, the panel is a ground plate at `#000000@0.5`, a title, two 167×94
halves and a 72×10 dot switcher. `UBRFeatureCard` is exactly that shape and is already a
`UCommonButtonBase` with `Ground`, `ImageBox`, `Caption`, `Border` and `DotsContainer`.

Retype the bind from `UWidget` to `UBRFeatureCard` and author `WBP_RecordPanel` against it. **One
class, two assets** — the same pattern that makes one `UBRButton` serve 26 screens.

*Stated honestly:* the class is **named** for the news card. If the rank halves later need real
behaviour, promote it then — but promoting a class that exists is cheap and writing one that
duplicates it is not.

### 4.3 DO NOT TAKE — merging `UBRRosterHeader` into `UBRRosterPanel`

It would save one class and one asset. It would also mean the panel binds the header's `Label` and
`Count` directly, so the header can never be reused by the lobby, the scoreboard or the roster
screen — which `SCREEN-MANIFEST.md` §5 says need it (5/31 screens). **A saving that costs reuse is
not a saving.** Left alone.

*(`UBRRosterPanel` → `UBRPanel` from §3 is the same shape of trade and is genuinely marginal: it
would remove three duplicated members but touches a 1,179-line file that works. **Recorded, not
scheduled** — worth doing the next time that file is open for another reason, not on its own.)*

---

## 5. The file list, both halves

### 5.1 `Source/` — what the main menu needs. **All of it exists.**

```
Source/Breachpoint/UI/
├── BRActivatableWidget.h/.cpp        base: input config + ViewModel bind/unbind on activation
├── BRRootLayout.h/.cpp               4 layer stacks + persistent chrome
├── BRUIManagerSubsystem.h/.cpp       ShowMainMenu() · PushWidgetToLayer() · VM ownership
├── BRUITypes.h/.cpp                  FUITag layer tags
├── BRUISettings.h                    MainMenuScreenClass (soft) + tuning
├── Components/
│   ├── BRComponentTokens.h           EBRUIColorToken · EBRStrokeWeight — no hex anywhere
│   ├── BRHairlineBorder.h/.cpp       SBRHairlineBorder · UBRHairlineBorder · UBRRule
│   ├── BRPanel.h/.cpp                45 Figma panel variants → 1 class
│   ├── BRButton.h/.cpp               the atom + settings row + highlight + styles
│   ├── BRNavBar.h/.cpp               UBRNavTab · UBRNavBar
│   ├── BRLeftRail.h/.cpp             the 349×510 Menu Combo
│   ├── BRFeatureCard.h/.cpp          clickable card (and the record panel — §4.2)
│   ├── BRRosterPanel.h/.cpp          UBRRosterHeader · UBRRosterRow · UBRRosterPanel
│   ├── BRProfileBar.h/.cpp           1280×50 footer
│   └── BRButtonPrompt.h/.cpp         glyph + verb
├── Styles/
│   ├── BRUITokens.h                  the token table
│   ├── BRTextStyles.h/.cpp           7 CommonTextStyle subclasses
│   │                                 (merged 4 Aug: was BRMenuRow + BRSettingsRow
│   └── BRBorderStyles.h/.cpp         3 CommonBorderStyle subclasses
└── ViewModels/
    ├── BRVM_FrontEnd.h/.cpp          tabs, rows, feature card, focus
    └── BRVM_Player.h/.cpp            gamertag, emblem, rank, credits
```

**The style classes are C++, not assets** — which is why `Content/UI/Styles/` does not exist and
should not. `CommonTextStyle` and friends are `UCLASS`es; subclassing them in C++ satisfies R18
and puts the palette under the same token table as everything else.

### 5.2 `Content/` — 11 assets to author, 1 already planned

| # | Asset | Folder | Parent | State |
|---|---|---|---|---|
| 1 | `WBP_RootLayout` | `UI/Layouts/` | `UBRRootLayout` | ✅ **built** |
| 2 | `WBP_MenuRow` | `UI/Components/` | `UBRButton` | 📋 **planned, validates** |
| 3 | `WBP_NavTab` | `UI/Components/` | `UBRNavTab` | ⬜ |
| 4 | `WBP_NavBar` | `UI/Components/` | `UBRNavBar` | ⬜ |
| 5 | `WBP_ButtonPrompt` | `UI/Components/` | `UBRButtonPrompt` | ⬜ |
| 6 | `WBP_FeatureCard` | `UI/Components/` | `UBRFeatureCard` | ⬜ |
| 7 | `WBP_RecordPanel` | `UI/Components/` | `UBRFeatureCard` | ⬜ *(§4.2 — same parent as 6)* |
| 8 | `WBP_LeftRail` | `UI/Components/` | `UBRLeftRail` | ⬜ |
| 9 | `WBP_RosterHeader` | `UI/Components/` | `UBRRosterHeader` | ⬜ |
| 10 | `WBP_RosterRow` | `UI/Components/` | `UBRRosterRow` | ⬜ |
| 11 | `WBP_RosterPanel` | `UI/Components/` | `UBRRosterPanel` | ⬜ |
| 12 | `WBP_ProfileBar` | `UI/Components/` | `UBRProfileBar` | ⬜ |
| 13 | `WBP_Screen_FrontEnd` | `UI/Screens/` | `UBRScreen_FrontEnd` | ⬜ |

**Build order is leaf-up** and falls out of the `BindWidget` graph: 2–5 → 6, 7 → 8 → 9, 10 → 11 →
12 → 13. `wbp_plan.py`'s `validate_all()` already enforces that a hosted class is generated before
its host, so getting it wrong is an import-time error, not a call-40 surprise.

**Repo-wide, for scale:** `Content/UI/` holds **157 `.uasset`s** — 10 WBPs (root, HUD, progress
bar) and 147 textures and fonts. The eleven above take the WBP count to 21 and cover the whole
front end's foundation, because the same components serve the lobby, settings and the browser.

---

## 6. Is it testable right away? — **No, and there are exactly two blockers**

### Blocker 1 — the assets do not exist. *Editor lane.*

Eleven WBPs. `wbp_plan.py` + `build_wbp.py` generate them against the editor MCP, which is not
connected in this session. **Mechanical, not a decision.**

### Blocker 2 — `UBRVM_FrontEnd` is constructed nowhere. *Terminal lane, and it is the real one.*

```
UBRUIManagerSubsystem::ShowMainMenu(LocalPlayer)
  → PushWidgetToLayer(Layer_Menu, UBRUISettings::Get().MainMenuScreenClass)   ✅ exists
  → the stack activates the screen                                            ✅
  → UBRScreen_FrontEnd::BindViewModels()                                      ✅ runs
  → BoundViewModel is NULL                                                    ❌ nothing ever
                                                                                 called
                                                                                 SetFrontEndViewModel()
  → ApplyEmptyState()                                                         → an empty menu
```

Grepping the whole module for `UBRVM_FrontEnd` outside its own two files returns **nothing but the
screen's forward declaration.** The subsystem has `GetCombatViewModel` and `GetMatchViewModel` and
no front-end equivalent — the screen's own header files this as a contract gap rather than working
around it, which is correct and is also why it is still open.

**The fix is small and is the highest-value thing left in this lane:** own a `UBRVM_FrontEnd` per
local player in `FBRLocalPlayerUI` exactly as the other two are owned, expose
`GetFrontEndViewModel()`, and push it in `ShowMainMenu` before returning. That is the same pattern
three times over, not a new one.

**Then, and only then, the menu renders with real content.** Until the fix, an editor build shows
the screen in its honest empty state — which is a correct result and a useless test.

### Not blockers, worth knowing

- **`MenuRowWidgetClass`** on the screen is a `TSoftClassPtr` and must point at `WBP_MenuRow`, and
  `MainMenuScreenClass` in `UBRUISettings` at `WBP_Screen_FrontEnd`. Both are config, both set once.
- **Rung honesty:** a menu on screen in PIE is **rung 2**. Party lists and roster presence need
  rung 4 (R30) before "works" is said out loud.

---

## 7. What this audit did not cut, and why

**The HUD, and the other 27 screens.** `Source/Breachpoint/UI/` is **85 files, 16,147 lines,
~60 classes**, and the main menu accounts for **41 files / 7,525 lines** of it. The other
**44 files / 8,622 lines** are the HUD and the components waves 2–7 need
(`UBRItemTile`, `UBRItemGrid`, `UBRTableRow`, `UBRGearDetail`, `UBRPageTitle`, `UBRHighlightButton`,
`UBRScrollBar`, `UBRScrim`, `UBRModal_*`). **None of it is main-menu work and none of it was
audited here.** A file being unused *today* is not evidence it is unnecessary — `SCREEN-MANIFEST.md`
§5 says which screens each one unblocks, and that is the ledger to argue with, not this one.

**The one thing worth saying about them:** they already follow the pattern this audit recommends —
`UBRItemTitle : UBRPageTitle`, `UBRScrim : UBRHairlineBorder`, `UBRGearDetail : UBRPanel`,
`UBRItemTile` and `UBRTableRow` both `UCommonButtonBase + IUserObjectListEntry`. The consolidation
this file argues for is **already the codebase's habit**. The 17-widget spec was the outlier.
