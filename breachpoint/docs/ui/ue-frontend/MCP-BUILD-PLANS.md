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
  ├ ActionGlyph    ACTION_GLYPH bind  │ 20 × 20 (BP73, node 0:10) · VAlign Center
  │                                   │ draws the PLATFORM's glyph for a bound input action —
  │                                   │ zero brushes, zero per-platform art. This is the whole
  │                                   │ reason the bind is CommonActionWidget and not UImage
  └ Verb           TEXT       bind    │ pad-left 10 (BP73: glyph ends x=20, text starts x=30,
                                      │ and the auto-layout gap reads 10 — two independent
                                      │ confirmations) · VAlign Center · h17 in a 20 box
                                      │ font MEASURED: Roboto Condensed **Bold** 14 · ls 0 ·
                                      │ white. NOT `Label/Micro` — the guess was wrong by
                                      │ family, weight and 4px. See the off-system finding below
```

> **Token finding (BP73).** `Roboto Condensed Bold 14` **is not in `figma_tokens.json`** — the
> nearest rows are `Body/Name` (Roboto Cond **Medium** 14) and `Data/Value` (Roboto Cond SemiBold
> 12). Do not add a token row from one node read (BP73 Out of scope); build the verb with the
> literal measured values and leave the token question for a style sweep.

**MUST NOT contain:** any glyph `Image` (the entire point of `UCommonActionWidget` is that the
platform supplies the art); a fixed width.

> **Packet prompt:** Add `WBP_ButtonPrompt` after `WBP_NavTab`. Parent
> `/Script/Breachpoint.BRButtonPrompt`. Three nodes: `RootSizeBox`, `PromptHBox`, then
> `ActionGlyph` (`/Script/CommonUI.CommonActionWidget`) and `Verb` (font: DECIDE first — record
> the ruling in the plan comment). Add `UCommonActionWidget` to `_BASES`. Two DECIDEs (verb font,
> glyph→verb gap) must be closed in the ticket before building.

---

### A4 · `WBP_RosterHeader` — `UBRRosterHeader` (`UCommonUserWidget`), 349 × 31 in-panel

**MEASURED 3 Aug 2026 (BP73, node `927:43301` `Roster Group Header` on `RS_Friends`).** The 31
is not one band — it is **28 + 3 + a 1px line**, and the header's separation IS its own business
after all (the MUST-NOT below is corrected). The `Ground` is not a flat tint either: it is a
**vertical gradient white 0.10 → white 0.30**, top to bottom, on the text band only.

```
RootSizeBox        SizeBox    bind    │ height UNAUTHORED (C++ HeaderHeight 31 = 28 + 3 + 1)
└ HeaderVBox       VBOX               │ gap 3 as slot padding (measured)
  ├ BandOverlay    OVERLAY            │ Auto · h 28
  │ ├ Ground       IMAGE      bind    │ slot FILL · NO brush — C++ drives the white10→white30
  │ │                                 │   VERTICAL gradient (measured). A flat tint is wrong
  │ └ HeaderHBox   HBOX               │ pad (10, 0, 10, 0) — MEASURED px-10 · VAlign Center
  │   ├ Label      TEXT       bind    │ Auto · font MEASURED: Rajdhani **Bold 18** · ls 100
  │   │                               │   (tracking 1.8px on 18px = 10%) · UPPER · white.
  │   │                               │   NOT `Heading/Caption` 12 — the guess was 6px short
  │   └ Count      TEXT       bind?   │ Fill 1.0 · HAlign/Justify RIGHT · h21 · opacity 0.80 ·
  │                                   │   font MEASURED: Rajdhani **SemiBold 18** · ls 0.
  │                                   │   NOT `Data/Value` — wrong family, weight AND size
  └ BottomLine     HAIRLINE  bind?    │ Auto · h1 · full width — the separator IS part of the
                                      │   header (corrected; see above)
