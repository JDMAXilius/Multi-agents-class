# MCP build plans — the 12 main-menu WBPs, one by one

**Status:** v1, 3 Aug 2026. The per-widget companion to `MAIN-MENU-INVENTORY.md` §5.2: for each
asset, the exact tree the MCP builds, the values it types, what it must NOT contain, and a short
packet prompt a ticket can carry verbatim.

**Every number here is read from a `static constexpr` in the component's own header** — the C++
already carries the measurements with their COMPONENT-SPECS citations, so this file quotes the
code, not the design doc. Where a value exists nowhere, it says **DECIDE**, and the ticket must
close it rather than let the builder invent it.

---

## 0. The generator's laws — read once, apply to all twelve

What `wbp_plan.py`/`build_wbp.py` can and cannot do decides most of what follows:

1. **The plan writes: tree structure, slot properties, `font` (a style name), `brush` (a texture
   that exists).** It cannot write widget properties — no `SetHeightOverride`, no colours, no
   visibility, no blur strength. **Anything sized or coloured at runtime is C++'s job, and the
   constants prove C++ already owns it.**
2. **A `UImage` with no brush renders as a blank white rectangle** (BP70 D2). An `Image` node is
   legal only if (a) a texture exists to brush it, or (b) C++ tints it from a token
   (`TextFrameFill`, `Ground`, `TeamFill` — the engine's default white brush is the correct input
   to a tint). Every other art-bearing image is **omitted until its art exists**.
3. **`UBRHairlineBorder` / `UBRRule` are placed directly** (`HAIRLINE` / `RULE` class paths).
   They draw with zero brushes and zero assets — never wrap them, never substitute an `Image`.
4. **BindWidget names are the law.** The plan validates every name and type against the header at
   import time. A name typed differently than the header is a plan error, not a runtime mystery.
5. **PLAN order is build order.** A hosted WBP must precede its host; `validate_all()` enforces it.
6. **One `CanvasPanel` is permitted at a component root, and only where C++ requires it.** In this
   set that is exactly one place: `WBP_LeftRail`, whose `ApplyCaret()` writes a canvas slot.
7. **No hex, no font literals, no spacers, no graph nodes. Anywhere.**

**Plan support to add once** (same commit as the first new entry):
- `ASSET_FOLDER` rows for all 12 (11 × `UI_COMPONENTS`, screen → `UI_SCREENS`).
- Class-path constants: `ACTION_GLYPH = "/Script/CommonUI.CommonActionWidget"`,
  `BLUR = "/Script/UMG.BackgroundBlur"`, `SAFEZONE = "/Script/UMG.SafeZone"`,
  `SPACER`-free — reserved space is a named empty panel, not a Spacer.
- `_BASES` chains: `UCommonActionWidget→[UWidget]`, `UBackgroundBlur→[UContentWidget,UPanelWidget,
  UWidget]`, `USafeZone→[UContentWidget,UPanelWidget,UWidget]`, plus one chain per hosted `UBR*`
  parent (`UBRNavTab`, `UBRButtonPrompt`, `UBRFeatureCard`, `UBRRosterHeader`, `UBRRosterRow`,
  `UBRRosterPanel`, `UBRLeftRail`, `UBRNavBar` → their `UCommon*` bases).

---

## Tier A — leaves (no hosted WBPs)

### A1 · `WBP_MenuRow` — ✅ already planned, validates. Reference entry.

See the `PLAN` entry itself; it is the template the other eleven follow. 8 widgets, 5 required
binds satisfied, `Label`/`Selection` on `Label/Button`, width deliberately unauthored.

---

### A2 · `WBP_NavTab` — `UBRNavTab` (`UCommonButtonBase`), 138 × 26

```
RootSizeBox        SizeBox    bind?   │ size UNAUTHORED — C++ owns 138×26 (TabWidth/TabHeight)
└ TabOverlay       Overlay            │
  ├ Border         HAIRLINE   bind?   │ slot FILL · all 4 edges, NO ticks (SideTickLength 0),
  │                                   │ NO dimmed edges. C++ drives weight 3 (active) / 2
  │                                   │ (inactive) and whole-widget opacity 0.6 inactive
  └ Label          TEXT       bind    │ slot pad (L13, T5, R5, B7) — from text (13,5) 120×14
                                      │ font Label/Tab = Rajdhani SemiBold 14 · ls 150/1000em
```

