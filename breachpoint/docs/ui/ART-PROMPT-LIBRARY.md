# BREACHPOINT — Art Prompt Library

**Status:** v1, 2 Aug 2026. Companion to `REFERENCE-EXTRACTION.md` (§6 icon families, §7b art
list), `COMPONENT-SPECS.md` (§1 language, §7 effects, §8 palette) and `SCREEN-BUILD-SPEC.md`
(§6 art-replacement list and taxonomies).

**This file is the generation contract.** Every prompt below is written to be pasted verbatim
into the image tool. Nothing here is a mood board; every clause is load-bearing.

---

## 0. The law, before anything else

The reference material for the *geometry* of this UI is Halo Infinite. **No generated pixel may
resemble Halo intellectual property.** The taxonomy transfers — a 16-step rank ladder, a slot
system, a medal set, a mode-icon family. The artwork does not.

**GLOBAL NEGATIVE BLOCK — inlined verbatim in every prompt in this file:**

```
Do NOT depict or reference: Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC, ODST,
Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world motif,
343 Industries, Cortana, Warthog vehicles, or the rounded-dome full-face visor helmet
silhouette. No olive-green power armour. No alien or bioluminescent glowing purple hardware.
No superhero armour, no glossy chrome muscle-suit, no cape, no crest, no wings.
```

Three additional rules that keep output out of the danger zone:

1. **Never say "sci-fi soldier", "space marine", "super soldier" or "power armour."** Say
   *near-future military hardware*, *issued kit*, *field-worn equipment*.
2. **No rounded dome silhouettes.** Breachpoint helmets are faceted, flat-crowned, with a
   rectangular or trapezoidal visor aperture and a visible chin-strap/mandible cage.
3. **No ring/torus/arc as a hero shape.** Rings appear only as thin reticle hairlines, never as
   the primary silhouette of an insignia, medal or emblem.

If a prompt could plausibly return Halo-looking output, it is rewritten before it is run.

---

## 1. The visual language every prompt must sit inside

From `COMPONENT-SPECS.md` §1: **flat panels, sharp corners, hairline white rules that are partial
rather than closed boxes, near-monochrome. Nothing glows, nothing rounds, nothing gradients
except item rarity.** Colour is accent only — 729 of the strokes in the measured library are 1px
`#ffffff`.

### Palette tokens (pass these to `recraft_v4_1` in the `colors` array)

| Token | Hex | Role |
|---|---|---|
| White | `#FFFFFF` | Chrome, all icon linework |
| Black | `#000000` | Grounds (used at 20/40/50/60/80% alpha in engine) |
| Electric Blue | `#2EC3E5` | Primary highlight · the in-game VISR channel |
| Premium Yellow | `#FFC11C` | Premium / battle pass |
| FOMO Orange | `#FF5C00` | Limited time / event |
| Wrong Red | `#FF4B4B` | Warning / destructive |
| Rare Blue | `#6295DA` | Rarity — rare |
| Epic Purple | `#AB55FF` | Rarity — epic |
| Legendary Gold | `#E8BA3D` | Rarity — legendary |
| Placeholder Grey | `#D9D9D9` | `surface/placeholder` |

`colors` accepts **max 10 hex values**. Every array below is at or under that.

### The three-tier fidelity rule (343's rule, inherited — `REFERENCE-EXTRACTION.md` §2, §6)

**Fidelity scales with nearness to gameplay.** This is not a suggestion; it is the reason the
families below get different prompts.

| Tier | Families | Treatment the prompt must demand |
|---|---|---|
| **T1 — FLATTEST** (front-end / menu chrome) | F slot icons, H manufacturer marks, J currency, C gametype icons, B grades & banners | Pure white monochrome. Uniform stroke weight. No fill gradients, no bevel, no shadow, no highlight, no perspective. Reads at 16px. |
| **T2 — MID** (in-game HUD) | C large mode icons, I difficulty icons | White plus the cyan `#2EC3E5` VISR channel, two channels only. Solid fills permitted alongside strokes. Still absolutely flat — no bevel, no shadow. |
| **T3 — MOST SKEUOMORPHIC** (achievement art) | **D medals**, **B rank insignia**, E emblems (partly) | Bevelled struck-metal treatment. Chamfered edges, a three-value metal ramp, visible facets, a raised rim. Front-on orthographic, but shaded as a physical struck token. |

T3 art additionally receives the **Medal 3D** effect token in UMG on top of the generated art
(`COMPONENT-SPECS.md` §7): `INNER_SHADOW #ffffff@0.8 (0,1) r0.5` · `INNER_SHADOW #000000@0.5
(0,-1) r0.5` · `DROP_SHADOW #000000@0.25 (0,2) r4` · `DROP_SHADOW #000000@0.5 (0,1) r1`. So the
generated art carries the *facets*; the engine carries the *lighting*. Do not double them —
prompts for T3 ask for **facet geometry and a flat metal ramp, not a rendered specular
highlight.**

---

## 2. Model selection, and the standing arguments

| Model | Cost | Use for | Never use for |
|---|---|---|---|
| `soul_location` | **0.12** | Environments, backgrounds, scene plates. Photographic, 16:9, depth. | Anything with a hard silhouette that needs alpha. It cannot produce clean vector edges. |
| `recraft_v4_1` `model_type: "vector"` | **2.5** | Icons, insignia, emblems, medals, marks, the wordmark. Returns **SVG** — resolution-independent, recolourable by fill swap, no tracing step. | Material chips and 3/4 product renders. Vector flattens the material read. |
| `recraft_v4_1` `model_type: "utility"` | **2.5** | Flat product-style raster renders: item tiles, finish chips, pattern swatches. | Icons. It will add soft shading the T1 tier forbids. |

**The vector model returning SVG is the single most important economic fact in this document.**
It removes the tracing step, makes palette recolouring free, and makes one sheet reusable at
every size (76px rank insignia and its 152px @2x are the same file). It is why 15 of the 27
generations in this library are `vector`.

**`background_color` is always `#000000`** for icon families. Three reasons: it matches the
app's grounds so a mis-key is invisible, the SVG background is a single deletable rect, and a
raster fallback keys cleanly against white linework. Never `#FFFFFF` — white-on-white is a
guaranteed reroll.

---

## 3. Sheet strategy — the whole cost model

At 2.5 credits per vector generation, **one icon per generation is malpractice.** Almost every
family below is a single generation producing a grid, split in post with a two-line ImageMagick
call. The economics: 16 rank insignia individually = 40 credits; as one 4×4 sheet = 2.5.

### The sheet rules, applied to every grid prompt below

1. **Ask for the grid explicitly and numerically.** "A 4×4 grid of sixteen distinct icons,
   evenly spaced, each centred in its own cell, uniform margins, identical scale across all
   cells." Vagueness here produces overlapping art and a wasted 2.5.
2. **Never ask for text labels in the cells.** Generative text is unreliable and letterforms
   bleed into the icon crop. Instead the prompt **enumerates the cells in row-major order** —
   "row 1 left to right: …" — and the split script names the crops from the same ordered list.
   Reading order is the label.
3. **Demand no cell borders, no gridlines, no frames.** A drawn grid line has to be painted out
   of sixteen crops.
4. **Demand identical scale and identical stroke weight across all cells.** Without it, cell 3
   arrives at twice the optical weight of cell 11 and the family reads as sixteen unrelated
   icons.
5. **Square sheet for square grids.** Request 1:1 for a 4×4 or 3×3; 2:1 for a 4×2 strip; 4:1
   for a single row.
6. **Generate far above target.** Raster sheets at 2048×2048 give a 4×4 cell of 512px for a
   100px target — 5× headroom for the downsample. Vector sheets are resolution-free.

### Families that genuinely need individual generations

| Family | Why |
|---|---|
| **A — Scene plates** | Each plate is a different *place*. There is no grid that produces eight coherent environments, and at 0.12 credits batching saves nothing. Individual is both better and cheaper-in-effect. |
| **L — Wordmark** | This is the game's identity mark. It is the one asset where iteration cost is justified: one exploration sheet of six lockups, then one dedicated refinement of the chosen direction at full canvas. A wordmark cropped out of a 4×4 cell will not survive the 304×118 placement. |

Everything else — B, C, D, E, F, G, H, I, J, K — ships as sheets.

---

# FAMILY A — Scene plates

| | |
|---|---|
| **Assets** | 8 primary + 4 alternates = **12** |
| **Size** | 1280×720 authored, ×1.5 → **1920×1080** shipped (`REFERENCE-EXTRACTION.md` §3) |
| **Aspect** | **16:9** |
| **Model** | `soul_location` @ **0.12** — environments only, and this is the only environment family |
| **Sheet strategy** | **None. One generation per plate.** See §3. |
| **Tier** | N/A (photographic plate, sits *behind* all UI, receives `LAYER_BLUR`) |

These are the `Start Menu Background` replacement from `SCREEN-BUILD-SPEC.md` §6 — the single
most-repeated Halo-owned asset in the source, appearing on every screen.

**Critical composition constraint, from `SCREEN-BUILD-SPEC.md` §1:** the left column runs
`x=69→620` and the right band `x>=650` is reserved for the 3D character subject. Every plate
must therefore be **compositionally empty in the left third and the bottom 50px**, with its
visual interest in the upper-right quadrant and mid-depth. State this in the prompt or the plate
fights the menu.

### A1 — `FE_Background` — main menu, staging deck

```
A wide cinematic interior of a near-future military staging deck at pre-dawn, photographed on a
35mm lens at f/4, camera at chest height, one-point perspective receding to the upper right.
Weathered functional hardware: bare structural steel ribs, riveted deck plating with worn traffic
paint, cable looms zip-tied along the wall, rolling equipment carts, stencilled numerals on
bulkheads, a roller shutter half open at the far end letting in a cold blue rectangle of dawn.
Practical lighting only — caged work lamps overhead in warm sodium, one cold cyan strip light
along the right wall. Heavy atmospheric haze, volumetric light shafts, fine airborne dust.
Desaturated near-monochrome grade, cool shadows, a single warm accent. Deep depth of field with
the far end softening into haze. No people, no vehicles, no text, no signage lettering.
Composition: the left third of the frame is deliberately empty dark deck plating with no detail;
all visual interest sits in the upper right quadrant; the bottom 60 pixels are flat unbroken
dark floor. Grounded, industrial, lived-in, understated.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, Warthog vehicles, or a rounded-dome full-face visor helmet.
No alien architecture, no glowing purple technology, no holograms floating in mid-air, no
readable text or logos, no lens flares, no orange-and-teal blockbuster grade, no people.
```

### A2 — `MM_Root` — matchmaking, operations room

