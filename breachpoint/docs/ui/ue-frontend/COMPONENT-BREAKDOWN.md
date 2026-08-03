# Main menu — the structure, and every widget inside it

**Status:** v1, 3 Aug 2026. The companion to `LAYOUT-DOCTRINE.md` §6: that section rules *the
shell*, this one breaks down *what goes in it*, widget by widget.

**Every measurement here is sourced.** `COMPONENT-SPECS.md` was read off the reference file's
live nodes via the Figma Plugin API — fills, strokes, stroke alignment, auto-layout padding and
letter-spacing units are exact, not screenshot-estimated. Where a number is **not** measured,
this file says `UNMEASURED` rather than inventing one.

Reuse counts are from `SCREEN-MANIFEST.md` §5's dependency graph. They are the reason to build in
the order given at the end, and the reason nothing here is authored twice.

---

## 1. The shell — the grid, drawn

Bands first, columns inside the middle band. The screen has **no `CanvasPanel`**.

```
┌────────────────────────────────────────────────────────────────────────┐ 1280 × 720
│ Screen_SafeZone            (0 inset on PC · platform ratio on console)  │
│ ┌────────────────────────────────────────────────────────────────────┐ │
│ │ HEADER BAND                                            Auto height │ │
│ │  ┌──────────────────────────────────────────┐                      │ │
│ │  │ WBP_NavBar            666 × 30           │                      │ │
│ │  └──────────────────────────────────────────┘        pad-bottom 63 │ │
│ ├────────────────────────────────────────────────────────────────────┤ │
│ │ CONTENT BAND                                             Fill 1.0  │ │
│ │ ┌────────────┐ ┌────────────┐ ┌────────────┐                       │ │
│ │ │ COL 1      │ │ COL 2      │ │ COL 3      │  each Fill 1.0        │ │
│ │ │ Fill 1.0   │ │ Fill 1.0   │ │ Fill 1.0   │  = 380.67 @1280       │ │
│ │ │            │ │            │ │            │                       │ │
│ │ │ ┌────────┐ │ │            │ │ ┌────────┐ │  panel max-w 348.67   │ │
│ │ │ │ menu   │ │ │  RESERVED  │ │ │ status │ │                       │ │
│ │ │ │ 348.67 │ │ │  the 3D    │ │ │ 348.67 │ │  col1 HAlign LEFT     │ │
│ │ │ │ HAlign │ │ │  subject   │ │ │ HAlign │ │  col3 HAlign RIGHT    │ │
│ │ │ │ LEFT   │ │ │  reads     │ │ │ RIGHT  │ │                       │ │
│ │ │ │        │ │ │  through   │ │ │        │ │                       │ │
│ │ │ └────────┘ │ │            │ │ └────────┘ │                       │ │
│ │ └────────────┘ └────────────┘ └────────────┘                       │ │
│ │        pad-r 24  pad 24  pad 24  pad-l 24                          │ │
│ │              └── 48 gutter ──┘└── 48 gutter ──┘                    │ │
│ └────────────────────────────────────────────────────────────────────┘ │
│ ┌────────────────────────────────────────────────────────────────────┐ │
│ │ FOOTER BAND    WBP_ProfileBar   1280 × 50      Auto · from ROOT    │ │
│ └────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────┘

margin 69 · content 1142 · 348.67 × 3 + 48 × 2 = 1142 exact
column origins  x = 69 · 465.67 · 862.33
```

**The gutter is half-padding on each neighbour** — `pad-right 24` on col 1 plus `pad-left 24` on
col 2 makes 48, and it survives a column being hidden. A spacer widget would leave a hole.

**Columns are `Fill 1.0`; panels inside carry the 348.67 `max-w` and align outward.** At 1280 the
two are identical. At 21:9 the columns grow to 490.67, the panels stay 348.67 pinned to the outer
edges, and all 563 new design px land on the subject. That is the whole ultrawide story.

---

## 2. The complete inventory — 17 widgets, 4 kinds

`B` = `UCommonButtonBase` · `U` = `UCommonUserWidget` · `A` = `UBRActivatableWidget` ·
`—` = a UMG panel in the tree, not its own asset.