```

> **Token finding (BP73).** Neither `Rajdhani Bold 18` nor `Rajdhani SemiBold 18` exists in
> `figma_tokens.json` — the ladder jumps `Heading/Panel` 16 → `Display/Title` 20. **Two of the
> four font DECIDEs landed off-system**, which is a finding about the token extraction, not a
> licence to invent a row. Build with the literal values; flag the gap to the style sweep.

**MUST NOT contain:** hex; the count as a fixed-width box (it is the fill slot, right-justified —
that is how the label and count share the band); a second gradient below the line.

> **Packet prompt:** Add `WBP_RosterHeader` after `WBP_ButtonPrompt`. Parent
> `/Script/Breachpoint.BRRosterHeader`, header `Components/BRRosterPanel.h` (three classes in
> that header — slice by class). Six nodes now — the tree gained a `VBox` and a `BottomLine` when
> BP73 read the real node. **Zero DECIDEs left**; both fonts are measured and both are off-system.

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

> **RESOLVED 3 Aug 2026 (BP73 row 9).** Both bumper prompts are **direct children of the 666×30
> `Navigation Bar` `124:1179`** — `0:18` at x=27 and `0:37` at x=639, each 27×15 at y=7.5. So both
> ARE bar-local and there is no source inconsistency. What there IS: tabs sit at x=39/189/339/489,
> so the left glyph (27..54) **overlaps the first tab by 15px in the reference**. That is an
> authored overlap on an absolute-positioned frame, and **an `HBox` cannot reproduce it** — an
> HBox lays out side by side. The tree below deliberately does not overlap. Founder call if the
> overlap is wanted: it needs an `Overlay`, not a fix to these numbers.

```
RootSizeBox        SizeBox    bind?   │ 666×30 (BarWidth/Height) — UNAUTHORED, C++ owns; the
└ BarHBox          HBOX               │   516 sub-level variant is the SAME asset resized by C++
  ├ BumperPrev     WBP_ButtonPrompt   │ bind? · VAlign Center (27×15 @ y7.5 → centred in 30) ·
  │                                   │   slot pad left 27 (measured origin, BP73 node 0:18)
  ├ TabContainer   HBOX       bind    │ Fill 1.0 · EMPTY — C++ fills it (see above)
  └ BumperNext     WBP_ButtonPrompt   │ bind? · VAlign Center · ends flush at 666 (639 + 27)
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

> **CORRECTED 3 Aug 2026 (BP73 row 5), node `1769:23147` `News Button` (349×222).** The caption
> is **not a band stacked under the image.** The reference composes it as an overlay: the image
> is full-bleed at inset 7, a transparent→black vertical gradient sits on top of it from the
> midpoint down, and the caption's 40-tall text frame is anchored to the bottom of that same
> inset-7 box. A stacked `VBox` would put the caption on a solid strip below the art, which is a
> different card. **The dots are also not "inside CardOverlay"** — they own the bottom 22 of the
> 222; the image button region is `inset [0,0,22,0]` = 200 tall.

```
RootSizeBox        SizeBox    bind    │ 349×222 (CardWidth/Height) — UNAUTHORED, C++ owns
└ CardVBox         VBOX               │
  ├ ButtonRegion   OVERLAY            │ Fill 1.0 → 200 tall (222 − 22)
  │ ├ Border       HAIRLINE   bind?   │ FILL · the 1px frame + its 12px corner returns
  │ ├ Ground       IMAGE      bind    │ slot inset 7 · NO brush — C++ tints PanelGround50
  │ │                                 │   (MEASURED #000000 @ 0.5, inset 7 — matches the token)
  │ ├ FeatureImage IMAGE      bind    │ slot inset 7 · FILL · scaling COVER (the reference clips
  │ │                                 │   overflow) · NO brush — VM pushes a soft path.
  │ │                                 │   GAP: no T_UI_Feature_Unknown placeholder exists yet —
  │ │                                 │   until it does this renders white in-editor. File it,
  │ │                                 │   don't hide it (LAYOUT-DOCTRINE §3.3)
  │ ├ Scrim        IMAGE      bind?   │ slot inset 7 · FILL · transparent→black VERTICAL gradient
  │ │                                 │   starting at 50% — this is what makes the caption legible
  │ │                                 │   over art. Needs a gradient material → ART PASS, and the
  │ │                                 │   card ships without it rather than faking a flat scrim
  │ └ CaptionBox   SIZEBOX    bind?   │ slot inset 7 · VAlign BOTTOM · h 40 (MEASURED)
  │   └ Caption    TEXT       bind    │ pad (20, 10, 20, 10) — MEASURED px-20 py-10 ·
  │                                   │   VAlign Center · font MEASURED = **`Label/Button`**
  │                                   │   (Rajdhani SemiBold 16 · ls 100 · UPPER · white) —
  │                                   │   the ONE font of the four that IS on-system
  └ DotsContainer  HBOX      bind?    │ Auto · h 22 · HAlign Center · pad (12, 2) · gap 6 ·
                                      │   EMPTY — dots are C++-created per carousel entry,
                                      │   6×6 each; the two end chevrons are 4×3 (MEASURED)
```