```
Interior of a near-future military operations room, wide 16:9, 28mm lens at f/2.8, camera
slightly above eye level looking down across a long low planning table. Weathered hardware:
scuffed matte-grey equipment racks, ribbed cable trunking, a wall of dark unlit monitor panels,
a paper map curling at the corner under a metal weight, folding chairs. Lighting is a single
overhead cold fluorescent panel plus a faint cyan underglow from the table edge; everything else
falls to black. Low-key exposure, heavy shadow, near-monochrome desaturated grade with one cyan
accent. Light haze. Shallow-to-medium depth of field, foreground table edge softly out of focus.
Composition: left third empty and dark, interest in the upper right, bottom 60 pixels flat dark.
No people, no readable text, no on-screen graphics.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, or Warthog vehicles. No alien technology, no floating holograms,
no glowing blue wireframe globes, no readable text, no people, no lens flare.
```

### A3 — Operator / customization backdrop, armoury bay

```
Interior of a compact near-future armoury bay, 16:9, 40mm lens at f/2.0, camera at chest height,
symmetrical frontal composition centred on an empty circular steel inspection pad set into a
grated floor. Behind it: a wall of open equipment lockers with hanging straps and empty hooks,
a workbench with hand tools laid out, a parts bin, a coil of hose. Everything weathered — chipped
paint, oil stains, scuffed metal. Lighting is a hard overhead key directly above the pad throwing
a tight pool of light, cold cyan rim strips down both side walls, everything beyond falling to
near black. Strong chiaroscuro, low-key, near-monochrome desaturated grade. Light haze catching
the overhead key. Medium depth of field; the back wall is softly out of focus. The centre of the
frame is intentionally empty — a character will be composited standing on the pad. No people,
no mannequins, no armour on display, no readable text.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a halo ring motif, 343 Industries, Cortana,
or Warthog vehicles. No power armour on a stand, no glowing alien tech, no floating holographic
interface, no readable text, no people.
```

### A4 — `FE_Splash` — boot splash, breach corridor

```
A tight near-future service corridor viewed head on, 16:9, 24mm lens at f/2.8, camera at chest
height, dead-centre one-point perspective. The corridor is dark, ribbed with structural frames,
cable runs along the ceiling, condensation on the walls. At the far end a heavy blast door stands
partly open, a hard blade of cold cyan-white light cutting through the gap and down the corridor
floor. Everything else is silhouette. Extremely low-key, high contrast, near-black with one
light source. Heavy volumetric haze, visible light shafts, fine dust in the beam. Deep depth of
field. Desaturated monochrome grade with a single cold accent. Grounded, industrial, tense,
minimal. No people, no readable text, no signage lettering.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a halo ring motif, 343 Industries, Cortana,
or Warthog vehicles. No alien geometry, no glowing purple, no floating holograms, no readable
text, no people, no anamorphic lens flare.
```

### A5 — `FE_Loading` — gantry at dawn

```
Exterior of a near-future airfield service gantry at first light, 16:9, 50mm lens at f/5.6,
camera low near ground level looking up along the gantry structure. Weathered steel lattice,
peeling safety paint, hazard stripes worn thin, a run of conduit, a fuel line coiled on a drum,
wet tarmac reflecting the sky. Overcast flat dawn light, cold and even, no direct sun; one
sodium work lamp still burning on the gantry. Very low contrast, muted desaturated palette,
almost monochrome grey-blue. Light ground mist. Deep depth of field. Composition is deliberately
quiet and uncluttered with large areas of flat empty sky — this plate sits under a full-width
loading bar and must not compete. No people, no aircraft, no readable text.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, a halo ring motif, 343 Industries, Cortana, or Warthog
vehicles. No spacecraft, no alien structures, no glowing technology, no readable text, no people.
```

### A6 — Store — quartermaster's depot

```
Interior of a near-future quartermaster's supply depot, 16:9, 35mm lens at f/2.8, camera at
chest height, perspective receding to the upper right down an aisle of shelving. Weathered olive
and grey shipping crates stencilled with worn geometric marks, banding straps, a pallet jack, a
clipboard hanging on a nail, shrink-wrapped stacks. Lighting: warm overhead work lamps down the
aisle falling off fast, one cold cyan strip on the left wall. Warm-cool mixed lighting, moderate
contrast, desaturated grade with a warm amber accent to read as "goods". Light haze. Medium
depth of field, far end of the aisle soft. Composition: left third empty dark floor, interest
upper right, bottom 60 pixels flat. No people, no readable text or legible stencilled words.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, a halo ring motif, 343 Industries, Cortana, or Warthog
vehicles. No alien cargo, no glowing crates, no floating holograms, no readable text or brand
logos, no people.
```

### A7 — Post-match — debrief room

```
Interior of a bare near-future debrief room, 16:9, 35mm lens at f/2.0, camera at seated eye
height looking across an empty dark table toward a blank wall. Minimal, austere: scuffed painted
concrete wall, a run of trunking at waist height, one folding chair pushed back, a mug left on
the table, a wall-mounted first aid box. Lighting is a single hard overhead panel above the table
creating a bright pool and long shadows, everything at the edges falling to black. Low-key,
high contrast, cold near-monochrome grade, no warm accent. Still air, minimal haze. Shallow depth
of field with the back wall soft. Extremely quiet composition, large empty areas — this plate
sits under a dense results table. No people, no readable text.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, a halo ring motif, 343 Industries, Cortana, or Warthog
vehicles. No alien technology, no holograms, no glowing screens, no readable text, no people.
```

### A8 — Vista — arena exterior key art plate

```
A wide establishing exterior of a near-future industrial relay station on a high desert plateau,
16:9, 24mm lens at f/8, camera at ground level on a slight rise looking across the site at golden
hour. Weathered functional structures: a low concrete blockhouse, a lattice mast, satellite
dishes at varied angles, chain-link fencing, a graded dirt road, sparse scrub. Warm low
side-lighting from camera left casting long hard shadows across the ground plane, cool blue
shadow fill, a deep gradient sky. Fine airborne dust catching the low sun. Deep depth of field
front to back. Warm-to-cool split grade, restrained saturation, filmic contrast, subtle grain.
Composition: horizon on the lower third, the mast at the right third line, the left third quiet
open ground. No people, no vehicles, no readable text.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif or any ring on the horizon, 343 Industries, Cortana, or Warthog vehicles. No alien
megastructures, no floating islands, no ringworld arc in the sky, no spacecraft, no glowing
technology, no readable text, no people.
```

### A9–A12 — Alternates

Re-run A1 with `at night, all work lamps off, only the cyan strip and moonlight` (seasonal
variant), A3 with `warm amber key instead of cold cyan` (premium/store variant), A8 at
`blue hour, overcast, no sun` (playlist card variant), and A5 as `an interior gantry catwalk
above the deck` (secondary loading variant). Same AVOID block each time.

### Post-processing — Family A

1. Upscale each plate to **1920×1080** (soul_location output is typically below this).
2. **No alpha, no cutout.** These are opaque plates.
3. Apply a **−15 saturation, −10 contrast** grade pass in engine or in the source PNG so the
   plate never competes with the UI. The reference plate is always visually quieter than it
   looks in isolation.
4. Verify the left-third and bottom-60px emptiness by overlaying `Grid - 3 Collumn`
   (1143×570 at 69,38) before accepting the plate.
5. Import as `T_Plate_<Name>`, TextureGroup **UI**, sRGB on, mip maps **on** (these do get
   scaled), compression **BC7**.
6. The `LAYER_BLUR` from `COMPONENT-SPECS.md` §7 is applied **in engine**, not baked. Ship the
   plate sharp.

---

# FAMILY B — Rank insignia ladder

| | |
|---|---|
| **Assets** | **16 ranks** + **3 grades** + **2 banners** = 21 |
| **Size** | Ranks **76×76** · Grades **42×32** · Banners **46×176** |
| **Aspect** | Rank sheet **1:1** (4×4) · Grades+banners sheet **1:1** (see layout) |
| **Model** | `recraft_v4_1`, `model_type: "vector"` — insignia, and SVG makes the 76/152/228 sizes one file |
| **Sheet strategy** | **2 generations.** Sheet B-1 = 4×4 grid, 16 ranks. Sheet B-2 = grades + banners together. |
| **Tier** | Ranks = **T3 skeuomorphic** (they carry Medal 3D per `COMPONENT-SPECS.md` §4/§7). Grades and banners = **T1 flat** (they are chrome around the insignia). |

### The ladder, designed to escalate legibly at 26×26 in a roster row

`COMPONENT-SPECS.md` §4 places the insignia at **26×26 inside a 30×26 rank frame**. Silhouette
must survive that. The escalation is therefore **structural, not detail-based** — four groups of
four, each group a new base shape:

| # | Name | Silhouette |
|---|---|---|
| 1 | RECRUIT | one horizontal bar |
| 2 | OPERATIVE | two stacked bars |
| 3 | SPECIALIST | three stacked bars |
| 4 | LEAD SPECIALIST | three stacked bars + a short centre tick above |
| 5 | SERGEANT | one upward chevron over a flat rocker bar |
| 6 | STAFF SERGEANT | two upward chevrons over a rocker bar |
| 7 | MASTER SERGEANT | three upward chevrons over a rocker bar |
| 8 | SERGEANT MAJOR | three chevrons over a rocker bar + a centre lozenge |
| 9 | WARRANT | a six-sided plate containing one vertical spearhead wedge |
| 10 | LIEUTENANT | the plate + wedge + one pip each side |
| 11 | CAPTAIN | the plate + wedge + two pips each side |
| 12 | COMMANDER | the plate + wedge + three pips each side |
| 13 | BRIGADIER | a hexagonal shield with two bracket wings, one ray above |
| 14 | VANGUARD | hexagonal shield, two bracket wings, two rays |
| 15 | SPEARHEAD | hexagonal shield, two bracket wings, three rays |
| 16 | BREACHPOINT | hexagonal shield, two bracket wings, three rays, and a single hard horizontal breach-line cutting the shield in half |

### Prompt B-1 — the sixteen ranks (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#D9D9D9","#8A8A8A","#3A3A3A","#E8BA3D","#C9982B","#6295DA","#2EC3E5"]`
**Aspect:** 1:1

