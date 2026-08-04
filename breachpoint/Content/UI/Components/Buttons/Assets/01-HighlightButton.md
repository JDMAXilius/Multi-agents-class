# Highlight Button — measured breakdown (Figma `12:1194`)

Source: file `yznvnVdOFDADaugZSeomfP`, page `Buttons & Rows` (`6:25`), component set `Highlight Button`
(`12:1194`). Read via the Figma Plugin API (`use_figma`), 4 Aug 2026. **13 variants on two axes:
Status × Type.** UE class: `UBRHighlightButton` (`Source/Breachpoint/UI/Components/BRHighlightButton.h`).

Every number here is read from the node graph, not measured off a screenshot. Colours are the
resolved paint values; `@n` is paint opacity.

---

## The anatomy, which is identical across all 13

```
COMPONENT                     250 x 28      (Photo Button: 349 x 72)
  Button Frame     FRAME      246 x 24 at (2,2)     <- the plate. Idle inset 2px; Hover at (-1,0)
    autolayout HORIZONTAL, gap 10, padding 0/10/0/10, primary CENTER, counter CENTER
    Button Prompts INSTANCE   16 x 16     HIDDEN in every variant
    BUTTON         TEXT       60 x 21     Rajdhani SemiBold 16px, letter-spacing 10%
    Border         FRAME      250 x 28 at (-2,-2)   <- four separate 1px VECTOR lines
      Top Line     VECTOR     250 x 2   stroke 1px CENTER
      Bottom Line  VECTOR     250 x 2   stroke 1px CENTER   <- carries the TYPE accent
      Right Line   VECTOR       0 x 20  stroke 1px CENTER @0.3
      Left Line    VECTOR       0 x 20  stroke 1px CENTER @0.3
      Background Line VECTOR  250 x 2   stroke #ffffff @0.3  <- HOVER ONLY, absent when idle
```

**Confirms three things already in the C++ and worth keeping:** the border is four vector lines and
never a closed box; side lines are 20 tall on a 28 row (ticks, not edges); the extra
`Background Line` at 0.3 appears only on hover.

---

## THE PAINT TABLE — this is the part that matters

| Status | Type | Button Frame fill | Text | Bottom Line |
|---|---|---|---|---|
| Idle | Main | `#000000@0.8` + `linear[#000000@0.3 → #2ec3e5]@0.8` + `#000000@0.2` | `#ffffff` | `#2ec3e5` |
| Hover | Main | `#2ec3e5` solid | `#000000` | `#2ec3e5` |
| Idle | Event | `#000000@0.8` + `linear[#000000@0.3 → #ff5c00]@0.8` + `#000000@0.2` | `#ffffff` | `#ff5c00` |
| Hover | Event | `#ff5c00` solid | `#000000` | `#ff5c00` |
| Idle | Premium | `#000000@0.8` + `linear[#000000@0.3 → #ffed4f]@0.8` + `#000000@0.2` | `#ffffff` | `#ffc11c` |
| Hover | Premium | `#ffc11c` solid | `#000000` | `#ffc11c` |
| Idle | Boring | *(none)* | `#ffffff` | `#ffffff` |
| Hover | Boring | `#ffffff` solid | `#000000` | `#ffffff` |
| Idle | Disabled | *(none)* | `#ffffff@0.5` | `#ffffff@0.3` |
| Hover | Disabled | `#ffffff@0.3` | `#ffffff@0.3` | — (single `Border` RECTANGLE, `#ffffff@0.5`, 1px INSIDE) |
| On Click | Disabled | `#ff4b4b` solid | `#000000` | — (single `Border` RECTANGLE, `#ffffff@0.5`, 1px INSIDE) |
| Idle | Photo Button | `#000000@0.8` + `linear[#000000@0.3 → #2ec3e5]@0.8` + `#000000@0.2` | `#ffffff` | `#2ec3e5` |
| Hover | Photo Button | `#2ec3e5` solid | `#000000` | `#2ec3e5` |

**Gradient geometry:** linear, stop 0 at `#000000@0.3`, stop 1 at the type's accent at full alpha,
whole paint at `@0.8`. It sits between a flat `#000000@0.8` beneath and a `#000000@0.2` above — three
stacked paints, in that order, not one blended colour.

---

## TWO FINDINGS AGAINST THE SHIPPED C++

**1. Hover is NOT a white inversion.** `BRHighlightButton.h` states the state model as *"the fill
plate goes from nothing to solid white (`SurfaceInverted`), the label goes black
(`TextInverted`)"*, and `InvertedFillToken` defaults to `EBRUIColorToken::SurfaceInverted`. The
measured file disagrees for 5 of 6 types: hover fill is the **type's own accent**
(`#2ec3e5` / `#ff5c00` / `#ffc11c`), not white. Only `Type=Boring` actually inverts to white — which
is presumably where the white rule came from. The label going black IS correct in every case.

**2. The ACCENT GAP is closable and the gap note is now stale.** That header says
*"`ResolveIdleFillToken` still returns `None` for Event and Premium and they render as Main."* The
file has distinct idle treatments per type — the gradient's stop-1 colour is the differentiator —
so there is now a measured source for all six. Note `Premium` uses **two** yellows: `#ffed4f` in the
idle gradient, `#ffc11c` for the hover fill and the bottom line. `BR::Tokens::AccentPremium` is
`#ffc11c`; the lighter `#ffed4f` has no token yet.

**Neither is fixed here.** This document is the measurement; changing `UBRHighlightButton` is a code
change with its own review, and `ui-presentation` §8's one-way rule says Figma is right about
appearance — so the C++ is what moves.

---

## New token needed

| Proposed | Value | Why |
|---|---|---|
| `AccentPremiumTint` | `#ffed4f` | Idle-gradient partner for `AccentPremium` (`#ffc11c`). Same tint/base pairing the rarity ramp already uses. |

`#ff4b4b` (Disabled/On Click) is close to but NOT the same as `AccentDanger` — check
`BRUITokens.h` before assuming they are one value.

---

## What needs a TEXTURE, and what does not

**Nothing in 11 of the 13 variants.** Plates, gradients, 1px lines and text are all things UMG draws
— `ui-presentation` §8: *"export nothing UMG can draw."* The gradient is a `UImage` with a linear
gradient brush or a small material; it is not a PNG.

**The two `Photo Button` variants are the exception.** They carry a real image:

```
Image [FRAME] 140 x 68 at (0,0)
  Rectangle 276 [VECTOR] 127 x 68   fill #d9d9d9        <- placeholder plate
  Skirmish [INSTANCE]    172 x 80 at (-26,-3)
    Rectangle 6 [VECTOR] 172 x 80   fill image:CROP     <- the actual bitmap
```

That bitmap is playlist art (`Skirmish`), which belongs with the playlist data, not in a button
folder. `Rectangle 276`'s `#d9d9d9` is the placeholder — that maps to
`EBRUIColorToken::SurfacePlaceholder`, already in the token enum, and needs no texture either.

---

## Provenance

Extracted with a read-only `use_figma` walk returning name/type/box/fills/strokes/autolayout/text
per node. Nothing in Figma was modified. The PNG route was tried first and **rejected**: MCP
`download_assets` composites the page background (`#F5F5F5`) into the export — verified on
`12:1205`, whose four corners came back fully opaque and whose opaque share was 1.000, despite the
component having no fill and a 2px empty margin. That is the exact defect
`Tools/gen_ui/figma_export.py` was written to avoid.
