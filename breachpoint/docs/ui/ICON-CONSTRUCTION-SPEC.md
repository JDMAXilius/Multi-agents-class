# Icon Construction Spec

**Derived from measured reference. 2026-08-02.**

> **Provenance / legal.** Every measurement below was taken off two 343 Industries /
> Microsoft icon design sheets (`Icon_Design_01.webp`, `Icon_Design_02.webp`, both
> 2500×1406). That artwork is **not ours** and never becomes ours. It lives on the
> Figma page `Refences - Icon Construction`, every frame prefixed `REF /`, and nowhere
> else. **This document is the deliverable** — the rules transfer, the artwork does not.
> See §7 for the hard "do not copy" list.

Method: threshold at luminance > 110 on the max RGB channel, 9-iteration binary
dilation to glue counters and dot-rings into one component, `scipy.ndimage.label`,
then re-tighten each box to the undilated ink. 62 icons recovered — **40 from sheet 1,
22 from sheet 2** — cut to transparent PNGs at native resolution in `icons_cut/`.

---

## 1. Headline finding: the small icon is a REDRAW, not a scale-down

The two sheets are not showing off artwork. They are showing off a **process**: the
same icon appears at a large size and again directly beneath it at half size, and the
small one has been **drawn again from scratch**.

The mechanism is measurable, and it is this:

**Stroke weight is absolute. It does not scale with the icon.**

| Family | Icon size | Modal stroke | Stroke as % of icon |
|---|---|---|---|
| Gear, large | ~36 px | 2.3 px | **6.4 %** |
| Gear, small | ~18 px | 2.8 px | **15.6 %** |
| Weapon, large | ~228 px | 2.3 px | **1.0 %** |
| Weapon, small | ~66 px | 2.7 px | **4.1 %** |

The line stays at 2–3 px everywhere on the sheet regardless of whether the icon is
18 px or 238 px wide. So when you halve an icon, the ink does not halve with it — the
*relative* line weight doubles, adjacent strokes collide, and counters fill in. A
straight scale-down at this stroke policy produces mud. The only way out is to remove
geometry until what remains still has 2 px of clear air between every pair of lines.

That is the whole rule. Everything in §3 is a consequence of it.

## 2. The two-tier (three-tier) size ladder

Measured icon boxes cluster on a clean **1 : 2 : 4** ladder off a ~18.5 px module:

| Tier | Measured box | Ladder | Used for |
|---|---|---|---|
| **Tiny** | 17–20 px | 1× | inline UI glyphs, gear at list density |
| **Small** | 34–42 px | 2× | mode chips, skull badges, gear at card density |
| **Large** | 73–83 px (74×98 for skulls) | 4× | hero / framed mode icons, medals |
| *Wide outlier* | 218–238 × 78–96 | — | weapon line-art, aspect-driven, see §3.5 |

The sheet's own layout grid measures **182.3 px per column** (14 columns detected on a
2500 px sheet), i.e. exactly **10 × the tiny module**. Row pitch confirms the tiers are
laid out deliberately: gear rows sit at 92.4 px pitch (half a grid column), mode icons
at 143.8 px, skulls at 127 px.

## 3. Per-family construction

### 3.1 Mode icons — LARGE (6 icons, sheet 1 row 1)

- Box **73–74 × 74–76 px**. Bounding-box CV **0.6 %** — these are *identical* boxes.
- **Containment: circular frame, always.** Ring stroke **2 px** (one at 3 px), flush to
  the bbox, inner clear span 69 px. Ring/box = 0.027.
- The glyph inside is **solid white fill**, the ring is **outline** → *mixed* family.
- Glyph occupies **0.54–0.84 of the frame diameter** — *not* a fixed inset. Wide glyphs
  (the bullet-belt, 0.84) run nearly edge to edge; compact glyphs (the ammo stack, 0.54)
  are pulled in. **Optical, not geometric.**
- Terminals are flat/chiselled, corners are hard. No rounding anywhere.

### 3.2 Mode icons — SMALL + the frame study (5 icons, sheet 1 row 2)

This row is doing two jobs at once. It is the **same CTF-flag glyph** repeated at the
small tier (36–42 px) inside **five different containers**:

