# Drop Down · Button Border · Slider Row Wide — measured breakdown

The three smaller sets on page `Buttons & Rows` (`6:25`). Read via the Figma Plugin API, 4 Aug 2026.
None of the three has a UE class yet.

---

## 1. Drop Down (`12:1327`) — 2 variants

The **popup list item**, not the row that opens it. `Menu Row / Type=Drop Down` is the closed
control; this is what appears in the list beneath it.

```
State=Default   COMPONENT 107 x 27   autolayout HORIZONTAL, gap 10, padding 5/10/5/10
  Text and Icon FRAME 87 x 17        autolayout HORIZONTAL, gap 10
    BUTTON      TEXT  61 x 17  fill #000000  Rajdhani Demi 14px
    Unchecked   INSTANCE 16 x 16  -> Rectangle 132, 16x16, stroke #000000 1px

State=Hover     COMPONENT 105 x 30   fill #000000@0.8
  Text and Icon FRAME 87 x 17 at (8,7)
    BUTTON      TEXT  fill #ffffff
    Unchecked   INSTANCE 16x16, stroke #ffffff 1px
  Border        VECTOR 108 x 33 at (-1,-1)  stroke #000000@0.8 1px
```

**It is drawn dark-on-light, the inverse of every other component here.** Default has black text
and a black checkbox stroke on no fill — meaning it expects a *light* surface behind it, while the
hover state supplies its own `#000000@0.8` plate and flips to white. Either this component assumes a
white popup panel that is not in this set, or the two states are inconsistent. **Flagged, not
resolved** — it needs a founder read before anyone builds it.

Second oddity: the shell **grows** on hover, 107x27 → 105x30. Every other component in the file
keeps its box and moves the inner frame. Treat as suspect.

---

## 2. Button Border (`12:1337`) — 6 variants

100 x 100, three axes: `State` (Default/Hover) × `w/ Fade` (False/True) × `Sticker`
(Default/Bonus). **This is a frame, not a button** — there is no text body and no fill. It is the
corner-bracket chrome that wraps a tile.

```
Left Line       VECTOR  68 long  stroke #ffffff@0.4  weight 2
Right Line      VECTOR  68 long  stroke #ffffff@0.4  weight 2
Top Line        VECTOR  98 x 10  stroke #ffffff      weight 2
Bottom Line     VECTOR  98 x 11  stroke #ffffff      weight 2
Left Line Full  VECTOR  12 long  stroke #ffffff      weight 2
Right Line Full VECTOR  12 long  stroke #ffffff      weight 2
Free Sticker    FRAME  100 x 12 at (0,-12)   HIDDEN on Sticker=Default
  Tab           VECTOR  42 x 12  fill #ffffff
  FREE          TEXT    28 x 15  fill #000000  Rajdhani Bold 12px
```

**State drives stroke weight, 2 → 4**, and the hover variant nudges the geometry out by 2px
(`Left Line Full` moves to y=-2, lines lengthen 12 → 86). So hover is "the bracket thickens and
opens", not a colour change.

**Sticker=Bonus** swaps three strokes (`Top Line`, `Left Line Full`, `Right Line Full`) to `#2ec3e5`
and shows a wider tab: `Tab` 54 x 12 filled `#2ec3e5`, text `BONUS`. `Sticker=Default` carries a
`FREE` tab of 42 x 12 that is **hidden** — so the FREE state exists in the file but is not wired to
a variant axis value. Someone has to decide whether `Sticker` should be a three-value axis
(None/Free/Bonus).

**`w/ Fade=True`** replaces the flat 0.4 side strokes with **gradient strokes** — `Left Line` fades
`#ffffff:0 → #ffffff:1`, and on the hover+fade variant the two full lines carry asymmetric stops
(`0 → 0.85` left, `0.16 → 1` right). Note the fade variants **drop the bottom line entirely**.

Gradient *strokes* are the one thing here UMG cannot do natively on a line — `UBRHairlineBorder`
draws solid edges. A faded bracket needs either a material or a small texture.

---

## 3. Slider Row Wide (`12:1388`) — 2 variants

160 x 26. **This one does not belong to the same design system as the rest of the page** and that is
the most important thing to record about it.

```
Active=True   COMPONENT 160 x 26  fill #34729b
  BUTTON      TEXT 120 x 14 at (12,5)  fill #ffffff  Rajdhani SemiBold 14px
  Settings    INSTANCE 24 x 24  HIDDEN
  4 x corner dots  RECT 1 x 1  fill #ffffff   at the four corners
  Line 70     LINE 160 long at y=1  stroke #ffffff@0.2  1px
  New         VECTOR 10 x 10  fill #ffd436  HIDDEN

Active=False  COMPONENT 160 x 26  fill #000000@0.2
  BUTTON      TEXT  fill #3f97ce
  corner dots only at the TOP two, @0.2
```

**The palette is wrong for this file.** `#34729b`, `#3f97ce` and `#ffd436` appear nowhere in the
VISR token set (`BRComponentTokens.h` / `BRUITokens.h`) and are not the near-monochrome + one-accent
treatment `ui-presentation` §4 requires of the front end. They read as a different tool's chrome —
most likely the Forge editor (there is a `Forge Editor` page at `14:970`). **Do not build this as a
front-end component without a ruling.** It may belong to the Forge lane, or it may be stale.

The 1 x 1 corner dots are a nice detail and are trivially drawn by UMG; they are not textures.

---

## Texture verdict across all three sets

| Item | Verdict |
|---|---|
| Drop Down — everything | None. Text, a 1px rect, a plate. |
| Button Border — solid brackets, stickers, tab | None. Lines and text; `UBRHairlineBorder` already draws edge sets. |
| Button Border — **gradient strokes** (`w/ Fade=True`) | **Material or texture.** A stroke with a gradient along its length is not something a Slate line brush does. |
| Slider Row Wide — everything | None (and see the palette warning above). |
