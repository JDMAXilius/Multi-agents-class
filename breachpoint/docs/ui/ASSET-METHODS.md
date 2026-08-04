# How to build the art without buying generations

**Written 2 Aug 2026, after burning 28.48 of 34 Higgsfield credits on four sheets.** The lesson
is not "generation is bad" — it is that **generation was the wrong default**. Most of what this
project needs is not an image problem. Ranked by cost and control, cheapest and most controllable
first.

---

## The ladder — stop at the first tier that produces the asset

| Tier | Method | Cost | Use for |
|---|---|---|---|
| **1** | **Parametric geometry in code** | £0 | Every icon that is describable as shapes: chevrons, bars, stars, hexagons, rings, ticks, frames, progress arcs, reticles |
| **2** | **Permissively-licensed icon libraries** | £0 | Generic UI glyphs — settings, lock, share, play, search, sort, delete |
| **3** | **Composition and recolour of what exists** | £0 | Rarity ramps, coatings, palettes, emblem variants, team colours |
| **4** | **Rendered from Unreal** | £0 | Every item tile, weapon render, operator render, and **the scene plates** |
| **5** | **AI generation** | paid | Only what none of the above can produce |

---

## Tier 1 — Parametric geometry. This is the big one.

Look at what the design system actually says its icons are:

> *"Strictly geometric: hard straight edges, mitred corners, uniform stroke weight, exact
> bilateral symmetry, flat front-on orthographic projection."* — our own prompt, written to
> describe the reference

**That is a specification for code, not a brief for an artist.** A chevron is six points. A
six-pointed star is twelve points on two radii. An outlined hexagon is two concentric polygons
with `EVENODD` winding. All of it is arithmetic.

```js
// a chevron: apex (cx, yTop), half-width hw, arm drop d, thickness t
// the inner edge is the outer edge translated down by t, so at y = yTop+d
// it has closed in to hw*(d-t)/d. Six points, exact mitres, no hand-drawing.
function chevron(cx, yTop, hw, d, t) {
  const k = (d - t) / d;
  return poly([[cx-hw, yTop+d], [cx, yTop], [cx+hw, yTop+d],
               [cx+hw*k, yTop+d], [cx, yTop+t], [cx-hw*k, yTop+d]]);
}
```

**What this buys that generation cannot:**
- **Exact consistency.** Every stroke is the same weight because it is the same variable.
- **No duplicates.** The AI rank sheet produced three near-identical two-chevron marks. Code
  cannot do that — the count is a parameter.
- **Editable forever.** Change `t` and all sixteen ranks re-weight together.
- **Free and instant.** No credits, no queue, no reroll lottery.
- **Diffable.** The generator lives in the repo; the asset is reproducible from source.

**Families this covers outright:** rank insignia (16) · grades (3) · mode icons (14) ·
gametype icons (4) · difficulty icons (4) · currency marks (2) · checkbox/radio/check ·
button prompts · carousel dots · progress arcs · the reticle set · damage-direction wedges ·
the Forge radial quadrants · scroll bars · the VISR frame linework.

That is **most of the icon inventory.**

### The gotcha that will bite you

**Figma normalises every vector node's path data to that node's own bounding box.** Build a
multi-part icon as several separate `VECTOR` nodes, then set each to `x = 0, y = 0`, and every
part collapses to the top-left corner. All relative positioning is destroyed, and destroyed
irrecoverably — the absolute coordinates are gone from the stored path data, so there is nothing
left to restore from.

This is not hypothetical. A 16-rank difficulty set rendered as one visible chevron, because three
identical chevrons were sitting exactly on top of each other. **41 of 42 generated components were
affected.**

**The fix: one compound path per component, `NONZERO` winding, then centre the single vector in
its box.**

- **One node** means Figma normalises the whole drawing as a unit, so the internal relationships
  survive.
- **`NONZERO` rather than `EVENODD`** because overlapping solid shapes must UNION — crossed
  blades, an arrow over a plate. `EVENODD` would knock a hole out of every overlap.
- **Rings still cut their counters correctly under `NONZERO`**, because the inner contour is wound
  backwards and opposite winding cancels. A hole is just a reversed polygon.

```js
const v = figma.createVector();
v.vectorPaths = [{ windingRule: 'NONZERO', data: paths.join(' ') }];
c.appendChild(v);
v.x = (w - v.width) / 2;   // centre AFTER assigning paths
v.y = (h - v.height) / 2;
```

The centring must come after the paths are assigned — `v.width` is meaningless until the node has
geometry to measure.

That reversed-winding rule is also what makes recognition holes work in a silhouette:
`hole = poly(points.reverse())`. Same points, opposite direction, and the fill drops out.

---

## Tier 2 — Licensed icon sets, restyled

For generic UI glyphs there is no reason to draw or generate anything. These are permissive and
commercial-safe:

| Set | Licence | Count |
|---|---|---|
| **Lucide** | ISC | ~1,500 |
| **Phosphor** | MIT | ~9,000 (6 weights) |
| **Tabler Icons** | MIT | ~5,900 |
| **Material Symbols** | Apache 2.0 | ~3,300 (variable axes) |