```
A 4x4 grid of sixteen distinct military rank insignia badges, flat orthographic front-on view,
each badge centred in its own cell, evenly spaced with uniform margins, identical scale and
identical stroke weight across every cell, on a plain solid black background. No grid lines, no
cell borders, no frames, no text, no letters, no numerals anywhere in the image.

The badges are struck metal tokens rendered as clean hard-edged vector shapes with chamfered
bevelled edges and a raised outer rim. Each badge uses a three-value metal ramp — a bright top
edge, a mid-tone face, a dark lower edge — describing the facets of a physical stamped token.
Flat facet shading only, no photographic specular highlight, no glow, no drop shadow, no
gradient blur. Silhouettes are angular and geometric: straight cuts, hard corners, sharp points.

Row 1, left to right: one horizontal bar; two stacked horizontal bars; three stacked horizontal
bars; three stacked bars with a short vertical tick centred above them.
Row 2, left to right: a single upward-pointing chevron above a flat horizontal rocker bar; two
stacked upward chevrons above a rocker bar; three stacked upward chevrons above a rocker bar;
three chevrons above a rocker bar with a small diamond lozenge centred between them.
Row 3, left to right: a six-sided angular plate containing a single vertical spearhead wedge
pointing up; the same plate and wedge with one small square pip on each side; the same with two
pips on each side; the same with three pips on each side.
Row 4, left to right: a hexagonal shield flanked by two angular bracket wings with one straight
ray rising above the shield; the same shield and wings with two rays; the same with three rays;
the same with three rays and one hard horizontal line cutting straight across the middle of the
shield.

Colour: rows 1 and 2 in cool steel greys and white; row 3 in steel with a blue accent; row 4 in
gold and warm brass with a white rim. Legible in silhouette at 26 pixels. Precise, austere,
issued military hardware — not decorative, not ornate.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, Warthog vehicles, or a rounded-dome full-face visor helmet
silhouette. No eagles, no laurel wreaths, no skulls, no stars, no national flags, no real-world
army or police insignia, no lightning bolts, no wings with feathers, no rounded shapes, no
circles or rings as the primary silhouette, no text, no letters, no numerals, no glow, no
photographic rendering, no drop shadows.
```

### Prompt B-2 — grades and banners (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#D9D9D9","#8A8A8A"]`
**Aspect:** 1:1

```
A single flat vector plate on a plain solid black background containing five separate white
military insignia components, all pure white, all identical stroke weight, all flat orthographic
front-on, arranged with generous even spacing and no overlap. No grid lines, no cell borders, no
text, no letters, no numerals.

The upper half of the image holds three small wide trapezoidal grade plates in a horizontal row,
each wider than it is tall, each a hard-edged flat white outline with a 2-unit stroke: the first
plate has one notch cut into its lower edge, the second has two notches, the third has three
notches. Uniform, mechanical, identical apart from the notch count.

The lower half of the image holds two tall narrow vertical ribbon banners side by side, each
roughly four times taller than it is wide, hanging from a flat top bar, each terminating in a
downward chevron cut at the bottom. The left banner is a plain flat white outline. The right
banner has a raised vertical centre seam running its full length and one small notch cut into
each side edge at the upper third.

Pure flat white line art on black. No fill gradients, no bevel, no shading, no shadow, no glow,
no perspective, no texture. Sharp corners throughout, zero rounded corners. Reads cleanly at
16 pixels.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, a halo ring motif, 343 Industries, Cortana, or Warthog
vehicles. No fabric folds, no tassels, no fringe, no embroidery, no heraldic ornament, no
laurels, no eagles, no stars, no text, no letters, no numerals, no drop shadows, no 3D
rendering, no rounded corners.
```

### Post-processing — Family B

1. **Delete the black background rect** from the returned SVG (it is a single `<rect>`).
2. Split the 4×4 sheet into 16 SVGs. In a vector editor this is select-by-cell; for raster
   fallback: `magick sheet.png -crop 4x4@ +repage rank_%d.png`.
3. **Normalise every insignia to a common optical weight** — the single most common sheet defect.
   Scale each so its bounding box fills 76×76 with a consistent 4px margin, then eyeball
   rows 1–4 side by side at 26px and fix outliers.
4. Recolour fills to tokens: ranks 1–4 `#D9D9D9`, 5–8 `#FFFFFF`, 9–12 `#6295DA` accent on white,
   13–16 `#E8BA3D`. Fill swap only, geometry untouched.
5. Export **76, 152 (@2x), 228 (@3x)** PNG with alpha, plus keep the SVG as source of truth.
6. Grades export at **42×32**, banners at **46×176**.
7. In UMG apply the **Medal 3D** effect to ranks only. Grades and banners get **nothing** —
   they are chrome, and `COMPONENT-SPECS.md` §7 scopes the effect to insignia and medals.
8. Import `T_Rank_01`…`T_Rank_16`, TextureGroup **UI**, **no mip maps**, sRGB on, alpha
   preserved (BC7 or uncompressed for the 76px set — they are tiny).

**Contingency:** if the top tier (row 4) reads weak against rows 1–3, re-run a **2×2 sheet of
ranks 13–16 only** at higher detail for +2.5 credits. Do not re-run the whole sheet.

---

# FAMILY C — Mode icons

| | |
|---|---|
| **Assets** | **14 large** + **4 gametype** = 18 |
| **Size** | Large **256×256** · Gametype **40×40** |
| **Aspect** | **1:1** (4×4 sheet, 14 used, 2 spare cells) |
| **Model** | `recraft_v4_1`, `model_type: "vector"` |
| **Sheet strategy** | **1 generation.** The 4 gametype icons are **derived in post** from the large sheet by stripping interior detail — 0 extra credits. |
| **Tier** | Large = **T2 MID**, white + cyan `#2EC3E5` VISR channel. Gametype = **T1 FLATTEST**, pure white. |

### The fourteen modes

| # | Mode | Glyph |
|---|---|---|
| 1 | BREACH | a wedge-shaped demolition charge on a wall bracket |
| 2 | RECOVERY | a hard-case with a top handle and a directional arrow through it |
| 3 | ATTRITION | four square pips in a row, the right two struck through by a diagonal |
| 4 | EXTRACTION | a downward hook over a landing bracket |
| 5 | WORKSHOP | an isometric wireframe cube with a corner node handle |
| 6 | CONTAGION | a hexagon with a fracture line spreading from one corner |
| 7 | HILL | a stepped plateau inside a square bracket |
| 8 | CLAIM | three stacked plates, the top one solid filled |
| 9 | LAST STAND | one square pip inside two closing brackets |
| 10 | RELIC | a faceted core held in a two-prong cradle |
| 11 | SKIRMISH | two crossed bars forming an X inside a reticle bracket |
| 12 | STOCKPILE | a bin outline divided into four cells, three filled |
| 13 | HOLDPOINTS | three squares in a row joined by a spine, each with a centre dot |
| 14 | DOMINION | three solid squares joined by a spine, with an arc bracket over all three |

### Prompt C-1 — fourteen large mode icons (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#2EC3E5"]`
**Aspect:** 1:1

```
A 4x4 grid of sixteen cells containing fourteen distinct flat geometric game-mode icons, each
icon centred in its own cell, evenly spaced with uniform margins, identical scale and identical
stroke weight across every cell, on a plain solid black background. The last two cells of the
bottom row are empty black. No grid lines, no cell borders, no frames, no text, no letters, no
numerals anywhere in the image.

Style: strictly flat two-dimensional vector iconography, orthographic front-on, no perspective,
no depth, no bevel, no shading, no gradient, no glow, no shadow. Two colours only — pure white
line work with a single bright cyan accent element per icon. Uniform heavy stroke weight
throughout, sharp square corners, zero rounded corners, zero circles used as a primary shape.
Angular and mechanical, drawn like a military instrument panel legend. Each icon must read in
silhouette at 40 pixels.

Row 1, left to right: a wedge-shaped demolition charge mounted on a flat wall bracket; a
rectangular hard case with a top handle and a straight arrow passing through it; four square
pips in a horizontal row with the right two crossed out by a single diagonal stroke; a downward
hook shape above a flat landing bracket.
Row 2, left to right: an isometric wireframe cube with a small square node at one corner; a
hexagon with a jagged fracture line spreading from its upper-left corner; a stepped plateau
shape enclosed by a square bracket open at the top and bottom; three stacked horizontal plates
with only the topmost one solid filled.
Row 3, left to right: a single square pip enclosed between two facing angular brackets; a
faceted diamond core held between two upward prongs; two crossed bars forming an X inside four
corner reticle ticks; a rectangular bin outline divided into four cells with three cells filled.
Row 4, left to right: three squares in a horizontal row joined by a straight spine, each square
containing a centre dot; three solid filled squares joined by a straight spine with a flat angular
bracket spanning above all three; then two empty cells.

The cyan accent is exactly one element per icon — the charge body, the arrow, the crossed pips,
the hook, the corner node, the fracture, the plateau step, the filled plate, the centre pip, the
core, the reticle ticks, the filled cells, the centre dots, the spanning bracket. Everything
else is white.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, Warthog vehicles, or a rounded-dome full-face visor helmet.
No skulls, no flames, no lightning bolts, no crosshair circles, no rounded corners, no
gradients, no glow, no drop shadows, no 3D rendering, no isometric shading, no text, no letters,
no numerals, no realistic firearms, no blood.
```

### Deriving the four gametype icons — no generation

The 40px gametype set is **SKIRMISH, TACTICAL, ATTRITION, CONTAGION**. Three of the four already
exist as cells 11, 3 and 6 of sheet C-1. In the vector editor:

1. Copy cells 11, 3, 6.
2. Recolour the cyan accent to `#FFFFFF` — the gametype icons are **T1, pure white monochrome**.
3. Delete interior detail: reticle ticks reduced to two, pip count preserved, fracture reduced
   to one straight break. Target ≤ 6 paths per icon.
4. **TACTICAL** is a new derivative: take SKIRMISH's crossed X and remove the reticle ticks,
   adding a single horizontal underline bar. Two minutes of vector work, zero credits.

### Post-processing — Family C

1. Delete the background rect; split into 14 SVGs, discard the two empty cells.
2. Normalise bounding boxes to **256×256 with a 16px margin**. Mode icons sit in large panels
   and inconsistent margin is highly visible.
3. Export large at **256** and **512 (@2x)**; gametype at **40**, **80**, **120**.
4. Recolour path: keep two named fill classes (`chrome` white, `visr` cyan) in the SVG so the
   HUD variant and the menu variant are one file with a swapped class.
5. `T_Mode_<Name>` / `T_Gametype_<Name>`, TextureGroup **UI**, no mips, alpha preserved.

---

# FAMILY D — Medals

| | |
|---|---|
| **Assets** | **16** (first wave) |
| **Size** | **64×64** |
| **Aspect** | **1:1** (4×4 sheet) |
| **Model** | `recraft_v4_1`, `model_type: "vector"` |
| **Sheet strategy** | **1 generation** for all 16. Optional **+1 `utility` generation** for a 2×2 hero-metal reference of the four highest medals. |
| **Tier** | **T3 — THE MOST SKEUOMORPHIC TIER IN THE ENTIRE LIBRARY.** This is the family where the fidelity rule is most visible. |

**Why vector and not utility for a skeuomorphic family.** The bevel lighting is supplied in
engine by the **Medal 3D** effect token (`COMPONENT-SPECS.md` §7). What the generation must
supply is **facet geometry and a flat metal ramp** — the chamfers, the raised rim, the struck
segments. A raster `utility` render would bake its own light direction and fight the effect, and
would not survive the 64px downsample. Vector gives crisp facets at 64px and lets the engine do
the lighting. This is the correct split, not a cost compromise.