| # | Widget | Class | Kind | Measured size | Reuse | Tier |
|---|---|---|---|---|---|---|
| 0 | `WBP_RootLayout` | `UBRRootLayout` | U | full bleed | **all 31** | 0 |
| 0 | `WBP_ProfileBar` | `UBRProfileBar` | U | 1280 × 50 | **all 31** | 0 |
| 0 | `CommonBoundActionBar` | engine | — | hugs | **all 31** | 0 |
| 1 | `WBP_Screen_MainMenu` | `UBRScreen_MainMenu` | **A** | — | 1 | — |
| 2 | `WBP_NavBar` | `UBRNavBar` † | U | 666 × 30 | 18/31 | 1 |
| 3 | `WBP_NavTab` | `UBRNavTab` | **B** | 138 × 26 | inside #2 | 1 |
| 4 | `TabSwitcher` | `CommonVisibilitySwitcher` | — | — | every tabbed screen | — |
| 5 | `WBP_Tab_Play` / `_Training` / `_Settings` | `UBRTabPage` | U | column-width | 3 instances | — |
| 6 | `WBP_FeatureCard` | `UBRFeatureCard` | **B** | 349 × 222 | 4/31 | 2 |
| 7 | `WBP_CarouselDots` | `UBRCarouselDots` | U | UNMEASURED | 4/31 | 2 |
| 8 | `WBP_MenuList` | `UBRMenuList` | U | 349 × 148 (4 rows) | 4/31 | 3 |
| 9 | `WBP_MenuRow` | `UBRMenuRow` | **B** | 250 × 28, pitch 40 | **26/31** | 0 |
| 10 | `WBP_DescriptionStrip` | `UBRDescriptionStrip` | U | 349 × 37 | 7/31 | 1 |
| 11 | `WBP_RecordPanel` | `UBRProgressionButton` † | **B** | 334 × 115 | 3/31 | 2 |
| 12 | `WBP_RosterPanel` | `UBRRosterPanel` † | U | 349 × 273 | 5/31 | 2 |
| 13 | `WBP_RosterRow` | `UBRRosterRow` | **B** | 390 × 30 · **349 in-panel** | 5/31 | 2 |
| 14 | `WBP_MicIcon` | `UBRMicIcon` | U | 16 × 18 | inside #13 | 4 |
| 15 | `WBP_RankInsignia` | `UBRRankInsignia` | U | 26 × 26 in-row | 5/31 | 4 |
| 16 | `WBP_PanelBorder` | `UBRPanelBorder` | U | tracks parent | **#9 #13 + item tiles** | 0 |

† = one of the 8 components we do not yet own (`SCREEN-MANIFEST.md` §6). Building the main menu
against a reference-page component is the blocker that section exists to clear.

**The main menu costs 13 new assets and unblocks 26 of 31 screens.** That ratio is the entire
argument for building components before screens.

---

## 3. The four kinds, and why the distinction is load-bearing

This is the one classification that decides everything else — class, input behaviour, and whether
a widget can be reused at all.

| Kind | Use for | Never |
|---|---|---|
| **`UBRActivatableWidget`** | the *screen*. Pushed to a layer, seizes input, handles Back, declares `GetDesiredFocusTarget()` and `GetDesiredInputConfig()` | a row, a card, a panel |
| **`UCommonButtonBase`** | anything the player can **click or focus**: menu rows, nav tabs, roster rows, the feature card, the record panel | a decoration |
| **`UCommonUserWidget`** | passive containers and readouts: panels, lists, strips, icons | anything clickable — you lose the whole state machine |
| **UMG panel** (`VerticalBox`, `Overlay`, `SizeBox`…) | structure inside a widget | its own `.uasset` — a panel is not a component |

**The pitfall this prevents:** making every component activatable. Each activatable is an input
routing node, and the stack starts fighting itself over who owns focus (`LAYOUT-DOCTRINE.md` §4.1).

**The second pitfall:** making a clickable thing a `UCommonUserWidget` and hand-wiring hover. You
then re-implement idle/hover/pressed/disabled/selected per widget, and they drift. `CommonButtonBase`
ships that state machine, drives it from a `CommonButtonStyle` asset, and is what makes the
inversion rule (§4) authored once.