**`ImageHeight = 196.7f` (`BRFeatureCard.h:62`) is suspect.** It traces the doc sentence "image
349 × 196.7 + caption", and the only 196.709 node in the file is **`Preview Photo` `0:1027`,
which is hidden**. The visible card has no 196.7 anywhere — its image is inset 7 inside a 200-tall
region (so 186). `SetHeightOverride(ImageHeight)` in `BRFeatureCard.cpp:28` is sizing to a hidden
layer. **Not changed here** — it is a C++ constant with two readers and belongs to BP71's packet,
not to a Figma read pass. Filed, not fixed.

**MUST NOT contain:** hand-placed dots; a 330 width (that is the component *board*, not the
instance); caption hex; a stacked image-over-caption `VBox` (the correction above); a flat black
rectangle standing in for the gradient scrim.

> **Packet prompt:** Add `WBP_FeatureCard` after `WBP_RosterRow`. Parent
> `/Script/Breachpoint.BRFeatureCard`. Eight nodes, six binds (+`DotsContainer`). **Zero DECIDEs
> left** — caption font and padding are measured (BP73, node `1769:23147`). Two filed gaps go
> with it: the placeholder texture and the gradient scrim material, both art-pass. Build the card
> without the scrim; do not fake it.

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

### B5 · `WBP_ProfileBar` — **DEFERRED to the root-layout chrome packet** (CPP-AUDIT §3)

> `UBRProfileBar` was cut: zero referencing classes, `SetIdentity` has zero callers, and
> `WBP_RootLayout` has **no chrome slot for the bar to live in yet**. The class, the slot and the
> caller land together in one chrome packet. The tree below is kept as that packet's spec.

> **CORRECTED 3 Aug 2026 (BP73 row 6).** The tree that stood here — a full-width `HBox` with the
> gamertag left and prompts right — **was not what the reference authors.** `Profile Bar`
> `119:1525` is 1280×50 and contains exactly ONE child: `Player Card` at **x=862, 349×50**. That
> is column 3's origin and column 3's width. The bar is not a bar of content; it is a
> **right-aligned 349-wide card on an otherwise empty 50px band**, and the band exists so the
> card lines up with the roster panel above it. Building the old tree would have stretched a
> 349-wide design across 1280.

Measured internals of `Player Card` (all card-local, BP73):

| Node | Origin | Size | Note |
|---|---|---|---|
| `Superintendent` | (5, 5) | 40 × 40 | avatar — square, 5px inset, so 50 − 5 − 40 = 5 bottom |
| `Gamer and Service Tag` | (55, 17) | 107 × 17 | 55 = 5 + 40 + **10 gap**; contains `Service Tag` **hidden** |
| `Buttons` | (211, 0) | 122 × 50 | full-height block, three dividers; 211 + 122 = 333, so **16 right inset** |

```
RootSizeBox        SizeBox    bind    │ 1280×50 band (BarHeight 50). Never y=670 — the bar is
└ BarHBox          HBOX               │   the root layout's LAST CHILD (§7.3 manifest)
  ├ (spacer)                          │ Fill 1.0 · NO WIDGET — the empty 862 left of the card is
  │                                   │   fill, not a sized box. Do not author 862 anywhere
  └ CardSizeBox    SIZEBOX   bind     │ 349 wide (PanelWidthMainMenu — the SAME constant as the
    └ CardOverlay  OVERLAY            │   roster panel; that is why they align)
      ├ Blur       BLUR      bind?    │ /Script/UMG.BackgroundBlur · FILL — the ONE blur in the
      │                               │   front end. Strength is C++/style, not authored
      ├ Ground     IMAGE     bind     │ FILL · NO brush — C++ tints PanelGround50
      └ CardHBox   HBOX               │ pad (5,5,16,5) — MEASURED, no longer a DECIDE
        ├ AvatarBox SIZEBOX bind?     │ 40×40 · VAlign Center
        │ └ Avatar  IMAGE    bind     │ FILL · NO brush — VM pushes a soft path
        ├ Gamertag  TEXT     bind     │ slot pad left 10 (measured) · VAlign Center ·
        │                             │   font Body/Name · h17
        └ PromptContainer HBOX bind?  │ Fill · HAlign Right · EMPTY — C++ fills. This is the
                                      │   reference's 122-wide `Buttons` block; its three
                                      │   dividers are per-prompt chrome, not bar widgets
```

**Dropped from the old tree:** `Status`. The reference's second line (`Service Tag`) is **hidden
in the source**, so there is no measured status line to build and DECIDE row 7 dies with it. If
the founder wants "IN MENUS · Invite Only" it is a new design, not a transcription.