### The sixteen medals

| # | Medal | Glyph |
|---|---|---|
| 1 | DOUBLE | two overlapping angular blades |
| 2 | TRIPLE | three stacked angular blades |
| 3 | RAMPAGE | five blades fanned from a common base |
| 4 | PRECISION | a hairline reticle ring with a solid centre spike |
| 5 | BREACHER | a wedge charge over a cracked plate |
| 6 | LAST ONE | a single upright pin inside an open bracket |
| 7 | CLUTCH | an hourglass wedge inside an angular frame |
| 8 | SAVIOUR | a shield chevron with a downward assist arrow |
| 9 | PERFECT | a diamond with an unbroken double border rule |
| 10 | KILLJOY | a broken chevron cut by a diagonal slash |
| 11 | QUICK DRAW | a bolt wedge over a horizontal slide bar |
| 12 | NO SCOPE | a reticle with the ring removed, only the cross remaining |
| 13 | RECOVERY | a hard case glyph with a return arc over it |
| 14 | OVERWATCH | a wide angular bracket with three downward ticks |
| 15 | DEMOLITION | a starburst of six hard wedges |
| 16 | UNTOUCHABLE | a closed hexagon containing one unbroken inner hexagon |

### Prompt D-1 — the sixteen medals (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#E8BA3D","#C9982B","#8A6A1C","#D9D9D9","#8A8A8A","#3A3A3A","#2EC3E5"]`
**Aspect:** 1:1

```
A 4x4 grid of sixteen distinct struck-metal achievement medallions, flat orthographic front-on
view, each medallion centred in its own cell, evenly spaced with uniform margins, identical
scale and identical outer diameter across every cell, on a plain solid black background. No grid
lines, no cell borders, no ribbons, no text, no letters, no numerals anywhere in the image.

Every medallion is the same base form: a hard-edged angular octagonal token with a raised
chamfered outer rim, a recessed inner field, and a single bold geometric device struck in relief
in the centre. Render as clean vector shapes using a stepped three-value metal ramp — a bright
chamfer on the upper edges, a mid-tone face, a dark chamfer on the lower edges — so the bevel
reads as faceted stamped metal. Flat facet shading only: no photographic specular highlight, no
soft gradient, no bloom, no glow, no cast drop shadow, no reflection. Sharp corners, zero
rounded forms except the hairline reticle rings where specified.

Row 1, left to right: two overlapping angular blade shapes; three stacked angular blades; five
angular blades fanned out from a common base point; a thin hairline reticle ring with a solid
pointed spike at its centre.
Row 2, left to right: a wedge-shaped charge above a cracked flat plate; a single upright pin
enclosed by an open angular bracket; an hourglass wedge inside a square angular frame; a
downward-pointing shield chevron with a straight arrow descending into it.
Row 3, left to right: a diamond enclosed by an unbroken double border rule; an upward chevron
broken in the middle and cut by a diagonal slash; a lightning-free angular bolt wedge above a
horizontal slide bar; a crosshair cross with no surrounding ring, only four straight arms.
Row 4, left to right: a rectangular hard case with a curved return arc above it; a wide flat
angular bracket with three short ticks descending from it; a starburst of six hard-edged wedges
radiating from a centre point; a hexagon containing a second unbroken hexagon inside it.

Metal treatment by row: rows 1 and 2 in cool gunmetal steel with a white rim; row 3 in brass and
warm bronze; row 4 in gold with a bright white rim. The central device is always brighter than
the recessed field behind it. Precise, heavy, physical, issued military commendation hardware.
Legible in silhouette at 32 pixels.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, Warthog vehicles, or a rounded-dome full-face visor helmet.
No skulls, no eagles, no laurel wreaths, no stars, no fabric ribbons, no hanging suspenders,
no national or real-world military decorations, no Olympic or sporting medals, no coins with
faces, no text, no letters, no numerals, no photographic rendering, no glow, no bloom, no lens
flare, no cast shadows on the background.
```

### Optional Prompt D-2 — hero metal reference (2×2, `utility`, +2.5)

Only run this if the vector metal ramp reads flat at 64px on a dark panel. It produces a raster
reference for the four gold medals whose ramp the vector fills are then hand-matched to.

**Config:** `model_type: "utility"` · `background_color: "#000000"` ·
`colors: ["#E8BA3D","#C9982B","#8A6A1C","#FFFFFF","#3A3A3A"]` · **Aspect 1:1**

```
A 2x2 grid of four struck gold military medallions, flat product photography, orthographic
front-on, each medallion centred in its own cell, identical scale, on a plain solid black
background. Angular octagonal tokens with a raised chamfered rim and a bold geometric device
struck in relief in the centre — top left a hard case with a return arc, top right a wide
bracket with three descending ticks, bottom left a six-wedge starburst, bottom right a hexagon
inside a hexagon. Aged brushed gold with darkened recesses, a bright polished chamfer, fine
machining marks, minor edge wear. Soft large overhead studio softbox with two strip lights for
edge definition, no hard specular hot spots. No ribbons, no text, no letters, no numerals, no
cell borders, no reflections on the background.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, UNSC, ODST, Covenant,
Banished, Forerunner, a halo ring motif, 343 Industries, or Cortana. No skulls, no eagles, no
laurel wreaths, no stars, no fabric ribbons, no real-world military decorations, no Olympic
medals, no text, no letters, no numerals, no hands, no people.
```

### Post-processing — Family D

1. Delete the background rect; split into 16 SVGs.
2. **Normalise outer diameter to exactly 64×64 with a 2px margin.** Medals sit in a 5-wide row
   on the Commendation Card; any diameter drift is instantly visible.
3. Verify the three-value ramp actually has three values. If the model collapsed it to two,
   split the mid-tone in the vector editor rather than rerolling.
4. Export **64**, **128 (@2x)**, and **256** (post-game reveal uses the large one).
5. **Apply the Medal 3D effect in UMG, not in the file** — `INNER_SHADOW #ffffff@0.8 (0,1) r0.5`
   · `INNER_SHADOW #000000@0.5 (0,-1) r0.5` · `DROP_SHADOW #000000@0.25 (0,2) r4` ·
   `DROP_SHADOW #000000@0.5 (0,1) r1`. Baking it kills the 256px variant.
6. `T_Medal_<Name>`, TextureGroup **UI**, no mips, alpha preserved, uncompressed at 64px.
7. **Never place a medal on front-end chrome.** `REFERENCE-EXTRACTION.md` §2 is explicit: the
   3D effect belongs on medals only, never on chrome.

---

# FAMILY E — Team emblems

| | |
|---|---|
| **Assets** | **9 emblem designs** × **N named palettes** + **4 backdrops** |
| **Size** | Badge **116×116** · Banner **680×128** · Backdrop **1000×776** |
| **Aspect** | Badge sheet **1:1** (3×3) · Backdrop sheet **1:1** (2×2, cropped to 1.29:1 in post) |
| **Model** | `recraft_v4_1`, `model_type: "vector"` for badges; `model_type: "utility"` for backdrops |
| **Sheet strategy** | **2 generations.** Badges: one 3×3 sheet of nine designs. Backdrops: one 2×2 sheet of four fields. **Banners are composited in engine, not generated.** **Palettes are recolours, not generations.** |
| **Tier** | Badges **T3-lite** — a shallow bevel, less than a medal, more than chrome. Backdrops are flat pattern fields. |

**The two-axis rule from `SCREEN-BUILD-SPEC.md` §6:** emblem *design* and named *palette* are
independent. One artwork, N reusable recolours. This only works if every design is authored as
exactly **three named colour zones** — `primary`, `secondary`, `detail`. The prompt enforces it.

**Why banners are not generated.** The 680×128 banner (5.31:1) is the badge device repeated or
centred over a backdrop field with the team stripe. That is a UMG composition of two assets
already in hand. Generating a third asset for it wastes 2.5 credits and creates a
drift risk between the badge and its own banner. Compose it.

### Prompt E-1 — nine emblem designs (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#2EC3E5","#FF5C00","#D9D9D9","#3A3A3A"]`
**Aspect:** 1:1

```
A 3x3 grid of nine distinct angular military unit emblems, flat orthographic front-on, each
emblem centred in its own cell, evenly spaced with uniform margins, identical scale and identical
stroke weight across every cell, on a plain solid black background. No grid lines, no cell
borders, no text, no letters, no numerals anywhere in the image.

Every emblem uses exactly three flat colour zones and no more: a large primary shape, a
secondary shape, and a small sharp detail accent. Hard-edged geometric vector shapes only, sharp
square corners, straight cuts, zero rounded corners, zero gradients, zero texture, zero shading
beyond a single shallow chamfer on the outer edge of the primary shape. Bold, poster-simple,
designed to read as a 26 pixel badge in a player roster row.

Row 1, left to right: three nested upward chevrons stacked tight; a hexagon split by a hard
diagonal into two unequal halves; a swept angular wing form built from three straight tapering
blades.
Row 2, left to right: a downward wedge driving into a horizontal bar; a trident of three vertical
bars rising from a common base plate; a lozenge cracked by a hard offset fracture line.
Row 3, left to right: an aperture formed from six straight blades meeting at a hexagonal centre;
an angular cross with a square centre plate and clipped arms; a delta triangle over three
horizontal ground bars of decreasing width.

Colour: the primary shape in white, the secondary shape in a bright cyan or bright orange, the
detail accent in dark grey. Alternate cyan and orange across the nine cells so both readings are
visible. Austere, industrial, insignia-grade. No mascot, no character, no illustration.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, or Warthog vehicles. No animals, no faces, no skulls, no
eagles, no wolves, no dragons, no flames, no lightning bolts, no laurel wreaths, no stars, no
national flags, no real-world military or sports team logos, no rounded corners, no gradients,
no glow, no drop shadows, no 3D rendering, no text, no letters, no numerals.
```

### Prompt E-2 — four emblem backdrops (one generation)

**Config:** `model_type: "utility"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#2EC3E5","#3A3A3A","#8A8A8A","#000000"]`
**Aspect:** 1:1

```
A 2x2 grid of four distinct flat abstract graphic background fields, each filling its own cell
completely edge to edge with no margin and no border, on a single square canvas. Each field is a
seamless flat pattern with no focal point and no central subject — these are backdrops that other
artwork will be placed on top of.

Top left: bold diagonal hazard bars at 45 degrees, alternating dark grey and near black, with a
thin cyan hairline between each band.
Top right: a topographic contour map field of thin concentric white hairlines on near black,
irregular organic contours, evenly distributed across the whole cell.
Bottom left: a hexagonal mesh grid of thin white hairlines on near black, fading smoothly to
pure black toward the lower edge.
Bottom right: a radar range grid — thin white concentric arc hairlines crossed by straight radial
spokes on near black, with one cyan arc.

All four are extremely low contrast and dark: near-black grounds, hairline linework, no bright
areas, no glow, no gradients other than the one specified fade, no texture noise, no photographic
elements, no perspective, no depth. Flat two-dimensional graphic design. No text, no letters, no
numerals, no icons, no logos.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, UNSC, ODST, Covenant,
Banished, Forerunner, a ring-shaped or arc-shaped halo world motif, 343 Industries, or Cortana.
No alien glyphs, no circuit board traces, no binary digits, no glowing holographic panels, no
text, no letters, no numerals, no faces, no characters.
```