1. **bare circle**, hairline (1 px) — recedes, reads as "not selected"
2. **solid circle**, 3 px ring — the default chip
3. **rounded hex** (soft-vertex), 3 px
4. **notched square** (chamfered corners), 2–3 px
5. **laurel + medal** at 53×51 — the only one that breaks the box, because ceremony

**Rule extracted: the glyph is constant, the container carries the state.** Never
redraw the mark to signal selection/rarity/tier — swap the frame around it.

### 3.3 Equipment / gear — the cleanest two-tier pair (6+6, sheet 1 rows 3–4)

Large **34–40 × 28–39 px** → Small **17–20 × 14–20 px**. Exactly 2:1. Same pitch
(92.4 → 92.5) so the pairs sit in perfect columns. What is dropped, per icon:

| Glyph | Large has | Small drops |
|---|---|---|
| Helmet | brow ridge, visor seam, jaw guard, chin plate, two side vents | all internal panel lines; **visor becomes one solid negative rectangle**; chin plate merges into the shell as a single mass |
| Arm / actuator | articulated upper arm, elbow break, hand, separate spherical joint | elbow and hand articulation; keeps only the bicep curve + the sphere (which gains a cross detail to stay legible) |
| Figure in arc | full limb separation, thin continuous arc | limb gaps thicken and the arms merge into the torso; **arc becomes two broken segments** |
| X-blades | crisp crossed blades, dotted tracking arc, satellite tick | the dot-ring reduces to 3 marks; the blade crossing thickens to hold a 2 px gap |
| Layered plates | 4 discrete stacked plates with visible gaps | collapses to **one silhouetted mass** with a single interior notch |
| Chest module | body + 4 side rails + interior detail | interior gone; becomes **4 flat rails + a 2×2 block** — a diagram, not a drawing |

Fill ratio rises 0.46 → 0.50 across the drop: reduction pushes toward solid.

### 3.4 Skull medals — the polarity flip (4+4, sheet 1 rows 5–6)

**The most aggressive redraw on either sheet, and the most instructive.**

- Large: **74 × 98 px, every one identical to the pixel** (CV 0.0 %). A **solid white
  skull** with the mode glyph *knocked out of the cranium as negative space*.
  Ink 0.62–0.63, 4–8 interior holes (eye sockets, nasal, tooth gaps, glyph counters).
- Small: **34–37 × 33–36 px**. The **skull is deleted entirely.** What survives is the
  inner glyph, redrawn as a **positive 2–3 px outline** sitting in a **broken circular
  ring**. Ink drops to 0.34–0.44; holes drop to 1–2.

So between tiers the family inverts on three axes at once:

| | Large | Small |
|---|---|---|
| Carrier | skull silhouette | broken ring |
| Rendering | solid fill | outline stroke |
| Glyph polarity | **negative** (knocked out) | **positive** (drawn) |

Note what is preserved: the *identity* (which mode) and the *aspect* (round-ish). Note
what is not: literally every pixel. This is a different drawing that means the same thing.

### 3.5 Weapons — outline → silhouette (3+3, sheet 2 rows 1–2)

Large **218–238 × 78–96 px**, pure **outline line-art**, 2–3 px, ink 0.14–0.21, with
**6 / 17 / 32** interior detail shapes respectively (vents, rails, grip texture, sights).

Small **62–69 × 23–28 px** — roughly 1/3.5 linear. They become **solid silhouettes**:
ink jumps to 0.32–0.57 and equals the filled-silhouette area (0.32 / 0.61 / 0.49), i.e.
the outline has been *filled in*. Interior detail count collapses **6→0, 17→2, 32→1**.

**Rule: below ~70 px on the long axis, line-art weapons become filled silhouettes with
at most two negative-space holes** — and those two holes are chosen for recognition
(trigger guard, magazine well), not for accuracy.

### 3.6 Action / boost icons — constant core, variable corona (1+4, sheet 2 rows 3–4)

The large icon (83 × 83, outline, ink 0.27) is the hero. The four beneath it are
**variants, not sizes** — and the measurement proves the discipline:

| Icon | Box | Core arrow bbox | Core area |
|---|---|---|---|
| variant 1 | 71 × 68 | **38 × 38** | 501 px |
| variant 2 | 64 × 66 | **38 × 38** | 477 px |
| variant 3 | 67 × 67 | **38 × 38** | 485 px |
| variant 4 | 69 × 74 | **38 × 38** | 488 px |

The core mark is **identical across all four** (5 % area variance = antialiasing). Only
the surrounding decoration changes — sparkles → moon disc → radial rays → jagged rays.
The outer bbox drifts 64–71 px because the corona is what grows, and it is allowed to.

**Rule: a variant family shares one pixel-identical core; escalation is expressed
entirely in the corona.**

### 3.7 UI glyphs — single tier, no large version (9 + 5)

Sheet 1 row 7: **17–20 × 13–19 px**, pitch 103, ink 0.51, stroke 2–3 px. Lock, gamepad,
swap, crown, mute, loop, focus-reticle, cut, edit. Sheet 2 row 5: **26–32 px**, ink 0.42.

These have **no large counterpart on either sheet** — they are born at their size. They
are also the only **bare** family: no container, ever. Mixed solid/outline chosen per
glyph for legibility, not for family consistency (the lock is solid, the loop is stroke).

### 3.8 Rarity badges — container = meaning (6, sheet 2 row 6)

53–70 px, the **only colour on either sheet**, and the only family where bbox CV is
high (**11.3 %**) because the containers genuinely differ:

- gold spiked ring · violet hex · **bare** (green) · **bare** (cyan) · silver/violet hex
  · gold laurel-circle

Rarity is read off the **frame + hue**, never off the object. The object inside is the
same class of white/tinted 3D-ish render in all six.

## 4. Cross-cutting rules

| Property | Finding |
|---|---|
| **Stroke** | **Uniform, 2–3 px, absolute.** No modulation, no tapering, no weight hierarchy within an icon. The ring and the glyph are the same weight. |
| **Corners** | Hard. Chamfers and notches, never radii. The "rounded hex" frame is the single exception and its vertices are cut, not filleted. |
| **Terminals** | Butt / chiselled. No round caps anywhere on either sheet. |
| **Optical sizing** | **Not a shared bounding box** except where a container defines it. Framed families are pixel-identical (CV 0.6 %) *because the frame is the box*. Bare families vary 5–16 % on both axes while holding constant **pitch** — they are centred on a grid and normalised by **visual weight**, not by box. |
| **Containment** | Mode icons, skull badges, rarity badges → **always contained**. Gear, weapons, UI glyphs → **always bare**. The rule tracks meaning: *state/rank/rarity gets a frame; objects and verbs do not.* |
| **Fill policy** | Large = outline-leaning (ink 0.14–0.30). Small = solid-leaning (ink 0.34–0.57). Reduction always moves toward fill. |

## 5. 343's tiers vs ours

Our existing icon sizes (`COMPONENT-SPECS.md`): **16, 24, 40, 64, 114, 120, 240, 256**.

| 343 measured | 343 ladder | Our nearest | Δ | Verdict |
|---|---|---|---|---|
| 18.5 px (tiny) | 1× | **16** | −14 % | **Direct map.** Our inline icon slot (`Icon 16×16`) is this tier. |
| 37 px (small) | 2× | **40** | +8 % | **Direct map.** Our `Icon Only` button (40×40). |
| 74 px (large) | 4× | **64** | −14 % | **Direct map.** |
| 24 | — | — | — | **Off-ladder.** Sits between our 16 and 40; 343 has no equivalent. Treat 24 as a *rendered* variant of the 16 artwork, not a fourth redraw. |
| 83 px (action hero) | ~4.5× | 64 / 114 | — | Falls between; not a real tier, an aspect artefact. |
| — | — | 114 / 120 | — | Above 343's ceiling. These are **tiles**, not icons — they contain a 64 icon. |
| — | — | 240 / 256 | — | Splash / render sizes. Outside icon construction. |

**Our 16 / 40 / 64 is a valid three-tier ladder and lines up with 343's 1:2:4 within
14 %.** The endpoints are exact: 64 : 16 = **4 : 1**, identical to 343's 74 : 18.5.

The one divergence worth naming: our mid tier (40) sits **high**. The geometric mean of
16 and 64 is 32, so 40 is 25 % over the true midpoint. Practically this means **the
40 px redraw should be derived from the 64 artwork, not from the 16** — it is closer to
large than to small.