**MUST NOT contain:** an action bar (`CommonBoundActionBar` lives in `WBP_RootLayout`, once);
a second blur anywhere else in the menu — this is the only one and that is a design statement;
a hard 862 offset; the three `Buttons` dividers as widgets.

> **Packet prompt:** Add `WBP_ProfileBar` after `WBP_RosterPanel`. Parent
> `/Script/Breachpoint.BRProfileBar`. Add `UBackgroundBlur` to `_BASES` and a `BLUR` class-path
> constant. **Zero DECIDEs left** — the card geometry is measured (BP73, node `119:1525`); build
> the spacer as a fill slot, never as a sized 862. Root-layout integration is a separate concern
> — this ticket only authors the bar.

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
  │ ├ Col2_Subject OVERLAY    UNBOUND │ Fill 1.0 · pad 24 · EMPTY — reserves the subject's
  │ │                                 │   space. (Amended per CPP-AUDIT §3: the ContentSlot
  │ │                                 │   bind died with the FBRFrontEndTabLayout cut — the
  │ │                                 │   per-tab content region returns WITH data, not before)
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

## The DECIDE ledger — **all 9 closed** (BP73, 3 Aug 2026)

Every row names the node it was read from, per BP73's Done-when: *a closed row must name the node
it was read from, or the next person cannot check it.*

| # | Where | Closed value | Read from |
|---|---|---|---|
| 1 | A3 `Verb` | **Roboto Condensed Bold 14 · ls 0 · white** — OFF-SYSTEM (no such token row) | `119:1491` `Button Prompts` design context |
| 3 | A4 `Label` | **Rajdhani Bold 18 · ls 100 · UPPER** — OFF-SYSTEM | `927:43301` `Roster Group Header` |
| 4 | A4 `Count` | **Rajdhani SemiBold 18 · ls 0 · opacity 0.8 · right** — OFF-SYSTEM | same node |
| 5 | B2 `Caption` | **`Label/Button`** (Rajdhani SemiBold 16 · ls 100 · UPPER) · pad (20,10,20,10) | `1769:23147` `News Button` |
| 2 | A3 glyph→verb gap | **10px** — glyph ends x=20, verb starts x=30; the auto-layout gap also reads 10 | `0:9` `Menu` variant of `Button Prompts` `119:1491`; glyph `0:10` @ (0,0) 20×20, text `0:16` @ (30, 1.5) 32×17 |
| 6 | B5 profile bar insets | **(5,5,16,5)** on a 349-wide card, avatar→name gap 10 — and the premise was wrong: the bar is a right-aligned card, not a full-width row (§B5) | `Profile Bar` `119:1525` → `Player Card` @ x=862 349×50 |
| 7 | B5 `Status` font | **DEAD** — the reference's second line `Service Tag` is hidden in the source; there is no status line to transcribe | same node |
| 8 | C1 `MenuRowSlot` | **16 uniform**, not 16/22 — and `BRLeftRail.h`'s unread `MenuRowSlotInsetRight = 22` was wrong and is deleted | `Contents` `0:1183` @ (16,16) 311 wide inside 343-wide `Menu List` `0:1176`; 343−16−311=16 |
| 9 | B1 bumper origins | **Both bar-local, no conflict.** The 15px overlap is authored; an `HBox` can't express it → founder call, not a number (§B1) | `0:18` x=27 and `0:37` x=639 as direct children of `Navigation Bar` `124:1179` |

**Three of the four fonts landed off-system** — `Roboto Cond Bold 14`, `Rajdhani Bold 18` and
`Rajdhani SemiBold 18` are all absent from `figma_tokens.json`, whose ladder jumps 16 → 20 and
carries Roboto Condensed only at Medium 14 / SemiBold 12. That is a **finding about the token
extraction**, recorded per BP73 step 2, and explicitly **not** a licence to add three rows from
three node reads. Build with the literal measured values; a style sweep decides whether the token
file grew a gap or the reference nodes are genuinely off-system.

Note what closing the ledger cost: **four of the nine rows corrected something that was already
written down** — a constant (`MenuRowSlotInsetRight = 22`), two plan trees (§B5's bar, §B2's
stacked caption) and a suspect C++ constant (`ImageHeight = 196.7`, filed for BP71). A DECIDE
closed by the builder mid-build is a measurement invented under deadline, which is how 44-vs-33
happened (§1 manifest) — and every one of those four would have shipped.