### The palette axis — zero credits

Author each of the nine SVGs with three CSS classes (`.emblem-primary`, `.emblem-secondary`,
`.emblem-detail`). A named palette is then a three-hex row in a data table:

| Palette | primary | secondary | detail |
|---|---|---|---|
| `Standard` | `#FFFFFF` | `#2EC3E5` | `#3A3A3A` |
| `Ember` | `#FFFFFF` | `#FF5C00` | `#3A3A3A` |
| `Gunmetal` | `#D9D9D9` | `#8A8A8A` | `#000000` |
| `Signal` | `#FFC11C` | `#000000` | `#FFFFFF` |
| `Hazard` | `#FF4B4B` | `#FFFFFF` | `#000000` |
| `Deep` | `#6295DA` | `#FFFFFF` | `#000000` |

9 designs × 6 palettes = **54 emblems for 2.5 credits.** This is the highest-leverage generation
in the library.

### Post-processing — Family E

1. Delete background rect; split the 3×3 into 9 SVGs; assign the three colour classes by hand —
   the model will not name them, and this five-minute step is what makes the palette axis work.
2. Normalise to **116×116**, 4px margin.
3. Split the backdrop sheet: `magick backdrops.png -crop 2x2@ +repage bd_%d.png`, then
   **crop each to 1.29:1** (1000×776) from the centre.
4. Banner (680×128) is a **UMG composition**: backdrop field tiled across 680×128, badge centred
   at 116, plus a 2px palette-coloured bottom rule. No new asset.
5. `T_Emblem_<Design>` (SVG-sourced PNG at 116/232), `T_EmblemBackdrop_<Name>` at 1000×776,
   TextureGroup **UI**. Backdrops get mips (they scale); badges do not.
6. **Backdrops are not palette-varianted** — `SCREEN-BUILD-SPEC.md` §6 is explicit. Do not build
   a recolour axis for them.

---

# FAMILY F — Operator gear slot icons

| | |
|---|---|
| **Assets** | **16** (13 slots + 3 spare) |
| **Size** | **40×40** |
| **Aspect** | **1:1** (4×4 sheet) |
| **Model** | `recraft_v4_1`, `model_type: "vector"` |
| **Sheet strategy** | **1 generation**, all 16 |
| **Tier** | **T1 — FLATTEST.** These are pure menu chrome, the flattest thing in the library alongside H and J. |

Slots from `SCREEN-BUILD-SPEC.md` §6: Helmet · Visor · Helmet Attachment (forehead / chin /
left / right) · Chest · Left Shoulder · Right Shoulder · Wrist · Gloves · Weapon Model. Plus
Attachment (generic), Coating, Kit.

**The helmet icon is the single highest-risk asset in this document.** A helmet drawn from the
front with a rounded dome and a curved visor band is the Halo silhouette. The prompt below
mandates a **flat-crowned faceted profile with a rectangular visor aperture and a visible
mandible cage, drawn in three-quarter left profile, not front-on.** Reject any output where the
crown is a smooth arc.

### Prompt F-1 — sixteen slot icons (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` · `colors: ["#FFFFFF"]`
**Aspect:** 1:1

```
A 4x4 grid of sixteen distinct flat white equipment icons on a plain solid black background,
each icon centred in its own cell, evenly spaced with uniform margins, identical scale and
identical stroke weight across every cell. No grid lines, no cell borders, no text, no letters,
no numerals anywhere in the image.

Style: pure white monochrome outline iconography, one uniform medium stroke weight throughout,
strictly flat two-dimensional, orthographic, no perspective, no depth, no fill gradients, no
shading, no bevel, no glow, no shadow. Sharp square corners, straight cuts, minimal path count.
Drawn like an equipment manifest legend on a military instrument panel. Every icon must read at
16 pixels.

Row 1, left to right: a combat helmet in three-quarter left profile with a FLAT ANGULAR CROWN
made of straight facets, a rectangular visor aperture, and a hinged mandible guard along the jaw
— absolutely no rounded dome; a flat rectangular visor lens plate seen edge-on with two mounting
tabs; a small rectangular attachment module with a mounting rail, positioned as a forehead unit;
a short cylindrical attachment module with a rail clamp, positioned as a chin unit.
Row 2, left to right: a side-mounted attachment rail with a small square module on the left; the
same rail with the module on the right; a plate carrier vest outline, flat front view, with two
horizontal cummerbund straps and a square front plate; a left shoulder pad, a curved-free
four-sided angular plate with two strap tabs.
Row 3, left to right: a right shoulder pad mirroring the previous cell; a wrist unit, a flat
rectangular cuff with a small square display panel and a strap buckle; a tactical glove seen from
the back of the hand, angular and simplified, with a knuckle plate; a stylised rifle silhouette
reduced to straight geometric segments — a receiver box, a straight barrel line, a stock wedge,
a magazine block, with no trigger detail and no realistic firearm modelling.
Row 4, left to right: a generic accessory module, a plain rectangle with a mounting rail below
it; a paint drop shape rendered as an angular faceted teardrop; a stacked set of three
rectangular plates representing a full loadout kit; a plain square outline with corner ticks
representing an empty slot.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, Warthog vehicles, or a rounded-dome full-face visor helmet
silhouette. No smooth domed helmet crown, no wraparound curved visor band, no power armour, no
superhero suit, no motorcycle helmet, no diving helmet, no astronaut helmet, no faces, no
people, no rounded corners, no gradients, no shading, no glow, no drop shadows, no 3D rendering,
no photographic detail, no text, no letters, no numerals.
```

### Post-processing — Family F

1. Delete background rect; split into 16 SVGs.
2. **Inspect cell 1 first.** If the helmet crown reads as an arc rather than facets, discard that
   cell and hand-draw the helmet from the faceted profile description. Do not re-run the sheet
   for one cell.
3. Normalise to **40×40 with a 3px margin**; simplify every icon to ≤ 8 paths — these render at
   16px inside `Main Button`'s INSTANCE_SWAP slot (`COMPONENT-SPECS.md` §2).
4. Export **40**, **80 (@2x)**, and **16** (the button-row size, hand-checked, not just scaled).
5. Single fill class so hover inversion is a fill swap white → `#000000`
   (`COMPONENT-SPECS.md` §1: *idle → hover is an inversion*).
6. `T_Slot_<Name>`, TextureGroup **UI**, no mips, alpha preserved, uncompressed.

---

# FAMILY G — Coatings / finishes / patterns

| | |
|---|---|
| **Assets** | **8 finishes + 8 patterns = 16 chips**; coatings are generated in engine |
| **Size** | **100×100** art inside the **114×114** tile (`COMPONENT-SPECS.md` §5: Art sits at (7,7) 100×100) |
| **Aspect** | **1:1** (4×4 sheet, 2048×2048 → 512px cells) |
| **Model** | `recraft_v4_1`, `model_type: "utility"` — material chips need the render, not the vector |
| **Sheet strategy** | **2 generations.** One finishes sheet, one patterns sheet. **Coatings = colour token × finish chip, composited in engine — zero generations.** |
| **Tier** | Material chips — a real render, but flat-lit and front-on, no scene, no environment reflection. |

**The coating economy.** `SCREEN-BUILD-SPEC.md` §6 lists Coating · Finish · Pattern · Colour as
four orthogonal cosmetic layers. A coating is a colour applied through a finish. Generating a
chip per coating is combinatorial suicide. Generate the **8 finish chips as neutral greyscale
material**, then multiply the colour token in engine: 8 finishes × 12 colours = **96 coatings for
2.5 credits.**

### Prompt G-1 — eight finish chips (one generation)

**Config:** `model_type: "utility"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#D9D9D9","#8A8A8A","#3A3A3A","#000000"]`
**Aspect:** 1:1

```
A 4x4 grid of sixteen cells containing eight material finish swatches in the top two rows and
eight empty black cells in the bottom two rows, on a single square canvas. Each swatch fills its
cell completely edge to edge with no margin, no border and no gap. Every swatch is a flat
front-on macro photograph of a neutral grey material surface, lit identically by a single large
soft overhead light with no visible hot spot, no reflections of any environment, no vignette, no
background, no object, no edges — surface only, filling the frame.

Top row, left to right: matte polymer, a fine even micro-texture with no sheen; satin enamel
paint, a smooth surface with a soft broad sheen; brushed aluminium, fine parallel horizontal
grain; anodised titanium, a very fine even matte metal with a faint directional grain.
Second row, left to right: powder coat, a slightly orange-peel textured painted surface;
rubberised grip, a raised diamond stipple pattern; weathered steel, a scuffed and pitted metal
with fine scratches and edge wear; gloss lacquer, a smooth high-sheen surface with a broad even
highlight.

All eight are strictly neutral grey — no colour cast, no tint, no hue whatsoever. Identical
lighting, identical exposure, identical scale of detail across all eight so they read as one
material family. Bottom two rows are pure flat black. No text, no letters, no numerals, no
labels, no cell borders, no shadows.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, UNSC, ODST, Covenant,
Banished, Forerunner, a halo ring motif, 343 Industries, or Cortana. No objects, no products, no
hands, no people, no logos, no text, no letters, no numerals, no coloured tints, no environment
reflections, no studio background, no vignette, no glow, no rainbow iridescence.
```

### Prompt G-2 — eight pattern chips (one generation)

**Config:** `model_type: "utility"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#D9D9D9","#8A8A8A","#3A3A3A","#000000"]`
**Aspect:** 1:1

```
A 4x4 grid of sixteen cells containing eight seamless graphic patterns in the top two rows and
eight empty black cells in the bottom two rows, on a single square canvas. Each pattern fills its
cell completely edge to edge with no margin, no border and no gap, and is a flat two-tone
graphic pattern — pure white shapes on pure black, no shading, no gradients, no texture, no
lighting, no perspective, no depth.

Top row, left to right: bold diagonal hazard stripes at 45 degrees; a digital fracture pattern of
hard-edged angular shards at mixed scales; a splinter camouflage of long straight-edged tapering
wedges; a fine regular micro grid of thin hairlines.
Second row, left to right: a coarse irregular speckle of small hard-edged flecks; a repeating
field of tight upward chevrons; a topographic contour pattern of irregular concentric hairlines;
a scuffed wear pattern of directional streaks and hard-edged chipped patches.

All eight tile seamlessly, are the same visual density, and use the same two tones. Bottom two
rows are pure flat black. No text, no letters, no numerals, no labels, no cell borders.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, UNSC, ODST, Covenant,
Banished, Forerunner, a ring-shaped or arc-shaped halo world motif, 343 Industries, or Cortana.
No alien glyphs, no circuit traces, no binary digits, no skulls, no flames, no floral or organic
motifs, no realistic photographic camouflage, no text, no letters, no numerals, no colour, no
gradients, no glow.
```