**Check each licence yourself before shipping** — they are permissive today, and that is a fact
to verify at ship time, not to take on trust from this document.

### Icon provenance

**49 Lucide icons are in the Figma file** (`Art / UI Glyphs`, 98 components across the 24 and 40
tiers). The SVGs were imported with their comment headers stripped for payload size, so nothing
in the artwork itself records where a given glyph came from — worth knowing if you ever need to
trace one back to its source set.
>
> **Still owed, and it is a UI packet not a legal one:** a credits screen that displays the
> staged file. `UBRCreditsScreen` is in the component inventory; until it exists the notice
> ships as a readable file beside the executable, which satisfies ISC, but a credits screen is
> what a storefront reviewer expects to find.

**Restyling applied on import** (permitted, and necessary to match our system):
`stroke-linecap: round → square`, `stroke-linejoin: round → miter`. The geometry is unchanged.

**The 40px tier uses source `stroke-width: 1.2`, not 2.** Because stroke weight is absolute,
`2 × (24/40) = 1.2` in source units renders as 2px at 40. Scaling the 24px version up instead
would give a 3.33px stroke and break the rule the whole system rests on.

**What libraries cannot supply:** game-specific iconography. No icon set ships `Oddball`,
`Strongholds`, a 16-step rank ladder, or a mode roster. Those stay Tier 1.

Import the SVG, then **restyle to our construction spec**: square the terminals, set the stroke to
our weight, force the 24-grid, strip the rounded joins. The result is ours in treatment even
though the topology came from the library. Covers: settings · lock · bookmark · share · play ·
search · sort · delete · download · refresh · friends · chat · report.

---

## Tier 3 — Composition, not creation

The reference file already proves the multiplier:

- **Rarity is a colour swap.** One item tile × 4 rarity tokens = 4 assets from 1.
- **Emblems are design × palette.** 9 designs × 6 named palettes = **54 emblems from 9 drawings.**
- **Coatings are finish × colour.** 8 finish treatments × 12 colour tokens = **96 coatings.**
- **Team colour is a token, not an asset.** Friendly/Enemy UI Colour is user-configurable in the
  reference — so it must be a bound variable, and a team variant is free.

**Rule: if two assets differ only by colour, they are one asset and a token.** Any pipeline that
generates them separately is wrong regardless of what it costs.

---

## Tier 4 — Render it in Unreal. This is the correct answer, not just the cheap one.

**The scene plates should never have been AI images.** Our own doctrine says so:

> *"Our front end lives over `BR_Arena01`, not over a black quad. The main menu's background is a
> camera in the arena… a menu with no scene behind it is a different, worse design."*
> — `ui-presentation` §1

An AI-generated arena is a **placeholder for a camera**, and a placeholder that looks finished is
worse than an obvious one, because it hides the missing work. The four plates generated are fine
as mood targets. They are not the asset.

Likewise the ~180 item tiles. Every shipping game renders these from the real models — a
turntable/hero-angle pass over the actual meshes, on a transparent background, at a fixed camera
and light rig. That is how the tile matches the thing you equip. An AI painting of a gun that is
not the gun in the game is a bug with good lighting.

**The mechanism already exists in this project**: the `ue-editor` skill covers the Python Editor
Script Plugin, commandlets and Remote Control, and R37 permits MCP-driven asset work **provided a
committed plan specifies it and a receipt is committed with the asset**.

Sketch:
1. A committed `render_plan.py` naming every mesh, camera angle, light rig and output size.
2. A commandlet that loads each mesh, frames it to the tile box, renders to PNG with alpha.
3. Output straight to `Content/UI/Tiles/`, receipt committed alongside.

Cost: zero. Fidelity: exact, because it *is* the asset.

---

## Tier 5 — What genuinely still needs generation

Short list, and it should stay short:

- **The wordmark**, and only as exploration — the final mark gets drawn in Rajdhani Bold by hand.
- **Medals**, if we want the skeuomorphic bevelled-metal tier without hand-modelling. The 16
  already generated may be enough forever.
- **Marketing key art** — not UI, and out of scope for this pass.

Everything else has a free path above.

---

## What happened to the 5.52 credits left

**They were not spent, and the work shipped anyway.** The rank ladder — the one actively defective
asset — was a Tier 1 job, not a Tier 5 one, and rebuilding it in code fixed the duplicate-rank
defect *and* made it editable. Spending 2.5 credits to re-roll the dice would have bought a worse
result than a script.

Built parametrically, for zero credits: the **rank ladder**, the **mode**, **difficulty**,
**currency** and **gametype** icons, the **glyph + container split**, and the **weapon
silhouettes**.

The remainder is still on the account, held for a genuine Tier 5 need.

---

## The cost mistake, recorded so it is not repeated

Cost was preflighted at `resolution: "1k"` (2.5 credits) and then generated at `2k`.

| resolution | credits |
|---|---|
| `1k` | 2.5 |
| `2k` | **10.0** |

Three sheets at 2k = 30 credits against a 7.5 estimate. **Preflight the exact parameter set you
are about to send, not a similar one** — `get_cost: true` takes one call and would have caught it.

For our sizes 1k is ample: insignia ship at 76px, icons at 16–40px. 2k bought nothing.
