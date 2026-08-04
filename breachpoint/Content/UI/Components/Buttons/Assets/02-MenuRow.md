# Menu Row — measured breakdown (Figma `12:724`)

Source: file `yznvnVdOFDADaugZSeomfP`, page `Buttons & Rows` (`6:25`), component set `Menu Row`
(`12:724`). Read via the Figma Plugin API, 4 Aug 2026. **27 variants on three axes: Status ×
Alignment × Type.** UE class: `UBRMenuRow` (`Source/Breachpoint/UI/Components/BRMenuRow.h`).

---

## The shell, common to every variant

```
COMPONENT              250 x 28          (Icon Only 40x40 · Map Voting 250x60 · Image 250x120)
  Text Frame   FRAME   246 x 24
      IDLE  at (2,2)   no fill
      HOVER at (-1,0)  fill #ffffff        <- the inversion
    autolayout HORIZONTAL, gap 10, padding 0/10/0/10   (Alignment=Center -> padding 0/0/0/0)
    Icon         INSTANCE  16 x 16   HIDDEN in every variant
    Text         TEXT      "BUTTON"  Rajdhani SemiBold 16px
    Selection    TEXT      "SELECTION" — HIDDEN on Default; visible on value-carrying types
    Filter Button GROUP    20 x 20   HIDDEN
    Border       FRAME     250 x 28 at (-2,-2)
      Top Line     VECTOR  250 x 2   HIDDEN when idle, visible on hover
      Bottom Line  VECTOR  250 x 2   @0.3 idle -> full on hover
      Right Line   VECTOR    0 x 20  @0.3, HIDDEN when idle
      Left Line    VECTOR    0 x 20  @0.3, HIDDEN when idle
      Background Line VECTOR 250 x 2 @0.3   <- HOVER ONLY; absent from the idle tree
```

**The C++ matches this.** `UBRMenuRow`'s stated model — plate goes solid white, label goes black,
bottom line 0.3 → 1.0, side ticks 20 tall on a 28 row, hover-only background line — is confirmed
variant by variant. `RowHeight 28`, `BorderSideTickLength 20`, `IconOnlySize 40`,
`MapVotingHeight 60`, `ImageHeight 120` are all correct.

**Note the contrast with `Highlight Button`,** whose hover is the type accent, not white. Menu Row
really does invert to `#ffffff`; Highlight Button does not. Two components, two rules — see
`01-HighlightButton.md`.

---

## Per-type anatomy

### Default
Text only. `Selection` hidden. Idle `Border` frame at full opacity.

### Disabled
Identical geometry to Default; **every child carries `opacity 0.5`** (Icon, Text, Selection, Filter
Button). Geometry unchanged. Confirms `DisabledOpacity = 0.5f`.

### Alignment = Center
Padding drops to `0/0/0/0`, Text centred at x=93 (60 wide in a 246 box). Idle `Border` frame is at
**`opacity 0.3`** as a whole — the only variant where the border frame itself is dimmed rather than
the individual lines.

### Drop Down
`Text` widens to 108. A `Text and Icon` sub-frame (108 x 21, autolayout H, **gap 8**) holds the
`SELECTION` text (84 wide) plus `Polygon 13`, a **6 x 6 triangle** glyph.
- Idle/Hover: triangle at y=8.
- **Active: triangle at y=14** — it moves down 6px. This is the "disclosure glyph rotates" note in
  the C++; measured, it is a 6px translate of a 6x6 polygon, and the WBP's `DisclosureAnim` should
  be authored to that.

### Drop Down (Active) and Dig Down — THE HATCH
Both carry a `Highlight` frame (246 x 24, `opacity 0.5`) containing:
- `Lines` — an autolayout frame with **69 `LINE` nodes**, each 51 long, stroke `#ffffff` at
  **0.5 weight**, spaced 3px apart, with `gap -33` producing the diagonal shear.
- `Rectangle 154` — 110 x 24, linear gradient `#000000 → #000000` at `@0.8` (a fade mask).

**This is the one thing in the whole set that should be a TEXTURE.** 69 vector lines is not
something to rebuild as 69 UMG widgets — it is a tiling diagonal hatch. Options: a small tiling PNG
with a gradient-masked `UImage`, or a material. Everything else on this page UMG draws natively.

`Dig Down` additionally has an `Arrows` group (9 x 6) outside the Text Frame at x=277 — a solid
`Polygon 9` plus a `Subtract` boolean at `opacity 0.5`, i.e. a chevron with two ghosted trailing
chevrons.

### Icon Only
40 x 40 shell, `Text Frame` 36 x 36 with **padding 2 on all sides**, gap 0. Holds a 32 x 32 icon
instance. Border lines are 40 long, side ticks 32.