---

## 4. The atom below the atom — `WBP_PanelBorder`

**The single most reused piece of geometry in the design, and it is easy to miss.** The measured
`Main Button`, `Player Buttons` and `Items` components all carry the same construct: **not a
closed rectangle** but four separate lines at different opacities.

```
WBP_PanelBorder                       : UBRPanelBorder (UCommonUserWidget)
└── Border_Overlay                    Overlay        stroke align CENTER
    ├── TopLine        Image  h1  #ffffff  opacity 1.0    VAlign Top   · HAlign Fill
    ├── BottomLine     Image  h1  #ffffff  opacity 0.3    VAlign Bottom· HAlign Fill
    ├── LeftTick       Image  w1  #ffffff  opacity 0.3    HAlign Left  · VAlign Fill · h20
    └── RightTick      Image  w1  #ffffff  opacity 0.3    HAlign Right · VAlign Fill · h20
```

| Exposed | Type | Why |
|---|---|---|
| `Weight` | 1px · 0.5px · **2px** | Three weights are in use — 404 / 323 / 177 occurrences. Not a free choice |
| `BottomOpacity` | float | **0.3 idle → 1.0 hover.** This is the hover tell on every row |
| `BottomColour` | token | item tiles put the **rarity** colour here; everything else leaves it white |
| `TickHeight` | float | 20 on a 28-row; 0 on a full-bleed band |

**Corner radius is 0 everywhere.** The only radii in the whole reference file are 5 (17 badges),
1, 3, 0.25, 0.75. Sharp corners are the language — a rounded panel is off-system.

**Build this first.** Three separate components measured the same border independently; authoring
it three times is three places the 0.3 drifts.

---

## 5. Layer 0 — the root layout, built once and never per screen

```
WBP_RootLayout                            : UBRRootLayout        ONE instance, persistent
└── Root_Overlay                          Overlay
    ├── GameLayerStack                    CommonActivatableWidgetStack   ← HUD
    ├── GameMenuLayerStack                CommonActivatableWidgetStack   ← pause · scoreboard
    ├── MenuLayerStack                    CommonActivatableWidgetStack   ← THE FRONT END
    ├── ModalLayerStack                   CommonActivatableWidgetStack   ← confirm · error
    ├── Footer_SafeZone                   SafeZone
    │   └── WBP_ProfileBar                UBRProfileBar   1280 × 50 · HAlign Fill
    └── ActionBar_SafeZone                SafeZone
        └── CommonBoundActionBar          ← ONE, persistent
```

**Three things the main menu therefore does not own, and must not re-declare:**

1. **The action bar.** `CommonBoundActionBar` renders whatever the *currently active* widget
   registers. One instance updates itself on every push and pop; one per screen rebuilds on every
   transition and drifts from the real bindings.
2. **The profile bar.** `#000000@0.5` + `BACKGROUND_BLUR`. Re-authoring it per screen is 31 places
   one blur setting drifts.
3. **Its own layer.** The screen is *pushed to* `Layer.Menu`. Only the top of the highest visible
   layer is activated and receives input — which is why the HUD does not have to hide itself when
   a menu opens.

**`WBP_ProfileBar` is a `UCommonUserWidget`, not a button**, even though it contains focusable
elements. The bar is a container; the gamertag chip inside it is the button.

---

## 6. Layer 1 — the screen

