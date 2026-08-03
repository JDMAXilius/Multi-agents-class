# RECEIPT — HUD A1 whitening · 3 Aug 2026

**Packet:** add a whitening step to the UI raster pipeline so single-flat-colour HUD art
ships WHITE and is tinted in UMG from the palette; re-run the pipeline into
`Content/UI/HUD/` → `/Game/UI/HUD/`.
**Owner path:** `Content/UI/HUD/`, `Tools/gen_ui/`, `docs/ui/receipts/`.
**Machine:** macOS 25.5.0, UE 5.8 editor live, MCP `http://127.0.0.1:8000/mcp`, sole driver
(R29.2). **No build run** (R21). **No `Source/` edit. Nothing staged or committed.**
**Import receipt (written by the tool, not by hand):**
`docs/ui/receipts/import-textures-20260803T043805Z.md`

---

## 0. Honesty preamble

- **Rung: none.** An imported texture is not a rendered one. Everything below is asset
  inspection plus editor read-back. No PIE, no listen server, no packaged build. Nothing
  here says the HUD *looks* right — it says the textures exist, are white, keep their alpha,
  and carry the four UI settings.
- **7 of the packet's 14 A1 assets did not ship.** They are blocked by preflight check 5,
  which I was instructed not to weaken and did not weaken. §5 is the finding.
- **A sibling lane landed `Export/UI/HUD/parts/` (commit `6025697`) while this packet was
  running.** My first pipeline run picked those 10 new SVGs up via `rglob` and wrote
  `Content/UI/HUD/parts/`. That is not this packet's work; I deleted those outputs and
  re-ran against the 29 top-level frames only. §7.

---

## 1. Where the whitening went, and why

`Tools/gen_ui/rasterize_svg.py`, new `--whiten` flag. It rewrites the **SVG text**, in the
temp copy `rasterize()` already makes — not the rendered PNG. `Export/` is never touched.

A post-render RGB stomp was the obvious alternative and is worse on all three axes:

1. **Alpha.** Whitening the vector cannot touch alpha, because the geometry does not change;
   the renderer produces the same coverage. A pixel pass would have to be *argued* not to
   round an edge. Measured in §3.
2. **The colour removed is the exact source hex**, so the receipt can name the token the UMG
   tint must be set to. Sampled back off a raster it is an anti-aliased approximation.
3. **The guard reads paints, not pixels.** Two fills meeting along an edge blend through
   every intermediate hue in the raster; in the text they are two strings.

## 2. The guard, and proof it fires

`single_chroma(text)` raises `ValueError` — the render never happens — on:

| refuses | why |
|---|---|
| two or more chromas | a white master cannot reproduce them with one multiply |
| `url(#…)` paint | a gradient/pattern is not a flat colour |
| `<image>` | embedded raster pixels are not reachable from here |
| paint in `style=` or `<style>` | it would survive the rewrite and ship coloured |
| any paint it cannot parse | an unreadable paint stops the run; it is never assumed harmless |

Greys/black/white (RGB spread ≤ 4) are **not** chromas — nothing to preserve — but are
recorded. Chromas within 8 per channel are merged as ONE colour plus authoring drift; that
merge is what keeps `HUD_Ability_Grapple_Ready` (`#35D0F2` frame + `#36D1F2` glyph) from
being misread as a composite.

**It fires — `python3 Tools/gen_ui/rasterize_svg.py --selftest`, all self-tests PASS:**

```
  guard  accept  one chroma #36D1F2 + baked #000000 keyline
  guard  accept  #35D0F2 + #36D1F2 merged (delta 1) -> #35D0F2
  guard  REFUSE  two fills (HUD_Ammo_Readout)         multi-colour: #36D1F2, #8CBFE0 …
  guard  REFUSE  gradient (HUD_Vitals_ShieldHealth)   gradient/pattern paint 'url(#paint0_linear_6_48)' …
  guard  REFUSE  unreadable paint                     unreadable paint 'rgb(1,2,3)'
  guard  REFUSE  paint in a style attribute           paint in a style attribute/block …
```

The self-test asserts the negative case too: a fixture that is whitened when it should have
been refused raises *"the guard is a shredder, not a gate"*.

**And it fired on the real set.** All 9 A2 composites were refused by the guard, on their
own merits, with no allowlist anywhere in the code:

```
HUD_Ammo_Battery       multi-colour: #36D1F2, #8CBFE0
HUD_Ammo_Low           multi-colour: #FF4A3D, #8CBFE0
HUD_Ammo_Readout       multi-colour: #36D1F2, #8CBFE0
HUD_Minimap_Clear      gradient/pattern paint 'url(#paint0_radial_6_48)'
HUD_Minimap_Contacts   gradient/pattern paint 'url(#paint0_radial_6_48)'
HUD_Minimap_Disabled   multi-colour: #1A2129, #4A5A6B
HUD_MotionTracker      gradient/pattern paint 'url(#paint0_radial_6_48)'
HUD_Vitals_ShieldBroken multi-colour: #330F0F, #FF4A3D, #F5C542
HUD_Vitals_ShieldHealth gradient/pattern paint 'url(#paint0_linear_6_48)'
```

9 refused, 20 whitened, 29 total. The refusals are written to
`Tools/gen_ui/quarantine/<name>.txt` with the reason and **no PNG**, because nothing was
rendered — the file that would have been the white blob does not exist.

## 3. Alpha survival — measured, not assumed

The self-test renders each fixture twice, whitened and not, and compares the alpha channels.

```
  alpha  fill only       0/6256 px moved (max 0, tol 0) · silhouette intact · all ink RGB 255 · 20 AA levels
  alpha  fill + stroke  43/6256 px moved (max 1, tol 1) · silhouette intact · all ink RGB 255 · 64 AA levels
```

**Fill-only art comes back byte-identical.** Art whose path carries BOTH a fill and a stroke
does not, quite — 43 px of 6256 move by exactly 1. That is the renderer, not the rewrite,
and it is diagnosed rather than tolerated:

| test | ndiff | max Δ | silhouette |
|---|---|---|---|
| same file rendered twice, unwhitened (backend determinism control) | 0 | 0 | 0 |
| fill only, whitened vs not | 0 | 0 | 0 |
| real `HUD_Weapon_Sniper` (fill only), whitened vs not | 0 | 0 | 0 |
| real `HUD_Reticle_EnemyState` (fill only), whitened vs not | 0 | 0 | 0 |
| fill + black stroke (grenade glyph), whitened vs not | 43 | 1 | 0 |
| real `HUD_Ability_Grapple_Ready` (fill + stroke), whitened vs not | 28 | 1 | 0 |

The backend is deterministic, so the delta is not noise. It appears **only** where a fill and
a stroke are coincident: Skia composites the stroke as a second op over the fill, and when
the two paints become the same colour it takes a different rounding path along that seam. It
never exceeds 1 and **no pixel ever crosses 0 → non-zero**, so the silhouette — the thing
that must not move — does not move. The self-test asserts the measured bar (0 for fill-only,
≤1 for stroked, silhouette 0), not an aspirational zero that would have been deleted the
first time a stroked icon hit it.

On-disk confirmation of all 10 shipped PNGs: **RGB spread 0, minimum opaque channel 255**
(pure white ink), AA levels 37–241 intact.

## 4. Per asset — the colour removed and its token

Cross-referenced against `Tools/gen_ui/figma_tokens.json` `primitives`.

| asset | chroma removed | token | shipped |
|---|---|---|---|
| `HUD_Weapon_AR` | `#35D0F2` | `visr/shield` (= `hud/self`) | YES |
| `HUD_Weapon_BR` | `#35D0F2` | `visr/shield` | YES |
| `HUD_Weapon_Magnum` | `#35D0F2` | `visr/shield` | YES |
| `HUD_Weapon_Rocket` | `#35D0F2` | `visr/shield` | YES |
| `HUD_Weapon_Shotgun` | `#35D0F2` | `visr/shield` | YES |
| `HUD_Weapon_Sniper` | `#35D0F2` | `visr/shield` | YES |
| `HUD_Reticle_EnemyState` | `#FF4A3D` | `visr/enemy` (= `hud/threat`) | YES |
| `HUD_Grenade_Frag_Sel` | `#36D1F2` + baked `#000000` | **OFF-PALETTE**, nearest `visr/shield`, Δ1 | no — check 5 |
| `HUD_Grenade_Dynamo_Sel` | `#36D1F2` + baked `#000000` | **OFF-PALETTE**, nearest `visr/shield`, Δ1 | no — check 5 |
| `HUD_Grenade_Plasma_Sel` | `#36D1F2` + baked `#000000` | **OFF-PALETTE**, nearest `visr/shield`, Δ1 | no — check 5 |
| `HUD_Grenade_Spike_Sel` | `#36D1F2` + baked `#000000` | **OFF-PALETTE**, nearest `visr/shield`, Δ1 | no — check 5 |
| `HUD_Ability_Grapple_Ready` | `#35D0F2` (frame) + `#36D1F2` (glyph, merged) + baked `#000000` | `visr/shield` | no — check 5 |
| `HUD_Feedback_ShieldBreak` | `#FF4A3D` | `visr/enemy` | no — check 5 |
| `HUD_Feedback_Medal` | `#FF4A3D` | `visr/enemy` | no — check 5 |