**Values C++ already owns (do not author):** stroke weights 3/2 (`ActiveStrokeWeightPx`),
inactive opacity 0.6 (`InactiveOpacity`), the OUTSIDE stroke alignment (the hairline leaf draws
at the geometry edge; the 3px-outside effect is the bar giving each tab slot the extra 3px —
C++'s slot math, not the WBP's).

**MUST NOT contain:** the `Icon` bind (art-bearing `UImage`, no texture — law 2); a fixed
width on the SizeBox; any per-state colour (the button style owns idle/active).

> **Packet prompt:** Add `WBP_NavTab` to `PLAN` after `WBP_MenuRow`. Parent
> `/Script/Breachpoint.BRNavTab`, header `Components/BRNavBar.h`, class `UBRNavTab` (the header
> declares two classes — the `class_body` slice matters). Create `RootSizeBox`, `Border`
> (HAIRLINE, FILL), `Label` (font `Label/Tab`, pad 13/5/5/7). Bind all three; omit `Icon` and
> say why in the notes. `python3 Tools/gen_ui/wbp_plan.py` → PLAN OK, then `build_wbp.py`;
> receipt in `docs/ui/receipts/`. Done when the read-back tree matches the plan exactly.

---

### A3 · `WBP_ButtonPrompt` — `UBRButtonPrompt` (`UCommonUserWidget`), hug × 20

```
RootSizeBox        SizeBox    bind?   │ height UNAUTHORED (C++ PromptHeight 20) · width HUG —
└ PromptHBox       HBOX               │ 62/146/253 at 1/2/3 prompts is MEASURED HUG, never fixed
  ├ ActionGlyph    ACTION_GLYPH bind  │ /Script/CommonUI.CommonActionWidget · VAlign Center
  │                                   │ draws the PLATFORM's glyph for a bound input action —
  │                                   │ zero brushes, zero per-platform art. This is the whole
  │                                   │ reason the bind is CommonActionWidget and not UImage
  └ Verb           TEXT       bind    │ pad-left DECIDE (unmeasured) · VAlign Center
                                      │ font DECIDE — no measured style; Label/Micro (10px) is
                                      │ the nearest. Close in the ticket, not in the editor
```

**MUST NOT contain:** any glyph `Image` (the entire point of `UCommonActionWidget` is that the
platform supplies the art); a fixed width.

> **Packet prompt:** Add `WBP_ButtonPrompt` after `WBP_NavTab`. Parent
> `/Script/Breachpoint.BRButtonPrompt`. Three nodes: `RootSizeBox`, `PromptHBox`, then
> `ActionGlyph` (`/Script/CommonUI.CommonActionWidget`) and `Verb` (font: DECIDE first — record
> the ruling in the plan comment). Add `UCommonActionWidget` to `_BASES`. Two DECIDEs (verb font,
> glyph→verb gap) must be closed in the ticket before building.

---

### A4 · `WBP_RosterHeader` — `UBRRosterHeader` (`UCommonUserWidget`), 349 × 31 in-panel

```
RootSizeBox        SizeBox    bind    │ height UNAUTHORED (C++ HeaderHeight 31)
└ HeaderOverlay    OVERLAY            │
  ├ Ground         IMAGE      bind    │ slot FILL · NO brush — C++ tints from a token (law 2b)
  └ HeaderHBox     HBOX               │ pad DECIDE (in-panel inset unmeasured)
    ├ Label        TEXT       bind    │ Fill 1.0 · VAlign Center · font DECIDE
    │                                 │ (Heading/Caption = Rajdhani Bold 12 · ls 100 is nearest)
    └ Count        TEXT       bind?   │ Auto · HAlign Right · font DECIDE (Data/Value nearest)
```

**MUST NOT contain:** a hairline (the header's separation is the panel's business); hex.

> **Packet prompt:** Add `WBP_RosterHeader` after `WBP_ButtonPrompt`. Parent
> `/Script/Breachpoint.BRRosterHeader`, header `Components/BRRosterPanel.h` (three classes in
> that header — slice by class). Four binds, two font DECIDEs to close in the ticket.

---

### A5 · `WBP_RosterRow` — `UBRRosterRow` (`UCommonButtonBase`), fill × 30