```
WBP_Screen_MainMenu                       : UBRScreen_MainMenu (UBRActivatableWidget)
│   pushed to Layer.Menu
│   GetDesiredFocusTarget()  → active tab's MenuList     ← not optional. Ever.
│   GetDesiredInputConfig()  → Menu · mouse captured
│   bIsBackHandler = true    → Back at root = quit confirm (OV_Warning)
│
└── Screen_SafeZone                       SafeZone
    └── Bands_VBox                        VerticalBox          ← NO CanvasPanel
        ├── HeaderBand                    Auto · pad-bottom 63
        │   └── WBP_NavBar
        ├── ContentBand                   Fill 1.0
        │   └── Columns_HBox              HorizontalBox
        │       ├── Col1_Menu             Fill 1.0 · pad-right 24
        │       │   └── TabSwitcher       CommonVisibilitySwitcher · HAlign Left · max-w 348.67
        │       │       ├── [0] WBP_Tab_Play
        │       │       ├── [1] WBP_Tab_Training
        │       │       └── [2] WBP_Tab_Settings
        │       ├── Col2_Subject          Fill 1.0 · pad 24        RESERVED — holds nothing
        │       └── Col3_Status           Fill 1.0 · pad-left 24
        │           └── InvalidationBox   HAlign Right · max-w 348.67
        │               └── Status_VBox   VerticalBox
        │                   ├── WBP_RecordPanel   h115 · pad-bottom 24
        │                   └── WBP_RosterPanel   HUG
        └── (FooterBand inherited from the root layout)
```

**`GetDesiredFocusTarget()` returns the active tab's `MenuList`, not the nav bar.** A controller
picked up mid-session must land on the thing the player acts with. Without the override, focus is
undefined *on controller only* — which is how this ships broken.

**`InvalidationBox` on column 3, not column 1.** The record panel and roster are static between
lobby events; the menu list re-paints on every focus change. Caching the thing that changes is
worse than not caching.

---

## 7. Layer 2 — the chrome

### 7.1 `WBP_NavBar` — 666 × 30, blocks 18 screens

```
WBP_NavBar                                : UBRNavBar (UCommonUserWidget)
└── Bar_HBox                              HorizontalBox
    ├── BumperLeft    WBP_ButtonPrompt    27 × 15 · VAlign Center      LB
    ├── Tabs_HBox     HorizontalBox       Fill 1.0
    │   ├── WBP_NavTab × N                138 × 26 · pad-right 12      ← pitch 150
    │   └── (tab count is DATA)
    └── BumperRight   WBP_ButtonPrompt    27 × 15 · VAlign Center      RB
```

**Tab count is data, not structure** — `SCREEN-STRINGS.md` §1.1 rules two tabs for the slice
(`PLAY`, `CAREER`), the reference has four, and the sub-level variant is 516 × 30. A bar built for
a fixed four is a bar rebuilt for every screen that has three.

**The nav bar drives the `CommonVisibilitySwitcher`, and nothing else.** It does not push screens.
Tabs swap the **rail data**, not the widget — one screen, N tab pages.

### 7.2 `WBP_NavTab` — 138 × 26, a button

```
WBP_NavTab                                : UBRNavTab (UCommonButtonBase)
└── Tab_Overlay                           Overlay
    ├── Border        Image               stroke #ffffff · align OUTSIDE
    │                                     weight 3 ACTIVE · 2 inactive
    ├── Label         CommonTextBlock     (13,5) 120 × 14
    │                                     Rajdhani SemiBold 14 · ls 15% · UPPER · LEFT
    └── Icon          CommonLazyImage     (113,1) 24 × 24 · optional
```

**Inactive = whole-component opacity 0.6.** Not a dimmer text colour — the whole widget. That is
one `Style` value, and it is why this is a `CommonButtonBase` and not a hand-lit frame.

**Stroke alignment is `OUTSIDE`.** UMG has no outside stroke, so the border is a
`Draw As: Box` brush with the margin authored into the nine-slice and the widget sized 3px larger
than the hit area. `ASSET-PIPELINE.md` §5 has the nine-slice rule; getting this wrong shifts every
tab by 3px and the pitch stops being 150.

---

## 8. Layer 3 — column 1, the menu

### 8.1 `WBP_Tab_Play` — one tab page, three instances of the same class

```
WBP_Tab_Play                              : UBRTabPage (UCommonUserWidget)
└── Tab_VBox                              VerticalBox
    ├── WBP_FeatureCard                   h 222 · pad-bottom 10
    ├── WBP_CarouselDots                  h 8   · pad-bottom 12
    ├── WBP_MenuList                      Auto                  ← the focus target
    └── WBP_DescriptionStrip              h 37  · pad-top 12
```