### Slider — DIFFERENT TYPE RAMP, worth flagging
```
Text & Slider  FRAME  246x24  autolayout H, gap 25, padding 0/10/0/10
  Text Frame   FRAME   62x24  gap 10
    Text       TEXT    "BUTTON"  Rajdhani DEMI 14px      <- NOT SemiBold 16px
  Slider & Percentage FRAME 141x19  gap 16
    Slider     FRAME   100x6
      Line     LINE    100 long, stroke #ffffff@0.8 1px
      Circle   ELLIPSE   6x6  fill #ffffff               <- the handle
      Polygon 15 VECTOR  4x4  at y=-6                    <- tick marker ABOVE the track
    50         TEXT     19x17  Roboto Condensed Medium Italic 14px, @0.8
```
On hover everything flips to `#000000` **except** the handle `Circle`, which becomes
`fill #000000` with a `#ffffff` 1px stroke — it keeps a white ring so it stays visible on the
inverted plate. That is a real detail worth preserving.

**The `Demi 14px` label is a deviation from the 16px SemiBold used by every other type.** Either the
slider row is deliberately quieter, or it is a drift in the Figma file. Not resolved here — flagged.

### Checkbox (4 states: Idle / Active / Hover / Active Hover)
`Text` widens to 200; a 16 x 16 `Checkbox` instance sits at x=220.
- **Idle/Hover:** `Vector` RECT 16x16, stroke only (1px). An empty square.
- **Active:** boolean — `Rectangle 132` 16x16 filled `#ffffff`, plus `Line 21` 11x5 stroke
  `#000000` at **weight 2** — the tick.
- Idle border frame at `opacity 0.3`; hover at full.

### Radio (4 states) — IT IS A SQUARE
Same layout as Checkbox. **The `Vector` is a `RECT`, not an ellipse** — this design system's radio
is square, consistent with the flat/sharp-corner rule (`ui-presentation` §4).
- Idle/Hover: 16x16 rect, stroke only.
- Active: `Outline` rect 16x16 stroke + `Fill` rect **10 x 10 at (3,3)** filled — a 3px inset.

---

### Map Voting (4 states: Idle / Idle Winning / Hover / Hover Winning)
250 x 60 shell, `Text Frame` 246 x 56.
```
Text Stacked        FRAME 173x44 at (10,6)  autolayout VERTICAL, gap 2
  Gametype          FRAME 173x21            autolayout HORIZONTAL, gap 4
    Slayer          INSTANCE 20x20          <- gametype emblem (concentric ring boolean)
    Text            TEXT 149x21  "BUTTON"   Rajdhani SemiBold 16px
  Text              TEXT 173x21  "SELECTION" Rajdhani SemiBold 16px
Checkbox and Counter FRAME 43x24 at (193,16) autolayout HORIZONTAL, gap 10
  6                 TEXT  9x20   the vote count, SemiBold 16px
  Radio Button      INSTANCE 24x24          <- 24px here, NOT the 16px used elsewhere
```
**"Winning" is a fill, not a border change.** Idle Winning puts a linear gradient
`#000000 → #2ec3e5` on the whole `Text Frame` and turns `Bottom Line` `#2ec3e5`. Hover Winning keeps
the white hover plate but still draws `Bottom Line` in `#2ec3e5` — so winning survives hover, which
is the correct read (you must not lose the vote state by pointing at it).

### Image (2 states: Idle / Hover)
250 x 120 shell, `Text Frame` 246 x 116, **gap 0, padding 0/10/0/10**.
```
Image     INSTANCE 120x120 at (10,-2)   <- overhangs the frame top by 2px, deliberately
Text      TEXT      90x21  at (130,48)  Rajdhani SemiBold 16px
Checkbox  INSTANCE  16x16  at (220,50)  16x16 rect, stroke only
Border    FRAME    250x120              idle frame @0.3
```
The `Image` instance in the file is **placeholder emblem art** — a `Shield`, two `Sword` frames and
a `Skull`, all boolean vector constructions with `#d9d9d9` fills. That is gametype-emblem artwork
standing in for real content; it is not part of the button and does not belong in this folder. On
hover its fills invert (`#ffffff` → `#000000` and vice versa), which tells you the emblem must be
tintable — i.e. a single-channel mask, not baked colour art.

---

## Coverage

**All 27 variants captured.** The first extraction returned 21 before hitting the tool's 20 kB
response cap; the remaining six (`Radio Active Hover`, `Map Voting` ×4, `Image` ×2) were fetched in
a second pass with the 69-node hatch collapsed so it could not eat the budget again.

---

## Textures needed

| Item | Verdict |
|---|---|
| Plates, borders, ticks, text, triangle, chevrons, checkbox, radio, slider track/handle | **None.** UMG draws all of it. `ui-presentation` §8: export nothing UMG can draw. |
| `Highlight` diagonal hatch (Drop Down Active, Dig Down) | **Texture or material.** 69 line nodes; a tiling hatch is the correct form. |
| `Icon` / `Revert` glyphs | Already the icon pipeline's job — `Art / UI Glyphs` page (`80:2`), `Tools/gen_ui/figma_export.py`. Not button assets. |
