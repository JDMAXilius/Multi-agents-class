# BN Feedback / Reticle (centre-screen) — measured breakdown

Source: file `yznvnVdOFDADaugZSeomfP`, page `6:48` (HUD / Elements). Read via the Figma MCP,
22 Aug 2026 (`get_metadata` + `download_assets` per node; `get_screenshot` used only to
cross-check ink position, never for alpha — GOTCHAS #9).

Assigned nodes: `62:12`, `62:17`, `62:20`, `62:29` (reticles) and `62:85`, `62:90`, `62:95`
(feedback). **Four of seven were not authored** — three are already-shipped duplicates, one is
a colour-only duplicate of a sibling in this same set. See below.

---

## Duplicates found and skipped (ASSET-RULES §1 — reuse before authoring)

| node | name | box | verdict |
|---|---|---|---|
| `62:12` | SET Reticle / Battle Rifle | 42.67×42.67 | **= `Content/UI/HUD/HUD_Reticle_BR.png`.** Metadata: 3 dots (ellipse, r2) at x{7, 19.3, 31.7} y19.3, + tick `62:16` x20.67 y8 1.33×6. Composited both PNGs on a dark ground (raw alpha isn't trustworthy per GOTCHAS #9) — pixel-identical. |
| `62:17` | SET Reticle / Magnum | 36×36 | **= `Content/UI/HUD/HUD_Reticle_Magnum.png`.** SVG: chevron path (`M8.93 0.9 L16.93 12.9...`) + a 2×2 dot at (17,17). Composited-pixel match. This is the node the ticket explicitly named to check — confirmed a duplicate, not authored. |
| `62:29` | SET Reticle / Enemy State | 42.67×42.67 | **= `Content/UI/HUD/HUD_Reticle_EnemyState.png`,** which is itself byte-for-byte identical to `HUD_Reticle_AR.png` (both: 4 arc paths, colour `#FF4A3D`, + a 4-arm centre cross, ticks `62:34`/`62:35`). Composited-pixel match against both files. |
| `62:90` | SET Feedback / Hitmarker Kill | 40×40 (see below) | **= `62:85` "Hitmarker" geometry**, colour only. Both nodes' four `rect` marks share IDENTICAL x/y/w/h/rotation (`M0..M3`, `10×1.6`, pivots at (6,7.07)/(25.8,7.07)/(6,26.87)/(25.8,26.87)`, rotate ±45°). `62:85` is white, `62:90` is `#FF4A3D` (`hud/threat`). Per the output rule (white line art, colour applied by C++), baking two colours would ship two byte-identical PNGs. One asset, `BN_Feedback_Hitmarker.png`, backs both states — C++ tints `hud/threat` for the kill case. |

Only `62:20` (Sniper) had no shipped reticle counterpart — `Content/UI/HUD/` ships AR, BR,
Magnum, EnemyState but no Sniper.

---

## Authored assets

| node | name | design box | shape | filename |
|---|---|---|---|---|
| `62:20` | SET Reticle / Sniper | **57.33 × 57.33** | 5 axis-aligned rects: `Bar1` (0,27.865,14,1.6), `Bar2` (43.33,27.865,14,1.6), `Bar3` (27.865,0,1.6,14), `Bar4` (27.865,43.33,1.6,14), `Centre` (27.5,27.5,2.3,2.3) — a crosshair with a centre-gap, bars reaching both box edges | `BN_Reticle_Sniper.png` |
| `62:85` / `62:90` | SET Feedback / Hitmarker (+ Kill) | **40 × 42** (see note) | 4 rotated rects, `10×1.6`, pivoted at their own (x,y) corner and rotated ±45° — a classic 4-tick hit-marker X with a gap at centre | `BN_Feedback_Hitmarker.png` |
| `62:95` | SET Feedback / Damage Direction | **120 × 120** | one closed cubic path, filled directly — a ring segment, outer r=60.00, inner r=51.60 (8.4-thick band), centred at the frame's own centre (60,60) | `BN_Feedback_DamageDirection.png` |

**Hitmarker's box is 40×42, not the ticket table's 40×40.** `get_metadata` reports 40×40 for
`62:85`/`62:90`, but the downloaded SVG's own `viewBox` is `0 0 40 42`, and the flattened ink
only reaches y=35.07 (comfortably inside 42, nowhere near 40). `preflight_textures.py`'s own
`EXPECTED_EXACT` table already records **40×42** for two siblings in this exact feedback family
(`HUD_Feedback_DamageDir`, `HUD_Feedback_ShieldBreak`) — trusted over the rounder metadata
number as the established convention.

**Damage Direction's placement required a fit, not a direct read.** `get_metadata` reports the
Arc child's bbox as the full 120×120 node at x=0,y=0 — that describes the underlying full
circle, not the crop's placement. Fitting a circle to the flattened outer curve (60 samples)
and inner curve (48 samples) separately gives two circles centred within 0.03 units of each
other, at LOCAL (48.07, 59.97), radii 60.00 and 51.60. Forcing that shared centre onto (60,60)
— the canvas centre implied by the 120×120 bbox — fixes the crop's offset at
**(+11.93, +0.03)**, applied to every point before scaling. Cross-checked independently against
`get_screenshot(62:95)`: measured ink bbox in the 120×120 render is x:12–58, y:0–28 — matches.

---

## Preflight results (`python3 mcp-ui/gen_ui/preflight_textures.py Content/BN/UI/Assets`)

All three fail on check 5 ("clipped") and pass every other check. Judged FALSE POSITIVES
(GOTCHAS #10 — the check assumes a padded icon; these are full-bleed shapes authored at their
exact Figma box):

| asset | opaque share | AA levels | finding | verdict |
|---|---|---|---|---|
| `BN_Reticle_Sniper.png` (115×115) | 1.7% | 32 | 12 edge px @ α>128 | **False positive.** Bars 1/3 start at x=0/y=0 and bars 2/4 end at x=57.33/y=57.33 — the box's own edges — by construction, straight from the SVG's own rect coordinates. A prior session hit the identical shape and quarantined it for the same reason at 14 px (`mcp-ui/gen_ui/quarantine/HUD_Reticle_Sniper.txt`); same geometry, same call. |
| `BN_Feedback_Hitmarker.png` (80×84) | 0.9% | 39 | 2 edge px @ α>128 | **False positive.** M0/M1's outer tip lands at y≈-0.00003 (floating-point zero) — the tick is drawn to touch the top edge by construction. A prior session hit this exact shape too, mislabelled as `HUD_Feedback_DamageDir` (`mcp-ui/gen_ui/quarantine/HUD_Feedback_DamageDir.txt` — its SVG's own `id` reads `"SET Feedback / Hitmarker"`), same 2-pixel finding. |
| `BN_Feedback_DamageDirection.png` (240×240) | 2.4% | 81 | 10 edge px @ α>128 | **False positive.** The arc's outer curve endpoint sits at y≈0.03 after placement — the ring's outer radius (60.00) is centred exactly on the frame's own centre (60,60), so this endpoint legitimately approaches the top edge of the 120×120 frame it's inscribed in. No prior extraction exists to cross-check against; judged from the fit above and the reference screenshot, both of which place the same endpoint at the same spot. |

None of the three trip corner-alpha (no baked background — all draw onto a transparent
canvas), none trip the 90% opaque-plate check (all under 2.5%), and all clear
`MIN_AA_LEVELS=3` by a wide margin (32/39/81).

---

## Could not extract

Nothing. All three authored shapes had either a usable SVG (Sniper: plain rects) or enough
raw geometry (Hitmarker: rects + explicit rotate transform; Damage Direction: one closed cubic
path) to reconstruct exactly — cross-checked against a screenshot in the one case (`62:95`)
where the numbers alone left an ambiguity (the crop offset).