**`_Training` and `_Settings` are the same class with different data.** A tab that needs a
different *structure* is a different screen, not a tab — that call belongs in the ticket, and
`SCREEN-MANIFEST.md` §4.6 already rules Settings a separate screen for exactly this reason.

### 8.2 `WBP_MenuList` — 349 × 148 at 4 rows

```
WBP_MenuList                              : UBRMenuList (UCommonUserWidget)
└── Rows_VBox                             VerticalBox
    └── WBP_MenuRow × N                   h 28 · pad-bottom 12        ← 28 + 12 = pitch 40
```

**`VerticalBox` at N ≤ ~8, `UListView` above it.** The front end's lists are four rows and fixed;
a `UListView` there costs an entry-widget class, an `IUserObjectListEntry` implementation and a
data-object per row to save nothing. The item grids (10/31 screens, hundreds of tiles) are where
virtualisation earns its complexity. **Do not reach for `UListView` by reflex.**

**Pitch is padding.** 28 + 12 = 40. Never a spacer widget — a spacer is a widget nobody can find
later, and it breaks the moment a row is hidden.

### 8.3 `WBP_MenuRow` — 250 × 28, the atom. **26 of 31 screens.**

The highest-leverage single asset in the project. Its variant matrix is why `Settings` and
`MatchComposer` are one screen each instead of N sub-screens.

```
WBP_MenuRow                               : UBRMenuRow (UCommonButtonBase)
└── Row_Overlay                           Overlay
    ├── Background    Image               fill transparent → #ffffff on hover
    ├── WBP_PanelBorder                   §4 · TickHeight 20
    └── Content_HBox  HorizontalBox       pad T0 R10 B0 L10 · gap 10 · VAlign CENTER
        ├── Icon      CommonLazyImage     16 × 16   optional
        ├── Label     CommonTextBlock     Fill 1.0 · Rajdhani SemiBold 16 · ls 10% · UPPER · LEFT
        └── Value     CommonTextBlock     Auto     · same style · RIGHT   (settings rows)
```

**The state table — one `CommonButtonStyle` asset, not four widgets:**

| Status | What changes |
|---|---|
| **Idle** | transparent fill · white text · bottom line 0.3 |
| **Hover / Focused** | **inversion**: fill → `#ffffff`, text → `#000000`, bottom line 0.3 → **1.0** |
| **Disabled** | every child opacity → 0.5. **Geometry unchanged** |
| **Active** (drop-down / dig-down) | as Hover, plus the disclosure glyph rotates |

**Idle → Hover is an inversion, not a highlight.** That one rule explains most of the reference
file, and it is authored **once** in the style asset. A widget that types its own hover colour is
the defect `LAYOUT-DOCTRINE.md` §4.3 exists to prevent.

**Hover and Focused must be the same visual.** Mouse hover and gamepad focus are different engine
states; if only one is styled, the menu looks dead on controller. `CommonButtonBase` gives both —
use both.

**The 10-value `Type` axis swaps what sits inside the same 250 × 28 shell:** `Default`,
`Disabled`, `Drop Down`, `Dig Down`, `Icon Only` (40 × 40), `Slider`, `Checkbox`, `Radio`,
`Map Voting` (250 × 60), `Image` (250 × 120). Plus an `Alignment` axis: `Left` | `Center`.
**27 variants, one asset.** Authoring `WBP_SettingsRow` as a separate widget is the mistake this
matrix exists to prevent.

**Width note:** 250 is the *measured component* width; in the rail the row is `HAlign Fill` inside
a 348.67 column and stretches. The `Content_HBox` is auto-layout, so this is free — nothing is
re-measured.

### 8.4 `WBP_FeatureCard` — 349 × 222, and it is a button

```
WBP_FeatureCard                           : UBRFeatureCard (UCommonButtonBase)
└── Card_Overlay                          Overlay
    ├── Background   Image                fill #000000@0.5
    ├── Art          CommonLazyImage      Fill · soft path from the ViewModel
    ├── WBP_PanelBorder                   §4
    └── Text_VBox    VerticalBox          VAlign Bottom · pad 16
        ├── Title    CommonTextBlock      Rajdhani SemiBold · UPPER
        └── Body     CommonTextBlock      Roboto Condensed Medium
```