### Post-processing — Family G

1. Split at 2048: `magick finishes.png -crop 4x4@ +repage fin_%d.png` → 512px cells; keep the
   top 8, discard the black 8.
2. Downsample to **100×100** with a Lanczos filter, then **verify the material still reads.**
   Brushed grain and micro grid are the two that die at 100px — coarsen the source crop before
   the downsample if needed.
3. Finish chips ship **greyscale, sRGB off, as a linear mask.** The colour comes from the token
   at runtime. This is what makes the 96-coating economy work.
4. Pattern chips ship as a **single-channel alpha mask** — white becomes the pattern colour,
   black becomes transparent.
5. Each chip is placed at **(7,7) inside the 114×114 `UBRItemTile`**, on top of the
   `#000000@0.5` background and under the rarity gradient (`COMPONENT-SPECS.md` §5). Do not bake
   the tile border into the chip.
6. `T_Finish_<Name>` / `T_Pattern_<Name>`, TextureGroup **UI**, no mips, BC4 for the masks.

---

# FAMILY H — Manufacturer marks · FAMILY J — Currency marks

| | |
|---|---|
| **Assets** | **6 manufacturer marks** + **2 currency marks** = 8 |
| **Size** | Manufacturer **40×40** · Currency **24×40** |
| **Aspect** | **2:1** (4×2 strip — 8 cells, exactly 8 assets) |
| **Model** | `recraft_v4_1`, `model_type: "vector"` |
| **Sheet strategy** | **1 generation for both families.** They share a tier, a colour (pure white), a background and a stroke weight — the only thing that differs is the cell aspect, and the currency marks sit in the two right-hand cells of the bottom row where the taller proportion is stated per-cell. **Family J costs zero additional credits.** |
| **Tier** | **T1 — FLATTEST.** |

`REFERENCE-EXTRACTION.md` §6 flags manufacturer icons against Eric Dies' *in-world branding*
note — these read as real companies stamped on real hardware, which is why they are wordless
device marks, not logotypes.

### The six in-world manufacturers

| Mark | Company | Makes | Device |
|---|---|---|---|
| 1 | KESTREL DYNAMICS | small arms, airframes | a swept tail-feather chevron |
| 2 | HALVORSEN ORDNANCE | heavy weapons, munitions | a crossed anvil bar |
| 3 | MERIDIAN ARMOURWORKS | plate carriers, shoulders | a latitude-lined lozenge |
| 4 | SUTRO OPTICS | visors, optics | a six-blade aperture hexagon |
| 5 | DRAYCOTT FIELD SYSTEMS | gloves, wrist, utility | a three-bar field-radio glyph |
| 6 | ORRIS POWERCELL | cells, energy | a stacked-cell square |

### The two currencies

| Mark | Name | Device |
|---|---|---|
| 7 | CREDITS (soft) | an upright elongated hexagonal token disc with a centre bore |
| 8 | MARKS (premium) | an upright milled bar with three horizontal ridge lines |

### Prompt HJ-1 — six brand marks and two currency marks (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#FFC11C"]`
**Aspect:** 2:1

```
A 4x2 grid of eight distinct flat white geometric marks on a plain solid black background, each
mark centred in its own cell, evenly spaced with uniform margins, identical stroke weight across
every cell. No grid lines, no cell borders, no text, no letters, no numerals, no words, no
lettering of any kind anywhere in the image.

Style: pure white monochrome, strictly flat two-dimensional, orthographic front-on, one uniform
medium stroke weight, sharp square corners, straight cuts, zero rounded corners, minimal path
count. These are wordless industrial device marks stamped onto military hardware — the kind of
abstract geometric mark found die-struck on a receiver or moulded into a polymer housing. No
fill gradients, no shading, no bevel, no glow, no shadow, no perspective. Each mark must read at
16 pixels.

Row 1, left to right: a swept tail-feather form built from four straight tapering blades fanning
to the right; two straight bars crossed over a flat anvil block; an elongated lozenge crossed by
three horizontal parallel lines of decreasing length; a hexagon formed from six straight blades
overlapping around a small hexagonal centre aperture.
Row 2 cells 1 and 2: three stacked horizontal bars of decreasing width rising from a short
vertical mast; a square divided into four stacked cell bands with the top band solid filled.
Row 2 cells 3 and 4 are taller and narrower than the others, each roughly three units wide by
five units tall: an upright elongated hexagonal token with a small square bore at its centre;
an upright rectangular bar with three horizontal ridge lines across its middle and chamfered
top and bottom edges.

Cells 1 to 6 are pure white. Cell 7 is pure white. Cell 8 is amber yellow. All eight share the
same visual weight and the same design language.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, or Warthog vehicles. No real-world company logos, no automotive
or firearms brand marks, no eagles, no animals, no faces, no skulls, no stars, no national
flags, no currency symbols such as dollar, euro, pound or yen, no coins with faces, no rounded
corners, no gradients, no glow, no drop shadows, no 3D rendering, no text, no letters, no
numerals, no words.
```

### Post-processing — Families H and J

1. Delete background rect; split into 8 SVGs.
2. Manufacturer marks normalise to **40×40**, 3px margin. Currency marks normalise to
   **24×40** — verify the taller proportion survived; if the model squared them, rescale
   non-uniformly and re-check the stroke weight, which will need thinning on the horizontal.
3. Currency also needs a **22×22** export — `COMPONENT-SPECS.md` §5 places a Currency badge at
   (81,11) inside the item tile.
4. `MARKS` (premium) is authored in `#FFC11C`; `CREDITS` in `#FFFFFF`. Single fill class each.
5. Manufacturer marks appear in the `Gear Detail` attribution row and in the
   `ANCHOR/MANUFACTURER-CODE NAME` attachment naming convention (`SCREEN-BUILD-SPEC.md` §6) —
   keep the mark-to-company mapping in the same data table as the code prefix, or the two drift.
6. `T_Brand_<Name>` / `T_Currency_<Name>`, TextureGroup **UI**, no mips, uncompressed.

---

# FAMILY I — Difficulty icons

| | |
|---|---|
| **Assets** | **4** |
| **Size** | **120×120** |
| **Aspect** | **1:1** (2×2 sheet) |
| **Model** | `recraft_v4_1`, `model_type: "vector"` |
| **Sheet strategy** | **1 generation** |
| **Tier** | **T2 — MID.** These carry the cyan VISR channel; they sit on the bot-difficulty picker, which `REFERENCE-EXTRACTION.md` §4 flags as the one carry-over from the dropped Difficulty Select screen (its radial layout). |

Four bot difficulties: **RECRUIT · MARKSMAN · VETERAN · GHOST.** Escalation is a wedge count
inside a hardening frame — the same structural-not-detail rule as the rank ladder, because these
render at 120px in a radial arrangement where all four are visible simultaneously.

### Prompt I-1 — four difficulty icons (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#2EC3E5","#FFC11C","#FF4B4B"]`
**Aspect:** 1:1

```
A 2x2 grid of four distinct flat geometric threat-level icons on a plain solid black background,
each icon centred in its own cell, evenly spaced with uniform margins, identical scale and
identical stroke weight across every cell. No grid lines, no cell borders, no text, no letters,
no numerals anywhere in the image.

Style: strictly flat two-dimensional vector iconography, orthographic front-on, no perspective,
no depth, no bevel, no shading, no gradient, no glow, no shadow. Uniform heavy stroke weight,
sharp square corners, straight cuts, zero rounded corners. Angular and mechanical, drawn as a
military threat-level legend. Each icon is built from the same two elements — upward-pointing
wedges and an enclosing angular frame — escalating in four steps.

Top left: a single upward wedge with no frame, in white.
Top right: two upward wedges side by side above a flat horizontal bar, the wedges in white, the
bar in cyan.
Bottom left: three upward wedges enclosed by an open angular hexagonal frame, wedges in white,
frame in amber yellow.
Bottom right: four upward wedges enclosed by a closed angular hexagonal frame with a solid filled
core behind them, wedges in white, frame and core in red.

The escalation must be legible at a glance with all four side by side: more wedges, tighter
frame, hotter accent. Austere, instrument-panel, not decorative.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, or Warthog vehicles. No skulls, no flames, no demons, no
lightning bolts, no stars, no difficulty stars or rating stars, no faces, no rounded corners, no
circles as a primary shape, no gradients, no glow, no drop shadows, no 3D rendering, no text, no
letters, no numerals.
```

### Post-processing — Family I

1. Delete background rect; split into 4 SVGs; normalise to **120×120**, 8px margin.
2. Export **120**, **240 (@2x)**, and a **40** variant for the compact settings row.
3. Keep two fill classes (`chrome` white, `level` accent) so the accent colour is data-driven
   from the difficulty row.
4. `T_Difficulty_<Name>`, TextureGroup **UI**, no mips.

---

# FAMILY K — Item render tiles

| | |
|---|---|
| **Assets** | **32** first wave (16 weapons + 16 gear). The reference has ~180 `Items` 114×114 nodes — this is wave one, not the full pool. |
| **Size** | **100×100** art inside the **114×114** tile |
| **Aspect** | **1:1** (4×4 sheet each, 2048×2048 → 512px cells) |
| **Model** | `recraft_v4_1`, `model_type: "utility"` — these are product renders, not icons |
| **Sheet strategy** | **2 generations.** One weapons sheet, one gear sheet. |
| **Tier** | Product render — 3/4 view, studio-lit, flat background. Not an icon; not a scene. |

**The lighting must be identical across both sheets** or the item grid reads as a collage. One
overhead softbox, two edge strips, same angle, same exposure. This is stated in both prompts and
is the thing to reject a sheet over.

### Prompt K-1 — sixteen weapon tiles (one generation)

**Config:** `model_type: "utility"` · `background_color: "#000000"` ·
`colors: ["#D9D9D9","#8A8A8A","#3A3A3A","#000000","#FFFFFF"]`
**Aspect:** 1:1

