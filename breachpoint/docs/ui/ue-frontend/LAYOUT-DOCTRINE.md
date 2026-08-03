# Widget layout doctrine — panels, grids, dynamic art, and CommonUI

**Status:** v1, 3 Aug 2026. Fills three gaps the existing `ue-frontend/` docs leave open:
which **panel** to use (grids in particular), how **dynamic art** binds to our data, and the
**CommonUI** rules that decide widget class and input.

Everything here is expressed in `Tools/gen_ui/wbp_plan.py`'s vocabulary — that file is the
committed plan R37.1 requires and the MCP executes it. **This doctrine is what a plan author
follows; it invents no new format.**

Binding law: `CLAUDE.md` 3/4/5/7, `R18`/`R26`. Geometry: `UI-DESIGN-SYSTEM.md` §3.
Mechanics: `ue5-ui-architecture` skill. Assets: `ASSET-PIPELINE.md`.

---

## 1. Panel selection — the whole decision, in one table

**There is no "grid overlay" *widget* in UMG — but the grid is structural, not a guide.**
*(Corrected 3 Aug 2026 against the reference grid overlay; the earlier claim that it "becomes
anchors + fixed offsets" was wrong and produced a worse layout — see §6.)* A screen's grid is
built from **bands (`VerticalBox`) and columns (`HorizontalBox` with `Fill` children and
half-gutter padding)**. That reproduces the overlay exactly, needs no anchors, and is why the
3-column law survives a resolution change. Reach for a *grid panel* only when content is
genuinely tabular.

| You have | Use | Why, and the trap |
|---|---|---|
| A stack with a rhythm (menu rows, roster rows) | **`VerticalBox`** / **`HorizontalBox`** | Slot `Padding` + `Size: Fill/Auto` reproduces Figma auto-layout exactly. **Pitch is padding, never a spacer widget** — a spacer is a widget nobody can find later |
| Things layered on each other | **`Overlay`** | Z-order = child order. Slot alignment centres without constraining — see §2 |
| A few regions pinned to **different screen edges**, with no flow relationship | **`CanvasPanel`** | The ONLY panel that needs anchors. **The HUD, and effectively only the HUD** — see the rule below. Once, at the root, never nested |
| A fixed measured size | **`SizeBox`** | `WidthOverride`/`HeightOverride`. Our measured numbers land here |
| **Equal-sized cells** — loadout grid, emblem picker, medal wall | **`UniformGridPanel`** | Every cell identical. `SlotPadding` once, not per cell |
| **Unequal columns that must align across rows** — scoreboard | **`GridPanel`** + `ColumnFill`/`RowFill` | This is the carnage report. `ColumnFill` is a *ratio* array; leave a column out and it collapses to auto |
| Items that reflow at width — icon trays | **`WrapBox`** | `InnerSlotPadding` is the gap. Do not hand-wrap |
| Content longer than its box | **`CommonHierarchicalScrollBox`** | CommonUI's version; plain `ScrollBox` breaks gamepad focus scrolling |
| A whole screen that must letterbox | **`ScaleBox`** | `ScaleBoxStretch`. Rare — the DPI curve handles our scaling (`SCREEN-MANIFEST.md` §7.2) |
| Swap between mutually exclusive panels | **`CommonVisibilitySwitcher`** | Beats toggling `Visibility` on siblings by hand; keeps focus sane |

**The scalability rule, stated once:** a front-end screen is **bands then columns, and needs no
`CanvasPanel` at all.** Canvas children carry anchors, and every anchor is a number that can be
wrong; Box children inherit their position from the layout. Reserve `CanvasPanel` for screens
whose elements genuinely have no flow relationship — **the HUD, and effectively only the HUD**
(§7), where each element is pinned to a different screen edge on purpose.

---

## 2. Slot rules that are not obvious

- **`Overlay` slot alignment `Center` does not constrain the child.** That is why the reticle
  sits in one — `UBRReticleWidget::ApplyArt` calls `SetDesiredSizeOverride` and the *size is
  the spread readout*. A constraining slot would silently delete the only accuracy cue on
  screen. (`wbp_plan.py` already names this `CENTER`.)
- **`Fill` needs a parent that can distribute.** `layoutSizing` equivalents apply after
  parenting, never before — append first, then set.
- **`SizeBox` is max-width, not position.** `MaxDesiredWidth` on a panel inside a `Fill` column
  is what keeps the panel at its designed 348.67 while the column grows. Position stays
  structural — the column's `HAlign` decides which edge the panel pins to.
- **Half-gutter padding, on both neighbours.** `pad-right 24` + `pad-left 24` = the 48 gutter,
  and it survives a column being hidden. A single 48 on one side does not, and a spacer widget
  leaves a hole.
- **Canvas anchors — HUD only.** Set the anchor to the edge you are measuring from; each corner
  element anchors to its own corner. On a front-end screen there is no canvas and therefore no
  anchor (§6).
- **The profile bar is the last child of a `VerticalBox`, height 50.** Never a fixed Y of 670 —
  670 is a 720-space number, and a band-based footer sits on the bottom edge at any height
  without anyone typing a coordinate.

---

## 3. Dynamic art — the weapon icon, and the rule behind it

**The question:** the HUD, the death screen and the carnage report all need "the icon for the
weapon this row is about", and there are three weapons today and more later.

**The answer is `UCommonLazyImage`, driven by a soft path in `DT_Weapons`.** CommonUI ships this
widget exactly for this: it takes a `TSoftObjectPtr<UTexture2D>`, async-loads it, and shows a
loading state meanwhile — so a soft reference never stalls the frame and never hard-links the
texture into everything that includes the row.

### 3.1 The data change — one column, one field

`DT_Weapons.csv` carries `MeshSoftPath` (the world mesh) and **no icon column**.
`FBRWeaponRow::IconSoftPath` is referenced by `BRScreen_DeathRespawn` today and **does not
exist** — the screen holds a collapsed frame with a comment naming it as the one visibility to
flip. That is **BP25**, still open.

```
DT_Weapons.csv   + IconSoftPath        /Game/UI/Icons/Weapons/T_UI_Weapon_AR
                 + SilhouetteSoftPath  /Game/UI/Art/Weapons/T_UI_Sil_AR   (bottom-right tray)

FBRWeaponRow     + UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UTexture2D> IconSoftPath;
                 + UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UTexture2D> SilhouetteSoftPath;
```

**Two paths, not one.** The tray silhouette is a wide, side-on 94×30.67 rendered from the mesh
(`WEAPON-RENDER-PLAN.md`); a killfeed or scoreboard icon is a small square glyph. One image
cannot be both, and forcing it produces a stretched silhouette in a 24px slot.

### 3.2 How a widget gets it — the flow, and where the rule lives

```
DT_Weapons row  →  UBREquipmentComponent (already resolves the row)
                →  UBRVM_Combat exposes the soft path  ← the ViewModel field is the seam
                →  CommonLazyImage.SetBrushFromLazyTexture(SoftPath)
```

**The widget never reads the DataTable.** It reads a ViewModel field, exactly like ammo and
weapon name do today. A widget that opens `DT_Weapons` is a widget that has to know about
row handles, load order and the equipment component — that is the coupling MVVM exists to
prevent.

**Availability, not just selection.** The user asked for "the selected weapon and all the
weapons available." Selected = `GetActiveWeapon*`. The full set is a **`UniformGridPanel` of
`WBP_WeaponSlot`**, one entry per row, each with its own `CommonLazyImage` — which is why the
grid row of §1 exists.

### 3.3 The default-asset rule

Every dynamic slot needs a **default** that shows the system is working before the real art
lands: `T_UI_Weapon_Unknown`, a neutral silhouette at the same dimensions. Set it as the
`CommonLazyImage`'s brush in the plan, so an unresolved or missing path renders a visible
placeholder rather than a blank hole. **An empty slot and a broken binding look identical;**
a placeholder distinguishes them at a glance.

---

## 4. CommonUI — the rules that decide class and input

Researched against Epic's guidelines and the community's hard-won ones. `ue5-ui-architecture`
owns the mechanics; these are the **decisions**.

### 4.1 Which widget class

| Need | Class | Reason |
|---|---|---|
| A screen that takes over input, can be pushed/popped, handles Back | `UCommonActivatableWidget` | Only this participates in the stack and can seize input |
| A reusable piece inside a screen — a row, a card, a bar | **`UCommonUserWidget`** | **Default choice.** Activatable is heavier and brings input semantics you do not want on a row |
| A tooltip, a decoration, a passive readout | `UCommonUserWidget` or plain `UUserWidget` | **Never activatable.** Epic names tooltips explicitly — they neither forward input nor need to seize it |

**The pitfall this prevents:** making every component activatable. Each one becomes an input
routing node, and the stack starts fighting itself over who owns focus.

### 4.2 Input, focus and gamepad

- **`GetDesiredFocusTarget()` is not optional on any activatable screen.** Return the widget
  the gamepad should land on. Without it, focus is undefined the moment a controller is picked
  up — and it will be undefined *only* on controller, which is how this ships broken.
- **`GetDesiredInputConfig()`** declares Menu / Game / GameAndMenu and mouse capture per screen.
  `UBRActivatableWidget` already overrides it — set it per screen, do not fight it globally.
- **Gamepad runs on a synthetic cursor.** If a thing works with a mouse and is focusable,
  gamepad generally follows. Non-focusable decoration should be explicitly non-focusable so it
  cannot swallow navigation.
- **Accept vs simulated click:** a widget listening for a bound Accept action and a button
  listening for a click can consume each other. Pick one per interactive element — for us,
  **`CommonButtonBase` handles the click; bound actions are for screen-level verbs** (Start
  Match, Back, Invite).
- **`CommonBoundActionBar`** renders the prompt row from the actions actually registered. Our
  `UBRButtonPrompt` inventory should feed it rather than hand-placing glyphs per screen —
  hand-placed prompts drift out of sync with the real bindings, silently.
- **A stack needs a root content class.** An empty `CommonActivatableWidgetStack` leaves the
  input config wherever the last screen left it. A tiny always-present stub activatable that
  requests the Game input config is the fix.

### 4.3 Styling — one place, three assets

`CommonTextStyle`, `CommonButtonStyle`, `CommonBorderStyle` in `Content/UI/Styles/`
(`ASSET-PIPELINE.md` §7). A WBP **references** a style; it never sets a colour, font or brush
locally. The idle→hover inversion is authored once and every menu row inherits it.

`CommonTextBlock` takes a style asset directly — which is why `wbp_plan.py` uses
`/Script/CommonUI.CommonTextBlock` rather than `UMG.TextBlock` for every label.

---

## 5. The architecture the trees hang off — read this before either tree

`UBRRootLayout` already implements Lyra's model and **every screen tree must be written against
it, not beside it**: four `CommonActivatableWidgetStack`s registered by `FUITag`
(`Layer.Game`, `Layer.GameMenu`, `Layer.Menu`, `Layer.Modal`), reached through
`GetLayerStack(FUITag)`.

```
WBP_RootLayout                            : UBRRootLayout      persistent, one instance
└── Root_Overlay                          Overlay
    ├── GameLayerStack                    CommonActivatableWidgetStack   ← HUD
    ├── GameMenuLayerStack                CommonActivatableWidgetStack   ← pause · scoreboard
    ├── MenuLayerStack                    CommonActivatableWidgetStack   ← front end
    ├── ModalLayerStack                   CommonActivatableWidgetStack   ← confirm · error
    └── ActionBar_SafeZone                SafeZone
        └── CommonBoundActionBar          ← ONE, persistent
```

**Three consequences that change how a screen is written:**

1. **A screen owns no action bar.** `CommonBoundActionBar` renders whatever the *currently
   active* widget registers, so one instance in the root layout updates itself on every push
   and pop. One per screen would rebuild on every transition and drift from the real bindings.
2. **A screen owns no layer.** It is pushed to a tag. Only the top of the highest visible layer
   is activated and receives input; everything beneath deactivates. That is why the HUD does not
   have to hide itself when the pause menu opens.
3. **Persistent chrome is composed, not re-declared.** The nav bar and profile bar appear on
   every front-end screen — they are a `WBP_FrontEndChrome` component each screen includes, not
   copies. Composition, not inheritance, because it stays visible in the editor.

---

## 6. Main menu — bands and columns, not anchored regions

**Corrected 3 Aug 2026 against the reference grid overlay.** The first two versions of this
section described three separately-anchored Canvas regions. That is not the system. The
reference is a **band-and-column grid**, and our own measured numbers prove it:

```
base 1280 · margin 69 both sides  ->  content 1142
3 equal columns · gutter 48       ->  column = 348.67   ("349")
348.67 x 3  +  48 x 2  =  1142   exact
column origins:  x=69 · x=465.67 · x=862.33
```

**The "349 left rail" was never a rail. It is column 1 of a three-column grid.** Column 2 is
the subject -- deliberately empty so the 3D scene reads through it -- and column 3 is status.
The overlay draws all three at equal width with the middle one reserved, which is a grid
decision, not an absence.

### 6.1 Three horizontal bands

The overlay splits the screen top-to-bottom before any column exists:

| Band | Holds | Sizing |
|---|---|---|
| **Header** | breadcrumb / nav tabs | Auto (content height) |
| **Content** | the three columns | **Fill 1.0** -- takes all remaining height |
| **Footer** | profile bar | Auto, height 50 |

Bands are a `VerticalBox`. **This is what removes every anchor from the screen.** A band-based
screen has no Canvas at all -- nothing to mis-anchor, nothing that drifts on aspect change, and
the footer sits on the bottom edge because it is the last child of a Fill layout, not because
someone typed `y = 670`.

### 6.2 The tree

```
WBP_Screen_MainMenu                       : UBRScreen_MainMenu (UBRActivatableWidget)
|   pushed to Layer.Menu
|   GetDesiredFocusTarget()  -> active tab's MenuList
|   GetDesiredInputConfig()  -> Menu · mouse captured
|
+-- Screen_SafeZone                       SafeZone        the outer margin, all four sides
    +-- Bands_VBox                        VerticalBox     <- NO CanvasPanel on this screen
        |
        +-- HeaderBand                    Auto            pad-bottom 63
        |   +-- WBP_NavBar                UBRNavBar       LB/RB · drives TabSwitcher
        |
        +-- ContentBand                   Fill 1.0
        |   +-- Columns_HBox              HorizontalBox
        |       |
        |       +-- Col1_Menu             Fill 1.0 · pad-right 24     <- half the 48 gutter
        |       |   +-- WBP_TabSwitcher   CommonVisibilitySwitcher · HAlign Left · max-w 348.67
        |       |       +-- [0] WBP_Tab_Play
        |       |       +-- [1] WBP_Tab_Training
        |       |       +-- [2] WBP_Tab_Settings
        |       |
        |       +-- Col2_Subject          Fill 1.0 · pad 24           <- RESERVED, holds nothing
        |       |                                                       the scene reads through
        |       |
        |       +-- Col3_Status           Fill 1.0 · pad-left 24
        |           +-- InvalidationBox   HAlign Right · max-w 348.67
        |               +-- Status_VBox   VerticalBox
        |                   +-- WBP_RecordPanel    h 115   pad-bottom 24
        |                   +-- WBP_RosterPanel    HUG
        |
        +-- FooterBand                    Auto · height 50
            +-- WBP_ProfileBar            HAlign Fill
```

```
WBP_Tab_Play                              : UBRTabPage (UCommonUserWidget)
+-- Tab_VBox                              VerticalBox
    +-- WBP_FeatureCard                   h 222   pad-bottom 10
    +-- WBP_CarouselDots                  h 8     pad-bottom 12
    +-- MenuList                          UBRMenuList          <- the focus target
    |   +-- Rows_VBox
    |       +-- WBP_MenuRow x 4           h 28, pad-bottom 12   <- pitch 40
    +-- WBP_DescriptionStrip              h 37    pad-top 12
```

### 6.3 Why it is built this way

- **Gutters are half-padding on each neighbour, not spacer widgets.** `pad-right 24` on column 1
  plus `pad-left 24` on column 2 makes the 48 gutter, and it survives a column being hidden --
  a spacer widget would leave a hole.
- **Columns `Fill 1.0`; the panels inside carry the 348.67 max-width and align outward.** At
  1280 the two are identical. At 21:9 the columns grow, the panels stay at their designed width
  pinned to the outer edges, and the extra space lands in the middle where the subject is. That
  ultrawide behaviour now falls out of the grid instead of out of hand-set anchors.
- **Column 2 is a real child that holds nothing.** It reserves the subject's space so columns 1
  and 3 cannot drift inward. Deleting it does not save a widget -- it breaks the grid.
- **No `CanvasPanel` anywhere on this screen.** Bands give vertical position, columns give
  horizontal, `SafeZone` gives the margin. Anchors were doing a job the layout does itself, and
  every anchor is a number somebody can get wrong.
- **`SizeBox` is max-width, not position.** Position is entirely structural.

### 6.4 What this replaces

The previous version anchored `NavBar` at `(69,45)`, `TabSwitcher` at `(69,138)`, `StatusColumn`
at `(-69,45)` and the profile bar bottom-stretch, inside one Canvas. The numbers were right and
the form was wrong: **four hand-placed anchors reproduce what one VBox and one HBox do for
free**, and the anchored version silently loses the grid -- nothing in it says the middle column
exists, so the next screen re-derives the layout instead of inheriting it.

Every other front-end screen (lobby, settings, matchmaking, carnage report) is the **same
band-and-column shell** with different column contents. That is the reuse this shape buys.

### 6.5 The widgets that go in it

**`COMPONENT-BREAKDOWN.md` breaks down all 17**, with each one's internal tree, its widget class,
its measured geometry and its state table. Three of its rules change how the tree above is read:

- **Six of the 17 are `UCommonButtonBase`** — menu rows, nav tabs, roster rows, the feature card,
  the record panel. Anything clickable or focusable. The idle→hover **inversion** is one style
  asset, not six implementations.
- **`WBP_PanelBorder` is the atom below the atom.** The measured `Main Button`, `Player Buttons`
  and `Items` components all carry the same four-line partial border at three opacities. Build it
  once, first.
- **`WBP_MenuRow` alone unblocks 26 of 31 screens**, and its 27-variant `Type` matrix is what
  keeps Settings and MatchComposer to one screen each. Before authoring a new row-like widget,
  check whether it is a variant of this one.

---

## 7. HUD — the tree, and why it is a different kind of thing

The HUD is not a screen with the chrome removed. **It takes no focus, routes no input, and must
never tick.** Positions from `HUD-CAMPAIGN-MEASURED.md`, at the 1280×720 base.

```
WBP_HUDLayout                             : UBRHUDLayout (UBRActivatableWidget)
│   pushed to Layer.Game, activated for the whole match
│   GetDesiredFocusTarget()  → nullptr        ← the HUD never takes focus
│   GetDesiredInputConfig()  → Game · no mouse capture
│   bIsBackHandler = false                    ← Back belongs to the pause menu
│   Visibility (root and every child) = HitTestInvisible
│
└── HUD_Canvas                             CanvasPanel        no SafeZone wrapper — see below
    │
    │   COORDINATES CORRECTED 3 Aug 2026 (HUD-CPP-AUDIT §6): the first version of this tree
    │   quoted HUD-CAMPAIGN-MEASURED numbers that figma_hud_layout.json — the declared
    │   authority for position — contradicts. The BUILT plan sides with Figma in every case;
    │   this tree now matches what is actually built. Elements marked [NOT BUILT] have no
    │   plan node, no C++ class and no imported art — they are future packets, listed so the
    │   six-child layout is not read as complete.
    │
    ├── Vitals          TOP-CENTRE   (0.5,0)   y=66                273.33 × 34
    │   └── WBP_VitalsWidget               UBRVitalsWidget
    │       └── Shield/health, health hidden until damaged (rule lives on UBRProgressBar)
    │
    ├── Reticle         CENTRE       (0.5,0.5)                     size per weapon — the
    │   └── WBP_ReticleWidget              UBRReticleWidget          SIZE IS the spread readout
    │       └── HitMarkers live INSIDE it (HitMarkerImage), not as a sibling
    │
    ├── EquipmentTray   BOTTOM-RIGHT (1,1)    (-200, -106)         140 × 34
    ├── AmmoBlock       BOTTOM-RIGHT (1,1)    (-62, -36)           218 × 60
    │       Figma measures both as ONE 280×110 "Loadout Tray" unit — the two-widget split is
    │       an open FOUNDER DECIDE, and the InvalidationBox waits on it (one box needs one
    │       common parent)
    │
    ├── MatchBand       BOTTOM-CENTRE(0.5,1)  (-14.33, -76)        score · clock · rocket
    │
    ├── Killfeed        BOTTOM-LEFT  (0,1)    (60, -189)           340 × 76 · pooled rows
    │       (doctrine used to say TOP-RIGHT; the render showed mid-left; the plan built
    │        bottom-left from Figma — FOUNDER DECIDE, recorded in TICKET_BP70)
    │
    ├── DamageDirection [NOT BUILT]  no class, no art (the export is the hitmarker, not the wheel)
    ├── MotionTracker   [NOT BUILT]  no class, no art, no data source (BP65)
    └── MedalPopup      [NOT BUILT]  no class, no art, no Figma rect
```

**The HUD-specific rules, and each one is a real bug if broken:**

- **`HitTestInvisible` on the root and every child.** A HUD that is `Visible` swallows mouse
  clicks and gamepad hit-testing. This is the single most common HUD defect and it presents as
  "the game stopped responding to clicks", which nobody attributes to the HUD.
- **No focus target, ever.** Returning a widget from `GetDesiredFocusTarget()` on the HUD means
  a controller lands *on the HUD* and the pause menu opens with focus already stolen.
- **No `SafeZone` wrapper on the HUD canvas.** Vitals and reticle are anchored to screen centre
  and must stay geometrically centred; a safe-zone inset shifts them off-centre on some
  displays. Instead, the *individual corner* elements carry safe-zone-aware offsets.
- **No Tick.** Every value arrives through `FieldNotify` on `UBRVM_Combat` / `UBRVM_Match`.
  Law 4, and the reason the HUD costs nothing when nothing changes.
- **`InvalidationBox` around the tray frame, not the numbers.** The frame, pips and silhouette
  are static for a whole weapon; the mag counter changes per shot. Wrapping the counter would
  invalidate every frame it changes, which is worse than not caching.
- **Killfeed rows are pooled.** `KillfeedMaxVisibleEntries` already exists in `UBRUISettings`;
  rows are hidden and reused, never created and destroyed during a firefight.
- **The reticle's slot must not constrain it.** Its size *is* the spread readout. A slot that
  clamps it silently deletes the only accuracy cue on screen.

**The pause menu is `Layer.GameMenu`, not a HUD child.** It pushes above the HUD, deactivates
it, requests the Menu input config, and pops back — the HUD never has to hide itself, and the
match keeps rendering behind. It reuses `WBP_MenuRow`, `WBP_DescriptionStrip` and the persistent
action bar unchanged, which is the whole return on building components before screens.

---

## 8. What the MCP receives

Nothing new. A plan author writes this tree into `Tools/gen_ui/wbp_plan.py` using the existing
node vocabulary — class path, name, `bind`, slot dict, `font`, `brush` — and `build_wbp.py`
executes it against the editor MCP.

**The two properties of that file that matter more than its format:**

1. **`bind: True` is validated against the real C++ header at plan time.** A `BindWidget` desync
   otherwise fails at *asset load*, not at build — rung 1 stays green and the HUD is just empty
   in PIE. Plan-time checking makes it a text error nobody can miss.
2. **Tool names are never guessed.** Five rounds were lost to guessed MCP names
   (`create_expression` → `add_expression`, `connect_property` → `connect_to_output`,
   `folderPath` → `folder_path`, a required `material_function` that only accepts null). **Probe
   the schema, then write the call.** The receipts in `docs/ui/receipts/` are the record of what
   the tools actually are.