```
RootSizeBox        SizeBox    bind    │ height UNAUTHORED (C++ RowHeight 30 · pitch 35 is the
└ RowOverlay       OVERLAY            │   PANEL's slot padding, never this widget's)
  ├ TeamFill       IMAGE      bind    │ slot inset 2 (ContentInset) · NO brush — C++ tints the
  │                                   │   per-player team colour onto the default white brush
  ├ Border         HAIRLINE   bind?   │ slot FILL · C++ drives opacity 0.3 (BorderOpacity)
  └ Content        HBOX       bind?   │ slot inset 2 · pad (L5, R15) · gap 10 as child padding
    └ Gamertag     TEXT       bind    │ Fill 1.0 · VAlign Center · font Body/Name
                                      │   = Roboto Condensed Medium 14 · ls 0
```

**Deliberately omitted — all art-bearing, all `BindWidgetOptional`, all land with the art pass:**
`Emblem` (26×26), `RankFrame` (30×26 @white/50), `RankInsignia` (26×26 + Medal 3D),
`MicSwitcher` (16×18 ×3 states), `ExternalIcons`/`PartyLeaderIcon`/`CurrentPlayerIcon`
(40×30, gap **−5** deliberate). Six blank white rectangles on every roster row is BP70's D2
times six. The C++ already guards every one on null.

**MUST NOT contain:** the black/white text flip as two widgets — `TextColor` is a C++ tone
decision (`TextToneLuminanceThreshold`); a `UListView` (four fixed slots, pooled by the panel).

> **Packet prompt:** Add `WBP_RosterRow` after `WBP_RosterHeader`. Parent
> `/Script/Breachpoint.BRRosterRow`, same header, slice by class. Five nodes; bind `RootSizeBox`,
> `TeamFill`, `Border`, `Content`, `Gamertag`. Notes must enumerate the seven omitted optional
> binds and the reason (blank-rect law). Nothing here is a DECIDE.

---

## Tier B — small hosts

### B1 · `WBP_NavBar` — `UBRNavBar` (`UCommonUserWidget`), 666 × 30 · hosts `WBP_ButtonPrompt`

**Tabs are not in this WBP.** `SetTabs()` creates `UBRNavTab`s at runtime from `TabWidgetClass`
(a soft class — config, points at `WBP_NavTab`) and slots them into `TabContainer` with C++-owned
padding (`FirstTabOffsetX` 39, `TabPitch` 150, `TabGap` 12). Tab count is data; the WBP ships an
**empty container** and that is correct, not unfinished.

```
RootSizeBox        SizeBox    bind?   │ 666×30 (BarWidth/Height) — UNAUTHORED, C++ owns; the
└ BarHBox          HBOX               │   516 sub-level variant is the SAME asset resized by C++
  ├ BumperPrev     WBP_ButtonPrompt   │ bind? · VAlign Center (27×15 @ y7.5 → centred in 30)
  ├ TabContainer   HBOX       bind    │ Fill 1.0 · EMPTY — C++ fills it (see above)
  └ BumperNext     WBP_ButtonPrompt   │ bind? · VAlign Center
```