```
A 4x4 grid of sixteen distinct near-future military firearms, each weapon centred in its own
cell, evenly spaced with uniform margins, identical scale relative to its cell, identical camera
angle and identical lighting across every cell, on a plain solid black background. No grid lines,
no cell borders, no text, no letters, no numerals, no logos anywhere in the image.

Every weapon is shown in the same three-quarter view from the left side, barrel pointing left,
tilted 15 degrees, as a clean studio product render. Lighting is one large soft overhead box
light plus two narrow edge strip lights left and right for silhouette definition — no hard
specular hot spots, no coloured light, no environment reflections, no ground shadow, no
reflective floor. Weathered functional hardware: matte grey and gunmetal polymer and machined
alloy, visible fasteners, milled rail slots, worn paint on the edges, honest wear at the grip
and muzzle. Grounded and believable — real engineering, not fantasy.

Row 1: a compact bullpup carbine; a long-barrelled marksman rifle with a folding bipod; a short
submachine gun with a folding stock; a heavy squad automatic with a top-fed drum.
Row 2: a semi-automatic sidearm; a machine pistol with a foregrip; a pump shotgun with a heat
shield; a short breaching shotgun with no stock.
Row 3: a magnetic-rail marksman rifle with a slab-sided receiver; a grenade launcher with a
single break-open tube; a compact rocket tube with a folding sight; a suppressed integral
carbine.
Row 4: a rail-mounted optic sight unit alone; a angled foregrip unit alone; a box magazine alone;
a cylindrical suppressor alone.

Consistent design language across all sixteen — the same manufacturer sensibility, the same
material palette, the same fastener and rail vocabulary. No optics scopes on the weapons except
where specified. No hands, no people.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, plasma weapons, a ring-shaped or arc-shaped
halo world motif, 343 Industries, Cortana, or Warthog vehicles. No alien weapons, no glowing
energy cores, no purple or green bioluminescence, no plasma, no laser bolts, no fantasy or
magical weapons, no real-world manufacturer logos or model markings, no text, no letters, no
numerals, no hands, no people, no blood, no muzzle flash, no ammunition spilling, no cell
borders, no cast shadows on the background.
```

### Prompt K-2 — sixteen gear tiles (one generation)

**Config:** `model_type: "utility"` · `background_color: "#000000"` ·
`colors: ["#D9D9D9","#8A8A8A","#3A3A3A","#000000","#FFFFFF"]`
**Aspect:** 1:1

```
A 4x4 grid of sixteen distinct pieces of near-future military personal equipment, each item
centred in its own cell, evenly spaced with uniform margins, identical scale relative to its
cell, identical camera angle and identical lighting across every cell, on a plain solid black
background. No grid lines, no cell borders, no text, no letters, no numerals, no logos anywhere
in the image.

Every item is shown in the same three-quarter view from the front left, as a clean studio product
render. Lighting is one large soft overhead box light plus two narrow edge strip lights left and
right — no hard specular hot spots, no coloured light, no environment reflections, no ground
shadow. Weathered functional hardware: matte polymer, ballistic nylon webbing, machined alloy
fittings, worn edges, honest field wear.

Row 1, four combat helmets, each with a FLAT ANGULAR FACETED CROWN and a rectangular visor
aperture, absolutely no rounded dome and no wraparound curved visor band: a bare shell with a
chin harness; a shell with a hinged mandible jaw guard; a shell with a raised forehead rail
module; a shell with side rails and an ear-cup assembly.
Row 2, four visor and optic units shown detached: a flat rectangular tinted lens plate with
mounting tabs; a narrow monocular unit on a swing arm; a split two-pane lens assembly; a solid
armoured face plate with a horizontal vision slot.
Row 3, four torso items: a plate carrier vest with front plate and cummerbund; a chest rig with
four magazine pouches; a lightweight scout harness with a single shoulder strap; a heavy carrier
with a groin flap and shoulder yokes.
Row 4, four limb items: a left shoulder pad, angular and four-sided with strap tabs; a forearm
wrist unit with a small flat display panel and a buckle; a pair of tactical gloves with knuckle
plates, laid flat; a utility belt with three pouches.

Consistent design language across all sixteen — the same manufacturer sensibility, the same
material palette, the same buckle and webbing vocabulary. No people, no mannequins, no bodies.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, a ring-shaped or arc-shaped halo world motif, 343
Industries, Cortana, or Warthog vehicles. No rounded-dome helmet, no wraparound curved visor
band, no power armour, no exoskeleton, no superhero suit, no motorcycle helmet, no astronaut or
diving helmet, no glowing technology, no energy shields, no alien materials, no real-world brand
logos, no text, no letters, no numerals, no faces, no people, no mannequin heads, no cell
borders, no cast shadows on the background.
```

### Post-processing — Family K

1. Split at 2048: `magick weapons.png -crop 4x4@ +repage wpn_%d.png` → 512px cells.
2. **Background removal per cell** — key the flat black, then clean the alpha edge. This is the
   only family in the library where a real cutout is required, and it is the reason these sheets
   demand a *plain solid black* background rather than a scene.
3. Downsample to **100×100** Lanczos, with a **4px safe margin** inside the 100 so nothing
   touches the tile edge.
4. **Reject the sheet if lighting drifts.** Lay all sixteen crops in a row; if any cell reads
   brighter or lit from a different side, re-run rather than patch — a mismatched tile in a
   4-column grid is more visible than a missing one.
5. Item tiles sit at **(7,7) inside `UBRItemTile`**, above the `#000000@0.5` background and the
   rarity gradient, below the Locked / Currency / Favourite badges (`COMPONENT-SPECS.md` §5).
   Do not bake the tile border, the rarity line, or any badge into the art.
6. `T_Item_<Name>`, TextureGroup **UI**, no mips, BC7 with alpha.
7. Helmet cells get the same rejection test as Family F cell 1: **any smooth domed crown is
   discarded**, no exceptions, regardless of how good the render is.

---

# FAMILY L — The Breachpoint wordmark

| | |
|---|---|
| **Assets** | **1 primary lockup** (+ a stacked and a horizontal variant, + a mark-only badge) |
| **Size** | **304×118** on the splash screen (ratio **2.58:1**) |
| **Aspect** | Exploration sheet **1:1** · refinement **16:9** |
| **Model** | `recraft_v4_1`, `model_type: "vector"` |
| **Sheet strategy** | **2 generations, and this is the one family where that is correct.** Gen 1 is a 2×3 exploration sheet of six lockup directions. Gen 2 is the chosen direction alone on a full canvas at full detail. A wordmark cropped out of a 4×4 cell will not hold up at 304×118 on a black splash. |
| **Tier** | Its own tier — the identity mark. Flat, but with more craft in the letterform than anything else here. |

**Note on generated lettering.** Every other prompt in this library forbids text, because
generative letterforms are unreliable. This family is the exception, and it must be treated as
one: **expect the returned letterforms to be wrong.** The generation's job is to produce the
*form language* — the cut of the terminals, the stencil bridges, the breach notch, the
proportion. The final wordmark is then **redrawn in a vector editor from a real typeface**
(Rajdhani SemiBold or Bold, already the project's licensed OFL face per `REFERENCE-EXTRACTION.md`
§7b) with the generated form language applied. Budget the two generations as *direction-finding*,
not as final art.

### Prompt L-1 — six wordmark directions (one generation)

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#2EC3E5"]`
**Aspect:** 1:1

```
A 2x3 grid of six distinct logotype lockup explorations for a video game titled BREACHPOINT, on
a plain solid black background, each lockup centred in its own cell, evenly spaced with uniform
margins. No grid lines, no cell borders, no additional words, no taglines, no subtitles, no
symbols other than those described.

Every lockup spells the single word BREACHPOINT in uppercase in a heavy condensed angular
military typeface with flat cut terminals, sharp square corners, generous letter spacing, and a
strictly geometric construction. Pure flat white letterforms on black, no gradients, no shading,
no bevel, no glow, no shadow, no outline, no perspective, no 3D extrusion, no texture, no
distressing.

Top left: BREACHPOINT set on one line, wide and low, with a single hard horizontal notch cut
straight through every letter at the x-height line — the letters are physically broken by the
notch.
Top right: BREACH stacked directly above POINT on two lines of equal width, separated by a thin
horizontal cyan rule running the full lockup width.
Middle left: BREACHPOINT on one line with stencil bridges cutting the enclosed counters of the
letters R, P and O, so each counter is open on one side.
Middle right: BREACH stacked above POINT with the two lines offset horizontally, the lower line
shifted right, and a diagonal cut across the right edge of the whole block.
Bottom left: BREACHPOINT on one line inside a hairline rectangular bracket that is open at the
left and right ends, with short corner ticks.
Bottom right: BREACHPOINT on one line where the leading B and the trailing T are joined to the
lockup by short horizontal extension bars reaching out to the left and right edges.

Austere, industrial, confident, weighty. Reads as a competitive first-person shooter title, not
as a fantasy or space opera title. Every lockup must survive at 304 pixels wide.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, or Warthog vehicles. No ring, arc, circle or halo used as a
letter, a counter, or an ornament. No eagles, no skulls, no wings, no lightning bolts, no
crosshairs, no bullet holes, no scratched grunge textures, no chrome, no metallic gradients, no
neon glow, no drop shadows, no 3D extrusion, no bevel, no italic slant, no serif faces, no
handwritten or brush lettering, no additional words, no taglines, no version numbers, no
subtitles.
```

### Prompt L-2 — refinement of the chosen direction (one generation)

Run after L-1 is reviewed. Replace the bracketed clause with the description of the winning cell.

**Config:** `model_type: "vector"` · `background_color: "#000000"` ·
`colors: ["#FFFFFF","#2EC3E5"]`
**Aspect:** 16:9

```
A single logotype for a video game titled BREACHPOINT, centred on a plain solid black background
with wide even margins on all sides, occupying the middle third of the canvas. [INSERT THE
WINNING LOCKUP DESCRIPTION FROM L-1 HERE, VERBATIM.]

Refine to final quality: absolutely even optical letter spacing, identical stroke weight on every
vertical stem, flat cut terminals at exactly 90 degrees, perfectly aligned baselines and cap
heights, sharp square corners throughout with zero rounded corners, and a single consistent
geometric construction across all eleven letters. Pure flat white on black, one optional thin
cyan accent element only. No gradients, no shading, no bevel, no glow, no shadow, no outline, no
3D extrusion, no texture, no distressing, no perspective.

The lockup must hold its silhouette at 304 pixels wide by 118 pixels tall on a dark splash
screen. Heavy, condensed, angular, austere, industrial. No additional words, no taglines, no
subtitles, no symbols, no ornaments.

AVOID: Do NOT depict or reference Halo, Master Chief, Spartan armour, Mjolnir armour, UNSC,
ODST, Covenant, Banished, Forerunner, energy swords, a ring-shaped or arc-shaped halo world
motif, 343 Industries, Cortana, or Warthog vehicles. No ring, arc, circle or halo as a letter,
a counter or an ornament. No eagles, no skulls, no wings, no lightning bolts, no crosshairs, no
grunge texture, no chrome, no metallic gradient, no neon glow, no drop shadow, no 3D extrusion,
no bevel, no italic slant, no serif, no script, no additional words, no taglines, no subtitles.
```