Already-white, whitened as a no-op and shipped: `HUD_Reticle_AR`, `HUD_Reticle_BR`,
`HUD_Reticle_Magnum` (plus its black keyline, §6). `HUD_Reticle_AR.png` and
`HUD_Reticle_BR.png` re-rendered **byte-identical to the committed files** — they do not
appear in `git status`. That is a free determinism proof for the whole pipeline.

### Palette drift — reported, deliberately NOT normalised

`token_for()` names the token a colour **missed** and by how much; it never snaps. Snapping
would make the drift permanent and invisible to whoever owns the Figma frame.

- **`#36D1F2` is off-palette.** All four grenades and the grapple glyph use it; `visr/shield`
  is `#35D0F2`, which is what the six weapon icons use. Δ1 on R and G. Two hexes for one
  intended colour.
- **`#4A596B` in `HUD_Minimap_Disabled` is a one-digit miss of `visr/dead` `#4A5A6B`.** The
  guard's Δ8 merge folded them together and *still* refused the file, because `#1A2129` is a
  genuine second chroma. So the drift is reported without weakening the refusal.
- These frames use **raw hex, not bound variables**, which is the root cause of all of it.
  Filing that is the Figma owner's job, not this packet's.

## 5. FINDING — check 5 (clipping) blocks 7 of the 14, and it should not be weakened

After whitening, 7 A1 assets still fail preflight check 5. I did not touch the threshold.

Evidence, gathered independently rather than inherited: each source was re-rendered with the
`viewBox` grown 4 units per side and the padding ring measured.

| asset | edge px > 128 | ring px > 128 | **ring max alpha** | verdict |
|---|---|---|---|---|
| `HUD_Ability_Grapple_Ready` | 165 | 0 | **0** | tangent |
| `HUD_Grenade_Dynamo_Sel` | 40 | 0 | **0** | tangent |
| `HUD_Grenade_Frag_Sel` | 31 | 0 | **0** | tangent |
| `HUD_Grenade_Spike_Sel` | 4 | 0 | **0** | tangent |
| `HUD_Grenade_Plasma_Sel` | 2 | 0 | **0** | tangent |
| `HUD_Feedback_Medal` | 9 | 0 | **0** | tangent |
| `HUD_Feedback_ShieldBreak` | 2 | 0 | **0** | tangent |
| `HUD_Feedback_DamageDir` | 2 | 0 | **0** | tangent |
| `HUD_Reticle_Shotgun` | 66 | 0 | **0** | tangent |
| `HUD_Reticle_Sniper` | 14 | 0 | **0** | tangent |

Not "few pixels above threshold" — the ring's **maximum** alpha is 0. There is no ink outside
the authored frame, at all, in any of the ten. Nothing is truncated. Worked example:
`HUD_Reticle_Shotgun` is `<circle cx=26 cy=26 r=25.3 stroke-width=1.4>` in a 52 frame — outer
radius exactly 26.0, perfectly inscribed. `HUD_Ability_Grapple_Ready`'s glyph starts at
`y=0.5` and carries a default-width-1 stroke, so its coverage reaches exactly `y=0.0`.

The check is correct for the library it was written against (Lucide, authored on a grid with
deliberate padding). **HUD art is authored bbox-tight**, so "ink on the border row" means
"the artist filled the frame", not "the glyph is cut off". The check cannot tell those apart
from the PNG alone — the frame is the only thing that says which one it is, and that lives in
the SVG.

**Recommendation, NOT built here** (it is a preflight-contract change, not this packet):
the fix has an exact precedent in this same file. `aa_possible()` already lets the side that
holds the SVG discharge a proof obligation and conditionally exempt a preflight check. A
sibling `ink_is_tangent()` — geometry provably reaches but never crosses the viewBox — would
retire these 7 the same way, with the same "False is a proof obligation" discipline. Lowering
`>128` to anything else would be the wrong fix and I did not do it.

## 6. FINDING — `stroke="black"` baked into tint-ready art. Decision: whiten it, do not delete it.

`HUD_Ability_Grapple_Ready`, all four grenades, and `HUD_Reticle_Magnum` carry
`stroke="black"` on the same path as their coloured fill — an opaque black keyline baked into
art meant to be tinted.

**No preflight check catches it.** Check 6 measures **RGB spread**, and black's spread is 0;
its brightest-opaque floor is satisfied by any white pixel elsewhere in the image. A
white-glyph-with-black-outline passes preflight today and renders a dark halo at runtime that
no tint can remove.

**Decision: `--whiten` rewrites it to white along with everything else. It does not delete
the stroke.** Reasons, in order:

- Deleting geometry changes the silhouette (the glyph shrinks by half a stroke width) and
  would violate this step's own contract: *only RGB changes, alpha is preserved*. Geometry
  belongs to the designer; colour is the pipeline's business.
- A white keyline under a UMG tint renders as the tint colour — the glyph simply reads one
  unit heavier. That is a visual weight change, not a colour lie.
- If a **dark** keyline is actually wanted, it must be re-authored (a second texture, or a
  UMG outline) — that is a design call, and baking it into a tintable master is how you lose
  the option permanently.

Consequence, stated plainly: **`Content/UI/HUD/HUD_Reticle_Magnum.png` changed**, and its
`.uasset` was re-imported. It was one of the three already-shipped assets; its black outline
is now white. `HUD_Reticle_AR` / `HUD_Reticle_BR` are unchanged byte-for-byte.

The whitening step reports every achromatic paint it removed, so the next one is visible
rather than silent — but that is a report, not a gate. **Adding a real check for baked dark
ink is a preflight-contract change and is not in this packet.**

## 7. Counts at every stage

| stage | in | out | blocked |
|---|---|---|---|
| SVGs read (`Export/UI/HUD/*.svg`, top level) | 29 | — | — |
| whitening guard | 29 | **20 whitened** | **9 refused** (all A2, §2) |
| rasterise @4x (chrome backend, probed) | 20 | 20 rendered | 0 render failures |
| preflight in `rasterize_svg` | 20 | **10 ok** | **10 quarantined, all check 5** (§5) |
| `preflight_textures.py Content/UI/HUD --scale 4` | 10 | **10/10 pass** | 0 |
| `import_textures.py … /Game/UI/HUD` | 10 | **10 imported, 4/4 settings each** | 0 |

Of the packet's **14 A1 targets: 7 shipped** (6 weapon icons + `HUD_Reticle_EnemyState`),
**7 blocked by check 5** (4 grenades, grapple, medal, shieldbreak). The other 3 imports are
the already-white reticles that were re-rendered through the same run.

Backend probe, unchanged and re-run every invocation: `cairosvg` absent, `rsvg-convert`
absent, `qlmanage` rejected (corner alpha 255), `sips` rejected (2 AA levels), **`chrome`
chosen**.

## 8. Independent read-back (my own MCP calls)

Not the pipeline's numbers. Expected size parsed here from each source SVG's `viewBox` × 4;
size and properties fetched with a separate minimal MCP client.

```
asset                           viewBox     expect     engine match  settings
HUD_Weapon_Sniper                 94x31    376x124    376x124 OK     4/4 verified
HUD_Reticle_EnemyState            43x43    172x172    172x172 OK     4/4 verified
HUD_Reticle_Magnum                36x36    144x144    144x144 OK     4/4 verified
HUD_Weapon_AR                     94x31    376x124    376x124 OK     4/4 verified
```

Verbatim property read-back, identical for all four:

```json
{"lODGroup": "TEXTUREGROUP_UI", "compressionSettings": "TC_EditorIcon",
 "mipGenSettings": "TMGS_NoMipmaps", "sRGB": true}
```

## 9. What the UMG side must now do

Every shipped HUD texture is white. It is **wrong on screen until tinted** — a white weapon
icon is a bug report, not a finished asset. Per `figma_tokens.json` `_law`, the tint is read
from the palette and never typed into a WBP:

- `HUD_Weapon_*` → `hud/self` (`visr/shield`)
- `HUD_Reticle_EnemyState` → `hud/threat` (`visr/enemy`)
- reticles → `text/primary` (white) unless a state says otherwise

That wiring is a WBP packet, not this one. The accessibility payoff — a colourblind option
that can repoint `hud/threat` without re-exporting art — only exists once the tint is bound
to the token rather than to a literal.

## 10. Diff

- `Tools/gen_ui/rasterize_svg.py` — `--whiten`, `single_chroma()`, `whiten()`,
  `token_for()`, `selftest_whiten()`; `rasterize()` returns a third value (the paints
  removed). Two existing self-test call sites updated to `aa_ok, *_ =`.
- `Content/UI/HUD/` — 10 PNGs + 10 `.uasset`. `HUD_Reticle_Magnum.png` modified (§6);
  `HUD_Reticle_AR/BR.png` unchanged.
- `Tools/gen_ui/quarantine/` — refusal reasons rewritten for the 19 blocked frames. **Not
  in the diff**: `.gitignore:46` ignores the folder, so the reasons live only on this
  machine. Anything a reviewer needs from them is quoted in §2 and §5 above.
- This receipt.

Nothing staged, nothing committed, no `Source/` edit, no build.