**MUST NOT contain:** any `WBP_NavTab` instance (C++-created — a hand-placed tab would bind by
name into `SetTabs`'s array and double-render, the killfeed lesson); per-tab offsets; LB/RB
key art (the prompts' `CommonActionWidget` handles platform).

> **Packet prompt:** Add `WBP_NavBar` after `WBP_ButtonPrompt` (it hosts it — PLAN order law).
> Parent `/Script/Breachpoint.BRNavBar`, slice class `UBRNavBar`. Four nodes + two hosted
> prompts via `wbp_class("WBP_ButtonPrompt")`. `TabContainer` ships empty; say why in notes.
> Config follow-up (terminal lane): point `TabWidgetClass` at `WBP_NavTab`.

---

### B2 · `WBP_FeatureCard` — `UBRFeatureCard` (`UCommonButtonBase`), 349 × 222

```
RootSizeBox        SizeBox    bind    │ 349×222 (CardWidth/Height) — UNAUTHORED, C++ owns
└ CardOverlay      OVERLAY            │
  ├ Ground         IMAGE      bind    │ FILL · NO brush — C++ tints PanelGround50
  ├ CardVBox       VBOX               │
  │ ├ ImageBox     SIZEBOX    bind    │ height 196.7 (ImageHeight) — C++-driven, not authored
  │ │ └ FeatureImage IMAGE    bind    │ FILL · NO brush — the VM pushes a soft path at runtime.
  │ │                                 │ GAP: no T_UI_Feature_Unknown placeholder exists yet —
  │ │                                 │ until it does this renders white in-editor. File it,
  │ │                                 │ don't hide it (LAYOUT-DOCTRINE §3.3)
  │ └ Caption      TEXT       bind    │ h 25.3 (CaptionHeight) · pad DECIDE · font DECIDE
  └ Border         HAIRLINE   bind?   │ FILL
DotsContainer      HBOX       bind?   │ inside CardOverlay, VAlign Bottom · HAlign Center ·
                                      │ EMPTY — dots are C++-created per carousel entry
```

**MUST NOT contain:** hand-placed dots; a 330 width (that is the component *board*, not the
instance); caption hex.

> **Packet prompt:** Add `WBP_FeatureCard` after `WBP_RosterRow`. Parent
> `/Script/Breachpoint.BRFeatureCard`. Eight nodes, six binds (+`DotsContainer`). Two DECIDEs
> (caption font + padding) and one filed gap (feature placeholder texture) — the ticket closes
> the DECIDEs and files the gap against the art pass, then builds.

---

### B3 · `WBP_RecordPanel` — **same parent `UBRFeatureCard`**, 334 × 115 (`MAIN-MENU-INVENTORY` §4.2)

Same tree as B2 — that is the point. The deltas, and only these:

| Node | Delta from B2 |
|---|---|
| `RootSizeBox` | 334 × 115 (`BRFrontEnd::ProgressionButton*` — C++-driven via the screen) |
| `Caption` | the TITLE: **font `Heading/Panel`** = Rajdhani **Bold** 16 · ls 100 — this one IS measured (COMPONENT-SPECS §6: "CAREER RANK … Rajdhani Bold 16 ls 10% + DROP_SHADOW"). String comes from the VM: **"SERVICE RECORD"** (`SCREEN-STRINGS.md` §1.3) |
| `FeatureImage` | the emblem area — art pass (`T_Logo_Mark`); the 167×94 left/right halves are the *art's* internal layout, not widgets |
| `DotsContainer` | the 72×10 switcher rail |

**MUST NOT contain:** a `UBRProgressionButton` class reference (does not exist, will not);
the "GRADE 1" string anywhere (our ladder has no grades); the drop shadow as a second text widget
(it is a style property).

> **Packet prompt:** Add `WBP_RecordPanel` directly after `WBP_FeatureCard` — same
> `parent_class`, same header, same tree shape, geometry and `Heading/Panel` caption per this
> section. One asset, zero new classes. Also (terminal lane, same ticket): retype
> `UBRScreen_FrontEnd::ProgressionButton` from `UWidget` to `UBRFeatureCard`.

---

### B4 · `WBP_RosterPanel` — `UBRRosterPanel` (`UCommonUserWidget`), 349 × 273 · hosts A4

**Rows are not in this WBP.** `RowPool` (`FUserWidgetPool`) creates `WBP_RosterRow`s from
`RowWidgetClass` (soft config) into `RowContainer`, pitch 35 = C++-owned slot padding.

```
RootSizeBox        SizeBox    bind    │ 349×273 (PanelWidthMainMenu/PanelHeight) — C++ owns.
└ PanelOverlay     OVERLAY            │   Lobby's 310 is UNMEASURED (§8 manifest) — same asset
  ├ Ground         IMAGE      bind?   │ slot inset 3 · NO brush — C++ tints PanelGround40
  │                                   │   (COMPONENT-SPECS §6: "boolean #000000@0.4 inset 3")
  ├ PanelBorder    HAIRLINE   bind?   │ FILL · stroke White20 1px INSIDE — C++ sets the style
  └ ContentVBox    VBOX               │ pad 3
    ├ Header       WBP_RosterHeader   │ bind? · h31
    ├ RowContainer VBOX       bind    │ Fill 1.0 · EMPTY — the pool fills it
    └ EmptyStateLabel TEXT    bind?   │ Center · font Body/Flavor (italic — it is flavour text)
                                      │   C++ toggles: 4 slots vs "no lobby yet" are different
```

**Deliberately omitted:** `GradientFill` (needs a gradient texture/material — art pass).

**MUST NOT contain:** hand-placed rows; a friends-list layout — this is **4 Fireteam slots with
empty `Invite…` states** (`SCREEN-MANIFEST` §4.10), and the empty slot is a *row variant the VM
sends*, not a hidden row.

> **Packet prompt:** Add `WBP_RosterPanel` after `WBP_RosterRow` and `WBP_RosterHeader` (hosts
> the header; the row it consumes via soft config, not via the tree). Seven nodes. Notes state
> the omitted `GradientFill` and the pool-fills-`RowContainer` contract. Config follow-up:
> `RowWidgetClass` → `WBP_RosterRow`.

---

### B5 · `WBP_ProfileBar` — `UBRProfileBar` (`UCommonUserWidget`), 1280 × 50

```
RootSizeBox        SizeBox    bind    │ height UNAUTHORED (BarHeight 50). Never y=670 — the bar
└ BarOverlay       OVERLAY            │   is the root layout's LAST CHILD (§7.3 manifest)
  ├ Blur           BLUR       bind?   │ /Script/UMG.BackgroundBlur · FILL — the ONE blur in the
  │                                   │   front end. Strength is C++/style, not authored
  ├ Ground         IMAGE      bind    │ FILL · NO brush — C++ tints PanelGround50
  └ BarHBox        HBOX              │ pad DECIDE (edge insets unmeasured)
    ├ Gamertag     TEXT       bind    │ VAlign Center · font Body/Name
    ├ Status       TEXT       bind?   │ VAlign Center · font DECIDE (Data/Value nearest) —
    │                                 │   "IN MENUS · Invite Only" session states
    └ PromptContainer HBOX    bind?   │ Fill · HAlign Right · EMPTY — C++ fills
```

**MUST NOT contain:** an action bar (`CommonBoundActionBar` lives in `WBP_RootLayout`, once);
a second blur anywhere else in the menu — this is the only one and that is a design statement.

> **Packet prompt:** Add `WBP_ProfileBar` after `WBP_RosterPanel`. Parent
> `/Script/Breachpoint.BRProfileBar`. Add `UBackgroundBlur` to `_BASES` and a `BLUR` class-path
> constant. Two DECIDEs (bar insets, status font). Root-layout integration is a separate
> concern — this ticket only authors the bar.

---

## Tier C — the rail and the screen

### C1 · `WBP_LeftRail` — `UBRLeftRail` (`UCommonUserWidget`), 349 × (data-driven) · hosts B2

**The one component-root `CanvasPanel` in the menu, and it is required, not tolerated:**
`ApplyCaret()` writes `SelectionCaret`'s position through `BRGetCanvasSlot` — the caret slides to
the focused row (rail-local x −4, y computed), and a box layout cannot express "3×65 at an
arbitrary y". Law 6.

**Menu rows are not in this WBP** — the screen's `FUserWidgetPool` fills `MenuRowSlot`.
**The description text is not in this WBP** — C++ fills `DescriptionSlot`.

```
RootSizeBox        SizeBox    bind    │ w349 (RailWidth) — height is ComputeRailHeight(rows,
└ RailCanvas       CANVAS             │   card) — ALL C++, nothing authored
  ├ RailVBox       VBOX               │ canvas slot: FILL (anchors 0,0→1,1, offsets 0)
  │ ├ FeatureCard  WBP_FeatureCard    │ bind? · h222 · pad-bottom 10 (FeatureCardGap)
  │ ├ MenuBorderBox SIZEBOX   bind    │ height C++: ComputeMenuBorderHeight (186 @ 4 rows)
  │ │ └ MenuBorderOverlay OVERLAY     │
  │ │   ├ MenuBorder HAIRLINE bind?   │ FILL — the rail's chrome frame
  │ │   └ MenuRowSlot VBOX    bind    │ slot inset L16 T? R22 (MenuRowSlotInset/…Right) ·
  │ │                                 │   311 × 148 @ 4 rows · EMPTY — pool fills it
  │ └ DescriptionSlot OVERLAY bind?   │ pad-top 55 (DescriptionGap) · h37 · EMPTY — C++ fills
  └ SelectionCaret RULE       bind    │ canvas slot 3×65 @ (−4, 0) placeholder — C++ REWRITES
                                      │   position per focused row; Vertical orientation
```

**Deliberately omitted:** `RevealNotchTop/Bottom` (88 × 4.727 notch art — art pass + anim).

**MUST NOT contain:** a second canvas below the root; any row; any description text; the three
origin Ys (138/286/327 are the *screen's* placement of the rail, not the rail's business).

> **Packet prompt:** Add `WBP_LeftRail` after `WBP_FeatureCard` (hosts it). Parent
> `/Script/Breachpoint.BRLeftRail`. Nine nodes; the root canvas is the documented §0-law-6
> exception — cite `ApplyCaret` in the notes so no reviewer "fixes" it. Bind `RootSizeBox`,
> `FeatureCard`, `MenuBorderBox`, `MenuBorder`, `MenuRowSlot`, `DescriptionSlot`,
> `SelectionCaret`; omit the two notches with reason. `MenuRowSlot`'s top inset: read the header
> before typing — only L16/R22 are named constants (one DECIDE).

---

### C2 · `WBP_Screen_FrontEnd` — `UBRScreen_FrontEnd` (`UBRActivatableWidget`) · hosts C1, B1, B3, B4

**PREREQUISITE (terminal lane, before this asset):** cut `FBRFrontEndTabLayout` /
`ApplyTabLayout` per `MAIN-MENU-INVENTORY.md` §4.1. Until that lands, the C++ demands a root
`CanvasPanel` this tree correctly refuses to have. **Do not build this asset against the uncut
class** — the bands tree + the canvas contract would fail at asset load.

```
ScreenSafeZone     SAFEZONE           │ the outer margin — platform title-safe for free (§8.4)
└ BandsVBox        VBOX               │ NO CanvasPanel on this screen (LAYOUT-DOCTRINE §6)
  ├ HeaderBand → NavBar  WBP_NavBar   │ bind · Auto · HAlign Left · pad-bottom 63
  │                                   │   (nav y45 → rail y138, gap 63 — the measured band gap)
  ├ ContentBand    HBOX               │ Fill 1.0
  │ ├ Col1_Menu    OVERLAY            │ Fill 1.0 · pad-right 24        ← half the 48 gutter
  │ │ └ LeftRail   WBP_LeftRail       │ bind · HAlign Left · max-w 348.67 via the rail's own box
  │ ├ ContentSlot  OVERLAY    bind?   │ Fill 1.0 · pad 24 · EMPTY — this IS column 2: it
  │ │                                 │   reserves the subject's space AND is the per-tab
  │ │                                 │   content bind. One node, both jobs
  │ └ Col3_Status  VBOX               │ Fill 1.0 · pad-left 24 · HAlign Right (children max-w
  │   ├ ProgressionButton WBP_RecordPanel │ bind? · h115 · pad-bottom 24        348.67)
  │   └ PartyList  WBP_RosterPanel    │ bind? · HUG
  └ EmptyStateLabel TEXT     bind?    │ Center · font Body/Flavor — "which kind of nothing":
                                      │   never-told vs told-and-empty (two different facts)
```

*(No FooterBand: profile bar + action bar live in `WBP_RootLayout`. `TabSwapAnim` is
`BindWidgetAnimOptional` — the generator cannot author animations; omitted, C++ guards null.)*

**MUST NOT contain:** a `CanvasPanel`; a `TabSwitcher` or tab pages (tabs swap VM data);
the profile bar or prompts; the player stage (480,118 320×602 is the CAMERA's box — §4.1
manifest: "not a widget"); any of the 869/55/862/397 coordinates — they are what the bands
replace.

> **Packet prompt:** LAST in PLAN — after `WBP_NavBar`, `WBP_LeftRail`, `WBP_RecordPanel`,
> `WBP_RosterPanel`. Parent `/Script/Breachpoint.BRScreen_FrontEnd`. Ten nodes, four hosted.
> Gate on the §4.1 C++ cut landing first — say so in the ticket's Kickoff as a `requires:` line.
> Config follow-ups: `UBRUISettings::MainMenuScreenClass` → this asset;
> `MenuRowWidgetClass` → `WBP_MenuRow`. Rung honesty: built + opens = rung 1; menu on screen in
> PIE with a wired `UBRVM_FrontEnd` = rung 2; nothing above that is claimable from this packet.

---

## The DECIDE ledger — 8 open values, none inventable

| # | Where | What | Nearest |
|---|---|---|---|
| 1 | A3 `Verb` | prompt verb font | `Label/Micro` |
| 2 | A3 | glyph→verb gap | — |
| 3 | A4 `Label` | roster header font | `Heading/Caption` |
| 4 | A4 `Count` | count font | `Data/Value` |
| 5 | B2 `Caption` | feature caption font + padding | — |
| 6 | B5 | profile bar edge insets | — |
| 7 | B5 `Status` | status font | `Data/Value` |
| 8 | C1 `MenuRowSlot` | top inset (only L16/R22 named) | read the header first |

Each ticket closes its own rows **before** its build runs. A DECIDE closed by the builder
mid-build is a measurement invented under deadline, which is how 44-vs-33 happened (§1 manifest).