### Post-processing — Family L

1. **Assume the letterforms are wrong.** Read the returned SVG for the form language, then set
   BREACHPOINT in **Rajdhani Bold**, convert to outlines, and apply the generated construction:
   the notch, the stencil bridges, the bracket, whichever won.
2. Kern manually. Eleven characters at the reference's enormous tracking (`Label/Button` is 10%,
   `Label/Tab` 15%) — machine kerning will not hold at 304px.
3. Snap every stem to the pixel grid at the **304×118 target size**, not at an arbitrary large
   size scaled down. This is a splash-screen asset with one canonical size.
4. Produce four locked exports:
   - `T_Logo_Primary` **304×118** — the splash lockup
   - `T_Logo_Primary_2x` **608×236**
   - `T_Logo_Horizontal` — single-line variant for the store page header
   - `T_Logo_Mark` **116×116** — the mark-only badge, for the emblem slot and the taskbar
5. Keep the SVG in `Content/Art/Brand/` as the source of truth; every raster is generated from it.
6. TextureGroup **UI**, no mips, alpha preserved, **uncompressed** — compression artefacts on a
   logo are visible and unforgivable.

---

# 4. Priority order — maximum visual impact per credit

Ordered by *how much of the product changes when this asset lands*, divided by cost.

| Rank | Family | Gen(s) | Credits | Why here |
|---|---|---|---|---|
| **1** | **A1 · A3 · A4** — three scene plates | 3 | **0.36** | `Start Menu Background` is the most-repeated Halo-owned asset in the source — it appears on **every screen**. Three plates at 0.36 credits change the look of the entire front end. Nothing else in this document comes close on impact-per-credit. |
| **2** | **E-1** — nine emblem designs | 1 | 2.50 | 9 designs × 6 palettes = **54 shipped emblems for 2.5 credits**. The highest multiplier in the library, and emblems appear in every roster row, every player card and every banner. |
| **3** | **L-1** — wordmark exploration | 1 | 2.50 | Identity. Everything downstream — store page, splash, Steam capsule — waits on this. It is also the longest lead-time item because it needs a human redraw after generation. |
| **4** | **B-1** — sixteen ranks | 1 | 2.50 | 16 assets, visible in every roster row at 26px and on the Career screen at full size. Unblocks the entire progression surface. |
| 5 | **C-1** — fourteen mode icons (+4 derived free) | 1 | 2.50 | 18 assets from one generation. Unblocks matchmaking, playlist select, match composer and voting — five screens. |
| 6 | **F-1** — sixteen slot icons | 1 | 2.50 | Unblocks the whole operator customization drill-down (`SCREEN-BUILD-SPEC.md` §3). |
| 7 | **D-1** — sixteen medals | 1 | 2.50 | High craft value, but post-match only. The T3 showpiece. |
| 8 | **K-1 · K-2** — item tiles | 2 | 5.00 | ~180 tiles eventually; these 32 make the grids stop looking empty. |
| 9 | **A2 · A5–A8** — remaining plates | 5 | 0.60 | Finishes the plate set. Cheap; deferred only because 1–8 unblock more screens. |
| 10 | **G-1 · G-2** — finishes and patterns | 2 | 5.00 | 96 coatings from 8 chips, but the coating layer is a later feature. |
| 11 | **HJ-1** — brand and currency marks | 1 | 2.50 | Small, detail-tier, needed for `Gear Detail` attribution. |
| 12 | **I-1** — difficulty icons | 1 | 2.50 | Bot difficulty picker only — one screen. |
| 13 | **B-2** — grades and banners | 1 | 2.50 | Chrome around the ranks. The ranks read fine without them. |
| 14 | **L-2** — wordmark refinement | 1 | 2.50 | Runs only after L-1 is reviewed and a direction is chosen. |
| 15 | **A9–A12** — alternate plates | 4 | 0.48 | Seasonal and variant plates. Genuinely optional. |

---

# 5. Credit budget

### Cheap vs expensive, stated plainly

- **CHEAP — `soul_location` @ 0.12.** Family **A only**. Twelve full-screen 16:9 plates cost
  **1.44 credits total** — less than a *single* vector generation. Plates are effectively free;
  generate variants liberally and pick the best.
- **EXPENSIVE — `recraft_v4_1` @ 2.5.** Families **B through L**. Every one of these is 20.8×
  the cost of a plate. This is why fifteen generations carry 130+ shipped assets: **the sheet
  strategy is the budget.**

### Running budget

| Gen | Family | Model / type | Credits | Cumulative | Assets produced |
|---|---|---|---|---|---|
| 1 | A1 main menu plate | `soul_location` | 0.12 | 0.12 | 1 |
| 2 | A3 operator backdrop | `soul_location` | 0.12 | 0.24 | 1 |
| 3 | A4 splash plate | `soul_location` | 0.12 | 0.36 | 1 |
| 4 | E-1 emblem designs | `recraft` vector | 2.50 | 2.86 | 9 designs → **54** with palettes |
| 5 | L-1 wordmark exploration | `recraft` vector | 2.50 | 5.36 | 6 directions |
| 6 | B-1 rank ladder | `recraft` vector | 2.50 | 7.86 | 16 |
| 7 | C-1 mode icons | `recraft` vector | 2.50 | 10.36 | 14 + **4 derived free** |
| 8 | F-1 slot icons | `recraft` vector | 2.50 | 12.86 | 16 |
| 9 | D-1 medals | `recraft` vector | 2.50 | 15.36 | 16 |
| 10 | K-1 weapon tiles | `recraft` utility | 2.50 | 17.86 | 16 |
| 11 | K-2 gear tiles | `recraft` utility | 2.50 | 20.36 | 16 |
| 12 | A2 matchmaking plate | `soul_location` | 0.12 | 20.48 | 1 |
| 13 | A5 loading plate | `soul_location` | 0.12 | 20.60 | 1 |
| 14 | A6 store plate | `soul_location` | 0.12 | 20.72 | 1 |
| 15 | A7 post-match plate | `soul_location` | 0.12 | 20.84 | 1 |
| 16 | A8 vista plate | `soul_location` | 0.12 | 20.96 | 1 |
| 17 | G-1 finish chips | `recraft` utility | 2.50 | 23.46 | 8 → **96 coatings** |
| 18 | G-2 pattern chips | `recraft` utility | 2.50 | 25.96 | 8 |
| 19 | E-2 emblem backdrops | `recraft` utility | 2.50 | 28.46 | 4 |
| 20 | HJ-1 brand + currency | `recraft` vector | 2.50 | 30.96 | 6 + **2 (Family J free)** |
| 21 | I-1 difficulty icons | `recraft` vector | 2.50 | 33.46 | 4 |
| 22 | B-2 grades + banners | `recraft` vector | 2.50 | 35.96 | 5 |
| 23 | L-2 wordmark refinement | `recraft` vector | 2.50 | 38.46 | 1 → 4 exports |
| 24–27 | A9–A12 alternate plates | `soul_location` | 0.48 | **38.94** | 4 |

### Totals

| | |
|---|---|
| `soul_location` generations | **12** × 0.12 = **1.44** |
| `recraft_v4_1` generations | **15** × 2.50 = **37.50** |
| **TOTAL** | **38.94 credits** |
| Distinct assets shipped | **~200** (139 base + palette and coating multiplications) |
| Effective cost per shipped asset | **≈ 0.19 credits** |

### Contingency (not in the total)

| Reroll | When | Credits |
|---|---|---|
| B-1b — ranks 13–16 at 2×2 | If the top tier reads weak beside rows 1–3 | 2.50 |
| D-2 — hero metal reference | If the vector metal ramp reads flat at 64px | 2.50 |
| C-2 — gametype icons generated rather than derived | If the derivations do not hold at 40px | 2.50 |
| One K-sheet re-run | If lighting drifts across cells (a reject, not a patch) | 2.50 |
| **Contingency ceiling** | | **10.00** |

**Worst case: 48.94 credits. Expected: 38.94.**

---

# 6. Appendix — families not covered by this wave

`REFERENCE-EXTRACTION.md` §6 lists more families than this document covers. Wave two, with the
sheet plan already decided so it can be run without re-planning:

| Family | Count | Size | Sheet plan | Credits |
|---|---|---|---|---|
| Misc. Icons | 23 | 40 | Two 4×4 vector sheets (16 + 7), T1 pure white | 5.00 |
| Button Icons | 10 | 16 | Rides along in the spare cells of Misc sheet 2 | 0.00 |
| File Icons | 5 | 40 | Rides along in the spare cells of Misc sheet 2 | 0.00 |
| Challenge Icons | 6 | 140 | One 3×2 vector sheet, T2 with cyan | 2.50 |
| Season Pass Icons | 4 | 100 | One 2×2 vector sheet, T2 | 2.50 |
| Core Icons | 5 | 40 | Rides along in the spare cells of a slot-icon re-run | 0.00 |
| Team Icons | 3 | 240 | Derived from E-1 at 240px — free | 0.00 |
| Loading Icon | 4 frames | 40 | One 2×2 vector sheet, the four sprite frames as four cells | 2.50 |
| Input Map Diagram | 1 | 591×291 | **Not generated.** Vector-drawn by hand — it is a keybind diagram and must be exactly accurate (`SCREEN-BUILD-SPEC.md` §5). | 0.00 |
| **Wave two total** | | | | **12.50** |

**Never generate:** the CRT scanline overlay (ship as a gradient, per `SCREEN-BUILD-SPEC.md` §5),
the scroll bars, the scrim, or any chrome component. Those are UMG geometry, not art. Generating
chrome is the fastest way to break the flat, hairline, sharp-cornered language this whole system
rests on.

---

# 7. Acceptance checklist — run before importing any generated asset

1. **Legal.** Does anything read as Halo? Rounded helmet dome, ring motif, olive power armour,
   energy blade? → **reject, do not "fix in post."**
2. **Tier.** Is the treatment correct for the family's tier? A bevel on a T1 slot icon or a flat
   fill on a T3 medal is a tier violation, not a taste call.
3. **Silhouette.** Does it read at its smallest shipped size — 16px for button icons, 26px for
   roster insignia, 32px for medals, 40px for slot icons?
4. **Weight.** Placed beside its siblings, is the optical stroke weight identical?
5. **Corners.** Any rounded corner that was not explicitly specified? `COMPONENT-SPECS.md` §0:
   corner radius is effectively **0**. Sharp corners are the language.
6. **Colour.** Are all fills token hexes from §1, or has the model invented a near-miss?
7. **Grounds.** Is the background genuinely removable, and is the alpha edge clean at 1px?
8. **Effects.** Is the Medal 3D effect applied **in UMG** and only to Family B ranks and Family D
   medals — never to chrome?