**`CommonLazyImage`, not `Image`.** The card art is a `TSoftObjectPtr` from
`GAP: UBRVM_FrontEnd` — async-loaded, never hard-linked, with a visible placeholder while it
resolves. `LAYOUT-DOCTRINE.md` §3.3: **an empty slot and a broken binding look identical**, and
the placeholder is what tells them apart. This is BP70's D2 defect, in a different widget.

### 8.5 `WBP_DescriptionStrip` — 349 × 37

`UCommonUserWidget`. One `CommonTextBlock`, Roboto Condensed **Medium Italic** 14 · ls 8% — the
only italic in the system, because Rajdhani ships none. **Explicitly non-focusable**: a passive
readout that can take focus swallows navigation.

Its text is *the focused row's* description, so it reads from the ViewModel's focus state, not
from the row. A row that pushes its own text into a sibling is a coupling that breaks the moment
two lists exist.

---

## 9. Layer 3 — column 3, the status

### 9.1 `WBP_RecordPanel` — 334 × 115, `UBRProgressionButton`

```
WBP_RecordPanel                           : UBRProgressionButton (UCommonButtonBase)
└── Panel_Overlay                         Overlay
    ├── Content      Image                fill #000000@0.5
    ├── Title        CommonTextBlock      "SERVICE RECORD"
    │                                     Rajdhani BOLD 16 · ls 10% · #ffffff · DROP_SHADOW
    ├── Sides_HBox   HorizontalBox
    │   ├── LeftSide     167 × 94         rank insignia + rank name
    │   └── RightSide    167 × 94         progress
    └── Switcher     WBP_CarouselDots     72 × 10 at (130, 121)
```

**"CAREER RANK" → "SERVICE RECORD"** (`SCREEN-STRINGS.md` §1.3) — same function, avoids Halo's
exact panel name. **"SERGEANT GRADE 1" → "SERGEANT"**: our ladder has 16 ranks and no grades.

**The UNSC emblem in this panel is the single most Halo-owned pixel on the screen.** It becomes
`T_Logo_Mark`. That is art, not a string, and it does not travel with the string pass.

**`DROP_SHADOW` on the title only.** It sits over the 3D scene and needs to separate; the panel
ground does not. Effects are per-role, not per-panel (`COMPONENT-SPECS.md` §7).

### 9.2 `WBP_RosterPanel` — 349 × 273

```
WBP_RosterPanel                           : UBRRosterPanel (UCommonUserWidget)
└── Panel_Overlay                         Overlay
    ├── Ground       Image                linear gradient @0.5
    ├── Inset        Image                #000000@0.4 · inset 3
    ├── Stroke       Image                #ffffff@0.2 · 1px · align INSIDE
    └── Content_VBox VerticalBox          pad 3
        ├── WBP_RosterHeader              h 31
        └── Rows_VBox                     VerticalBox
            └── WBP_RosterRow × 4         h 30
```

**Four slots, not a friends list.** The reference render shows a social roster; ours is a
**Fireteam of 4 with empty `Invite…` states** (`SCREEN-MANIFEST.md` §4.10). The empty slot is a
`WBP_RosterRow` variant, not a hidden row — a hidden row collapses the panel and the layout
jitters as players join.

**Stroke alignment here is `INSIDE`**, where the nav tab's is `OUTSIDE`. Both are one brush margin
in UMG, and getting them backwards is a 1px drift nobody traces.

### 9.3 `WBP_RosterRow` — 390 standalone, **349 in-panel**

```
WBP_RosterRow                             : UBRRosterRow (UCommonButtonBase)
└── Row_Overlay                           Overlay
    ├── TeamFill     Image                inset 2 · per-player colour / emblem art
    ├── WBP_PanelBorder                   §4 · opacity 0.3
    └── Content_HBox HorizontalBox        inset 2 · pad T0 R15 B0 L5 · gap 10 · CENTER
        ├── Emblem       CommonLazyImage  26 × 26
        ├── Gamertag     CommonTextBlock  Fill · Roboto Condensed Medium 14 · ls 0% · LEFT
        ├── Rank         Overlay          30 × 26 · fill #ffffff@0.5
        │   └── WBP_RankInsignia          26 × 26 + **Medal 3D effect**
        ├── WBP_MicIcon                   16 × 18   Mic · Speaking · Muted
        └── External_HBox                 40 × 30 · gap **−5** (deliberate overlap)
            ├── PartyLeader               30 × 30
            └── CurrentPlayer             10 × 10
```

