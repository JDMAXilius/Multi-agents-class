# Chrome & identity — measured (Phase 0)

Figma file `yznvnVdOFDADaugZSeomfP`, page `6:48` (HUD / Elements) and page `6:49` (HUD /
Scoreboard) inside the MEASURED frame `43:2`. Numbers below are the literal `viewBox`/`d`/
`cx,cy,r` values `get_metadata` + `download_assets` returned per node, not eyeballed.
Generator: `mcp-ui/gen_ui/gen_chrome_art.py`. All output is white / neutral; team, self,
threat and medal-tier colour is C++'s at runtime (`hud/self`, `hud/team-them`, …) and is
never baked. Lane: chrome and identity — mode icons, team emblems, medals, tracker, world
markers. Nodes and files outside this list belong to the other two extraction lanes and
were not touched.

## Motion tracker — `24:2`, design box 133 x 133

| Node | Name | Shape | Stroke-w | Opacity |
|---|---|---|---|---|
| `24:4` | Rim | circle r=65.75 | 1.5 | 0.9 |
| `24:5` | Mid Ring | circle r=43.5 | 1 (default) | 0.55 |
| `24:6` | Inner Ring | circle r=21.5 | 1 (default) | 0.75 |

All three concentric at (66.5, 66.5) — one authored asset, `BN_Tracker_Face.png` (266x266).
`24:3` Field's own fill is a `hud/self` radial gradient (30%→6%) — coloured decorative wash,
dropped. `24:7..24:10` (ticks) and `24:11` (Self Chevron) returned **zero svgAssets** —
Figma itself judged them plain / rotated rectangles; a rotated `UImage` draws them with no
art, so they are not in this file. **No runtime consumer today** — BN has no motion tracker.

## Mode icon — `43:5` / `43:6` / `43:7`, design box 44 x 44 (inside scoreboard frame `43:2`)

| Node | Name | Shape | Stroke-w | Opacity |
|---|---|---|---|---|
| `43:5` | Outer (44) | circle r=19.5 | 5 | 1 |
| `43:6` | Inner (24) | circle r=9.5 | 5 | 1 |
| `43:7` | Core (14) | circle r=7, FILLED | — | 1 |

Three concentric circles sharing centre (22, 22) — the ticket's own read of this shape:
**one drawable asset**, `BN_Scoreboard_ModeIcon.png` (88x88), not three files. **No runtime
consumer today** — BN has no game-mode HUD glyph.

## Minimap — `62:54` (Clear) / `62:65` (Contacts) / `62:79` (Disabled), design box 140 x 138

The 140x138 component is the ellipse (140x120) plus an 18px text row (`Range`/`Callout`,
pure UMG text) below it — the art canvas is 140x120, not 140x138.

`62:54` and `62:65`'s `Field`/`Ring 2`/`Ring 3` SVGs are **byte-identical**, diffed directly,
not assumed. `62:65` (Contacts) adds only three blip circles (`62:74..62:76`, "Enemy Blip
1/2", "Ally Blip 3") — mock demo content at fixed positions, not a real minimap's layout (a
real widget places blips dynamically), so they are **not baked into the ring texture**. One
ring asset serves both states:

| Node | Name | Shape | Stroke-w | Opacity |
|---|---|---|---|---|
| `62:55`/`62:66` | Field (boundary) | ellipse rx=69.25 ry=59.25, centre (70,60) | 1.5 | 1 |
| `62:56`/`62:67` | Ring 2 (mid) | ellipse rx=42.9 ry=36.7 | 1 (default) | 0.5 |
| `62:57`/`62:68` | Ring 3 (inner) | ellipse rx=20.5 ry=17.5 | 1 (default) | 0.5 |

→ `BN_Minimap_Ring.png` (280x240), covers both `62:54` and `62:65`.

`62:79` (Disabled) is genuinely different art — one dim ellipse, no rings — and gets its own
file:

| Node | Name | Shape | Notes |
|---|---|---|---|
| `62:80` | Field Off | ellipse rx=69.5 ry=59.5, centre (70,60) | Figma: stroke `#4A5A6B`, fill `#1A2129`@35%. Both stripped to neutral white and re-expressed as alpha only: stroke 0.4 (duller than the active ring's 1.0), fill 0.10 (the active ring has no fill at all). Judgement call, not a measurement — a translucent WHITE plate cannot read as "dim" the way a translucent DARK plate does, so exact alpha was not preserved; what IS preserved is the one real distinction Figma draws between "ring" and "off" (has a fill, duller stroke). |

→ `BN_Minimap_Disabled.png` (280x240).

Spokes (`62:58..62:61`/`62:69..62:72`) and Self marker (`62:62`/`62:73`): zero svgAssets,
same rotated-rectangle story as the tracker. Not authored. **No runtime consumer today** —
BN has no minimap.

## Medal chip — `62:106`, design box 64 x 64

| Node | Name | SVG viewBox | Local offset | Fill (source → authored) |
|---|---|---|---|---|
| `62:107` | plate | 45 x 55.2156 | (10, 5) | `#8C949E` → white @ 35% |
| `62:108` | inlay | 27 x 35.2229 | (19, 15) | `#F2C752` → white @ 100% |

Both paths are absolute `M/L/V/Z` hexagons — inside `svg_pillow`'s supported grammar, so
`svg_pillow.render` draws them directly rather than a hand-rolled polygon. Composited onto
one 64x64 canvas at their measured offsets → `BN_Feedback_MedalChip.png` (128x128). This is
the chip **backing** only — no separate rank/medal glyph node exists under `62:106`; nothing
draws the actual medal icon that would sit in the inlay. **No runtime consumer today** — BN
has no medal system.

## Nameplate — `62:97`, design box 130 x 30

Only one child is art: `star` (`62:100`), an absolute `M/L/H/V/Z` path, fill `#36D1F2` →
white. SVG's own viewBox (17 x 15.5387) used as the box, not the metadata's 14x13 — the
export pads for the source's 1px stroke. → `BN_Feedback_NameplateStar.png` (34x31).

`Bar` (`62:98`, a 130x2 rule) and `Name` (`62:99`, text) are plain UMG — not authored.
**No runtime consumer today** — BN has no nameplate/feedback overlay.

## Interaction prompt — `62:101`, design box 220 x 24

Only one child is art: `Key` (`62:102`), circle r=8.5, stroke-width 1 (default), opacity 1,
box 18x18. → `BN_Feedback_KeyRing.png` (36x36).

`K` (`62:103`) and `Verb` (`62:104`) are text; `Hold Progress` (`62:105`, a 96x2 rule) is a
plain UMG rect — none authored. **No runtime consumer today** — BN has no interaction
system.

## Waypoint — `23:24`, design box 20 x 20 — NOTHING AUTHORED

`Outer` (`23:25`, 18.38 x 18.38) and `Inner` (`23:26`, 8.49 x 8.49) are both rotated squares
(18.38 = 13·√2, 8.49 = 6·√2 — a square rotated 45°). `download_assets` returned **zero**
`svgAssets` for this node — Figma itself judged both children plain rectangles. A rotated
`UImage` (RenderTransform) draws this diamond outline + filled diamond with no art at all.
Nothing to extract. **No runtime consumer today** — BN has no waypoint system.

## Location label — `23:13`, design box 240 x 16 — NOTHING AUTHORED

Single text node ("OUTPOST MERIDIAN ARMOURWORKS"), zero `svgAssets`. Pure UMG text; nothing
to extract. **No runtime consumer today**.

## Team emblems — `43:16`/`43:17` (EAGLE), `43:23`/`43:24` (COBRA) — PLACEHOLDERS, NOT ART

Both are labelled "ORIGINAL ART" in the Figma file. Checked directly, not assumed:
`download_assets` on `43:16` returns **zero `svgAssets` and zero `rawImages`**, and its
`export` PNG (44x44, 209 bytes) is a **single flat colour** — `(71, 128, 163)` sampled at
every one of the 1936 pixels, confirmed via `Image.getcolors()`. `43:17` ("EAGLE glyph")
is the same story at 401 bytes. `43:23`/`43:24` (COBRA) are byte-for-byte the same export
sizes (209 / 401), a different flat tint.

There is no vector glyph, no image fill, nothing drawable behind either label — just a
solid-colour rounded rectangle, which UMG already draws with a plain colour-and-opacity
box, no texture needed. **Reported as a gap, not invented**: no emblem art exists in this
Figma file for either team, despite the "ORIGINAL ART" label. If real team emblems are
wanted, they need to be designed — there is nothing here to extract.
