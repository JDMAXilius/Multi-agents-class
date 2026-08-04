# Buttons — Figma breakdown for widget rebuild

Measured breakdown of every button component on the Figma page `Buttons & Rows` (`6:25`), file
`yznvnVdOFDADaugZSeomfP`. Read via the Figma Plugin API (`use_figma`) on 4 Aug 2026. **50 component
variants across 5 sets, all captured.**

| File | Set | Variants | UE class |
|---|---|---|---|
| `01-HighlightButton.md` | Highlight Button (`12:1194`) | 13 | `UBRHighlightButton` |
| `02-MenuRow.md` | Menu Row (`12:724`) | 27 | `UBRMenuRow` |
| `03-DropDown-ButtonBorder-SliderRowWide.md` | Drop Down (`12:1327`) | 2 | — none |
| `03-…` | Button Border (`12:1337`) | 6 | — none |
| `03-…` | Slider Row Wide (`12:1388`) | 2 | — none |

---

## Why this folder holds documents and not PNGs

**The PNG route was tried and rejected on evidence.** MCP `download_assets` composites the Figma
page background into every export. Verified on `12:1205` (`Highlight Button / Idle / Main`): the
component has **no fill** and a 2px empty margin at every corner, yet the returned PNG came back
with all four corners fully opaque and an opaque share of **1.000** — a solid `#F5F5F5` plate. That
is exactly the defect `Tools/gen_ui/figma_export.py` documents in its own docstring ("41 files with
baked-in backgrounds that looked perfect in every viewer and were wrong only in engine"), and
`Tools/gen_ui/check_transparency.py` exists to catch it. The probe was deleted rather than landed.

**More importantly, flat PNGs are the wrong artifact for the stated goal.** These are to be rebuilt
as widget components. A baked bitmap of a button cannot be re-tinted from the palette, cannot change
state, and cannot resize — `ui-presentation` §8 puts it plainly: **export nothing UMG can draw.**
What actually crosses the Figma→UE boundary is layout, geometry, spacing, hierarchy, state
treatment and colour token names. That is what these documents contain, at the fidelity needed to
rebuild each variant without opening Figma.

**Working route if raw art is ever genuinely needed:** `Tools/gen_ui/figma_export.py`, which uses the
Figma REST `/v1/images` endpoint (renders in isolation, no compositing) and validates every output
through `preflight_textures.py` before anything lands. It needs `FIGMA_TOKEN=figd_...` in
`Tools/env.local`, which is **not currently set** on this machine.

---

## What actually needs a texture

Out of 50 variants, **three things**, and everything else is drawable by UMG:

| # | Item | Where | Why |
|---|---|---|---|
| 1 | Diagonal hatch | Menu Row `Drop Down (Active)`, `Dig Down` | 69 `LINE` nodes at 0.5 weight, sheared, under a gradient mask. Rebuild as a **tiling hatch texture** or a material — not 69 widgets. |
| 2 | Gradient strokes | Button Border `w/ Fade=True` | A stroke whose colour fades along its own length. Slate line brushes are solid; needs a material or a strip texture. |
| 3 | Type-accent gradient plate | Highlight Button, all `Idle` states | Linear `#000000@0.3 → accent`, stacked between `#000000@0.8` and `#000000@0.2`. A `UImage` with a gradient brush is enough — **not** a per-type PNG. |

Emblem and glyph art seen inside variants (`Slayer` ring, `Shield`/`Sword`/`Skull`, `Revert` icon,
`Credit Icon`) is **not button art**. It belongs to the icon pipeline — Figma pages `Art / UI Glyphs`
(`80:2`) and `Art / Insignia` (`48:2`), handled by `figma_export.py` into `Content/UI/Icons`.

---

## Findings against shipped code — read before building

1. **`UBRHighlightButton`'s hover model is wrong.** The header says hover inverts the plate to solid
   white (`SurfaceInverted`). Measured: hover fill is the **type's own accent** — `#2ec3e5` Main,
   `#ff5c00` Event, `#ffc11c` Premium. Only `Type=Boring` inverts to white. The label going black is
   correct throughout. See `01-HighlightButton.md`.

2. **`UBRMenuRow`'s model is correct** — it really does invert to `#ffffff`. Two components, two
   rules; do not unify them.

3. **The ACCENT GAP in `BRHighlightButton.h` is now closable.** Every type has a distinct idle
   gradient; the stop-1 colour is the differentiator. One new token is owed: `AccentPremiumTint`
   `#ffed4f` (the idle-gradient partner to `AccentPremium` `#ffc11c`).

4. **Menu Row `Type=Slider` uses a different type ramp** — Rajdhani **Demi 14px**, not SemiBold
   16px, with the percentage in Roboto Condensed Medium Italic 14px. Either deliberate or drift;
   unresolved.

5. **Radio is a square.** `Vector` is a `RECT`, not an ellipse — consistent with the flat/sharp
   system, but it will surprise anyone building from the name alone.

6. **`Slider Row Wide` uses a palette outside the token system** (`#34729b`, `#3f97ce`, `#ffd436`).
   Probably Forge-editor chrome, not front end. Needs a ruling before it is built.

7. **`Drop Down` (the popup item) is drawn dark-on-light**, inverse of everything else, and grows
   107x27 → 105x30 on hover. Both are suspect.

None of these were fixed here. `ui-presentation` §8's one-way rule: Figma is right about appearance,
so the C++ is what moves — but that is a code change with its own review.

---

## Provenance

Read-only. Nothing in the Figma file was modified. Extraction walked each variant returning
name/type/box/fills/strokes/autolayout/text per node; the 69-node hatch was collapsed on the second
pass so it could not exhaust the tool's 20 kB response cap.