**`TextColor` is an axis: `Black` | `White`.** The gamertag flips against the team fill's
luminance. This is a variant, not a runtime luminance calculation.

**Gap −5 is deliberate.** The party-leader and current-player icons overlap by design. Negative
`HorizontalBox` slot padding reproduces it; "fixing" it to 0 is a regression.

**Medal 3D is the only skeuomorphic effect in the system** — two inner shadows and two drop
shadows, on rank insignia and medals *only*. It is 343's "fidelity scales with nearness to
gameplay" tier rule, and applying it to a panel breaks the flat language everywhere else.

---

## 10. What makes this modular — the five rules, stated once

1. **Every clickable thing is a `CommonButtonBase` with a shared `CommonButtonStyle`.** Six of the
   17 widgets are buttons. The inversion, the focus ring, the disabled dimming and the
   hover-equals-focus rule are authored **once** and inherited six times. A widget that types its
   own hover colour has left the system.
2. **Variants beat new assets.** `WBP_MenuRow`'s 27 variants replace ~10 would-be widgets. Before
   authoring anything new, check whether it is a `Type` value on something that exists.
3. **Structure is shared; content is data.** Tab count, row count, roster occupancy, tab page
   contents — all data. The band-and-column shell is identical on the lobby, settings,
   matchmaking and carnage report; only the column contents change.
4. **Geometry lives in the layout, never in a widget.** No widget knows where it is. Bands give
   vertical position, columns give horizontal, `SizeBox` gives max-width, `SafeZone` gives the
   margin. This is why there is no `CanvasPanel`.
5. **One writer per asset.** A WBP is binary and the critic cannot diff it. Two tickets editing
   `WBP_MenuRow` in the same window is a lost change with no merge conflict to warn you.

---

## 11. Build order — it falls straight out of the dependency graph

| # | Build | Why here |
|---|---|---|
| 1 | `WBP_PanelBorder` + the three `Common*Style` assets | Everything below draws with them. Three components measured this border independently |
| 2 | `WBP_MenuRow` | 26/31 screens. The single highest-leverage asset in the project |
| 3 | `WBP_NavTab` → `WBP_NavBar` | 18/31. Unblocks whole waves |
| 4 | `WBP_MenuList`, `WBP_DescriptionStrip` | Column 1 is now complete |
| 5 | `WBP_RankInsignia`, `WBP_MicIcon` → `WBP_RosterRow` → `WBP_RosterPanel` | Leaf-up. The row cannot be built before its two children |
| 6 | `WBP_FeatureCard`, `WBP_CarouselDots`, `WBP_RecordPanel` | Parallelisable — no dependants |
| 7 | `WBP_Tab_Play` → `WBP_Screen_MainMenu` | The screen is last. It is assembly, not authoring |

**Steps 1–5 are 90% of the reuse and none of the screen.** If the screen gets built first, every
component is authored inside it and extracted later — which is the same work twice, plus a binary
merge.

---

## 12. What this file does not settle

- **`WBP_CarouselDots` is UNMEASURED.** It appears at 72 × 10 inside the record panel and at an
  unrecorded size under the feature card. Read it before building, do not scale the one we have.
- **The 8 †-marked components are not ours yet** (`SCREEN-MANIFEST.md` §6). Instancing a component
  that lives on a reference page means a layer inside the instance cannot be renamed and editing
  the main corrupts the reference. **That blocks step 3 of §11, not step 1.**
- **The art.** Emblems, rank insignia and the feature-card image are `ART-PROMPT-LIBRARY.md`'s
  families A–E and they are the long pole. Every slot above takes a soft path and renders a
  placeholder until then, which is deliberate: the layout can be finished and looked at before a
  single final asset exists.
