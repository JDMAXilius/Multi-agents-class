# Loadout tray — measured (Phase 0)

Figma file `yznvnVdOFDADaugZSeomfP`, page `6:48` (HUD / Elements). Numbers below are the
literal `viewBox`/`d` values `get_metadata` + `download_assets` returned per node, not
eyeballed. Generator: `mcp-ui/gen_ui/gen_loadout_art.py`. All output is white / neutral,
0% opaque at the corners, tinted by C++ at runtime — colour is never baked.

## Grenades — design box 22 x 16.67 (frame incl. UMG-drawn "Count" text)

| Node | Name | Glyph viewBox | Shape | Authored |
|---|---|---|---|---|
| `61:27` | Frag | 11 x 16.28 | hexagon (elongated diamond) | `BN_Grenade_Frag.png` 22x33 |
| `61:30` | Plasma | 13.25 x 17.67 | diamond | `BN_Grenade_Plasma.png` 26x35 |
| `61:33` | Spike | 16.32 x 20.64 | 4-point star | `BN_Grenade_Spike.png` 33x41 |
| `61:36` | Dynamo | 11 x 13 | "S/Z" dynamite glyph, two subpaths | `BN_Grenade_Dynamo.png` 22x26 |
| `61:39` | Frag Unselected | identical `d` to `61:27` | — | **skipped, opacity dupe** |
| `61:42` | Plasma Unselected | identical `d` to `61:30` | — | **skipped, opacity dupe** |
| `61:45` | Spike Unselected | (not individually diffed — pattern below) | — | **skipped, assumed opacity dupe** |
| `61:48` | Dynamo Unselected | (not individually diffed — pattern below) | — | **skipped, assumed opacity dupe** |

**Selected vs Unselected is opacity, not shape.** Diffed `61:27` vs `61:39` and `61:30` vs
`61:42` directly: byte-identical `<path d="...">`, the Unselected copy adds only
`opacity="0.45"` on the `<path>`. `61:45`/`61:48` were not individually diffed — the pattern
held 2/2 times checked, so it is assumed rather than re-verified. If wrong for one family,
only that family needs a redo; either way this is a runtime alpha multiply, not a second
texture, per the ticket's colour/state law.

BN ships **one grenade type** today (per the ticket brief) — nothing in the current data
table reads Plasma, Spike, or Dynamo. All four are extracted anyway per the founder's
instruction; only Frag has a live consumer.

## Abilities — design box 50.67 x 30 (Ready/Cooling pair)

| Node (Ready) | Name | Glyph viewBox | Shape | Authored |
|---|---|---|---|---|
| `61:54` | Grapple | 14.41 x 20 | hook: arrow + shaft + top bar, 3 subpaths | `BN_Ability_Grapple.png` 29x40 |
| `61:60` | Repulsor | 17.41 x 17.41 | diamond ring — outer diamond, inner diamond is a punched hole (evenodd) | `BN_Ability_Repulsor.png` 35x35 |
| `61:66` | Threat | 21 x 21.51 | EKG/lightning pulse, single subpath | `BN_Ability_Threat.png` 42x43 |
| `61:72` | Drop Wall | 17 x 14 | 3 stacked bars | `BN_Ability_DropWall.png` 34x28 |
| `61:78` | Thruster | 11.70 x 18.53 | upward arrow/flame | `BN_Ability_Thruster.png` 23x37 |
| `61:84` | Overshield | 19 x 19.18 | hexagon ring — outer hexagon, inner hexagon is a punched hole (evenodd) | `BN_Ability_Overshield.png` 38x38 |

Cooling siblings (`61:57`, `61:63`, `61:69`, `61:75`, `61:81`, `61:87`) all **skipped**:
diffed `61:54` vs `61:57` and `61:60` vs `61:63` directly — same finding as grenades,
identical `d` plus `opacity="0.35"`. Cooling additionally overlays a `Cooldown Band (fills
UP)` `rounded-rectangle` (`61:59`, and one per sibling) — that is a plain rounded rect UMG
already draws (see the vitals bars), not art, and is not exported here.

**BN has no ability system today.** None of these six have a runtime consumer; extracted
per the founder's instruction as forward stock.

## Weapon silhouettes — design box 94 x 30.67

`Content/UI/HUD/` already ships `HUD_Weapon_{AR,BR,Magnum,Rocket,Shotgun,Sniper}.png`.
Compared every `78:x` node's Figma render (94x31, native) against the shipped PNG on a dark
backdrop before authoring anything (ASSET-RULES §1 — reuse before authoring):

| Node | Name | Result |
|---|---|---|
| `78:2` | Assault Rifle | **Identical** silhouette to `HUD_Weapon_AR.png` — skipped |
| `78:4` | Battle Rifle | **Identical** silhouette to `HUD_Weapon_BR.png` — skipped |
| `78:6` | Magnum | **Identical** silhouette to `HUD_Weapon_Magnum.png` — skipped |
| `78:8` | Sniper | **Identical** silhouette to `HUD_Weapon_Sniper.png` — skipped |
| `78:10` | Shotgun | **Different.** Figma: pump shotgun, rail/pump cutout under the barrel, no scope. Shipped PNG: bolt-action profile with a scope dot. Authored: `BN_Weapon_Shotgun.png` 156x45 |
| `78:12` | Rocket | **Different.** Figma: circular window on the tube, plain top rail. Shipped PNG: a stripe pattern instead, no circular window. Authored: `BN_Weapon_Rocket.png` 164x51 |

`download_assets`' `export` PNG for both `78:10` and `78:12` came back opaque share
**1.000** — the same page-background-composited trap as GOTCHAS #9/#2. The `svgAssets` SVG
(clean, absolute M/L/H/V/Z, no curves) was rasterised instead.

Silhouettes have a runtime consumer: BN's weapon table has Rifle/Pistol/Shotgun rows, which
is why Shotgun got the redraw. Rocket has no consumer in BN's current weapon table (kept
anyway since it was already half-measured; flag if it should be dropped).

## Preflight — judgement calls

`python3 mcp-ui/gen_ui/preflight_textures.py Content/BN/UI/Assets` flags every asset above
`WARN: matches no naming rule` (expected — `BN_*` isn't in the checker's size-convention
table; every other BN asset in this folder gets the same warning). Four hit a real check and
are judged FALSE POSITIVES per GOTCHAS #10, all for the same reason: the source SVGs are
tightly-cropped viewBoxes with **0% padding**, so the geometry legitimately touches or sits a
sub-pixel from the canvas edge — starting from a transparent canvas, a baked-in background is
structurally impossible.

- `BN_Ability_DropWall.png`, `BN_Ability_Grapple.png`, `BN_Grenade_Dynamo.png` — "corner
  alpha [...] background is baked in". Values are 0-19 out of 255 (AA bleed from geometry
  starting at `x=0.5`/`y=0.5` in an 11-17px box), not a filled corner. Overruled.
- `BN_Weapon_Shotgun.png`, `BN_Weapon_Rocket.png` — "edge pixels at alpha>128 — glyph is
  clipped". Both source `d` strings put a subpath flush on the viewBox edge by design (the
  rocket's end cap starts at `x=0`; the tail block's right edge sits at `x=82`, exactly the
  viewBox width). The silhouette is meant to run edge-to-edge here — clipping nothing.
  Overruled.

`BN_Vitals_HealthBar.png`/`BN_Vitals_ShieldBar.png` in the same preflight run belong to a
different agent's packet (vitals bars, not loadout) and are not addressed here.