And: **our 2 px border standard already matches 343's measured stroke exactly.**
`COMPONENT-SPECS.md` records 2 px on item tiles and emphasis (177 uses); 343's modal
stroke across all 62 icons is 2–3 px. Nothing to change.

## 6. Rules that transfer — instructions for drawing OUR icons

Written as directives. These are the deliverable.

1. **Draw three icons, not one.** Every icon in the system is authored three times: at
   **64**, at **40**, at **16**. Never export one and scale it.
2. **Hold the stroke at 2 px at every tier.** Do not scale the line with the box. This
   is the constraint that forces the redraw, and it is already our house border weight.
3. **Enforce 2 px of clear air between any two strokes.** This is the acceptance test
   for a redraw. If two lines are closer than 2 px at the target size, delete one of
   them — do not thin them.
4. **Reduce toward solid.** As size drops, outlines become fills. Ink coverage should
   climb from roughly 0.20 at 64 to roughly 0.45 at 16.
5. **At 16, allow at most two interior negative shapes.** Choose them for recognition,
   not for accuracy. 343 goes 32 interior details → 1.
6. **Invert polarity when it buys legibility.** A knocked-out mark at 64 may become a
   drawn mark at 16, and the carrier shape may be discarded entirely. Preserve the
   *meaning* and the *aspect ratio*; nothing else is sacred.
7. **State lives in the container, never in the glyph.** To show selected / ranked /
   rare, swap the frame (bare circle → solid ring → chamfered square → laurel). The
   mark inside stays byte-identical.
8. **Variants share a pixel-identical core.** Escalation is drawn in the corona around
   the core, and the corona is allowed to grow the outer box.
9. **Frame contained families to a shared bounding box; centre bare families on a
   shared pitch.** Do not force bare icons into a common box — normalise them by visual
   area instead. Target ≤ 6 % variance in √area, and accept up to 16 % on either
   individual dimension.
10. **Contain state; leave objects bare.** Mode / rank / rarity get a frame. Weapons,
    equipment and UI verbs do not.
11. **Hard corners, butt terminals, uniform weight.** Chamfer and notch; never fillet.
    No tapering, no thick-thin modulation, no internal weight hierarchy.
12. **Optical inset, not geometric.** Inside a circular frame the glyph may occupy
    anywhere from 0.54 to 0.84 of the diameter. Set it by eye against its neighbours in
    the row; do not apply a fixed padding token.
13. **Sit the tiny tier on the layout module.** 343's grid column is exactly 10× the
    tiny icon. Our 16 px tier should relate to the 1280×720 grid the same way.

## 7. What must NOT be copied

Non-negotiable. These are 343 Industries / Microsoft assets.

- **The skull mark.** In any form — solid, outline, badged, with or without the cranial
  knock-out. It is a Halo identity mark. We take the *polarity-flip technique*; we do
  not take the skull.
- **Any weapon silhouette.** The energy sword, the plasma/needle rifle, the assault
  rifle. Recognisable weapon silhouettes are protected trade dress. We take the
  *outline→silhouette reduction rule*; we draw our own weapons.
- **The mode glyphs.** CTF flag, oddball, the crossed-blade mark, the radial burst.
  These are Halo playlist identities.
- **The Spartan helmet and the armour/equipment glyphs.** Mjolnir silhouettes are
  among the most protected marks in the franchise.
- **The rarity badge frames** and their colour coding. Gold-spiked ring, the specific
  hex, the specific hue-to-rarity mapping.
- **Any brand mark** from the accompanying sheets (`Brands_AllUp`, `InWorld_Branding`,
  `VISR+Frame`) — UNSC, Banished, or otherwise.
- **The laurel/medal container.** Common enough to be generic in the abstract, but this
  specific execution is theirs.

What we take is §6, and §6 only: a stroke policy, a size ladder, a reduction procedure,
a containment convention, and an optical-normalisation target. Those are craft rules.
They are not anyone's artwork.

---

*Measurements: `slice.py` + `icons.json`. 62 icons, 40 from sheet 1, 22 from sheet 2.
Figma reference page: `Refences - Icon Construction`.*
