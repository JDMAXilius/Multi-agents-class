# RECEIPT — HUD raster + import · 2026-08-03T03:37:02Z (R37)

**Packet:** rasterise the 29 backdrop-stripped HUD SVGs at 4x, import to `/Game/UI/HUD`.
**Owner path:** `Content/UI/HUD/`, `Tools/gen_ui/quarantine/`, `docs/ui/receipts/`.
**Driver:** sole editor driver (R29.2). No build run (R21). No `Source/` edit. Nothing staged
or committed.
**Claim:** `.claude/active-packet.json` written before the first tool call — it did not exist,
so owner-path confinement was inert. Writing it turned the hook on for this session.

## Committed plan behind every call (R37.1)

`Tools/gen_ui/rasterize_svg.py` → `preflight_textures.py` → `import_textures.py`, all
pre-existing and unmodified. **No pipeline file was edited.** No check was weakened, no
exemption list added, `EXPECTED_EXACT` untouched.

## Stage counts

| stage | in | out |
|---|---|---|
| SVG sources in `Export/UI/HUD` | — | 29 |
| rasterised at 4x (chrome) | 29 | 29 rendered |
| preflight gate | 29 | **3 pass · 26 quarantined** |
| standalone preflight re-run on the staged 3 | 3 | 3 pass |
| imported to `/Game/UI/HUD` | 3 | **3 pass, 0 fail** |
| independent MCP read-back (29 names probed) | 29 | 3 PRESENT · 26 ABSENT |

Backend probe, unchanged and re-confirmed this run:

```
no    cairosvg       No module named 'cairosvg'
no    rsvg-convert   [Errno 2] No such file or directory
no    qlmanage       corner alpha [255,255,255,255] — flattens transparency onto a background
no    sips           only 2 partial-alpha levels — strokes come out jagged
ok    chrome
```

## Size baseline — derived independently, before the pipeline ran

Every one of the 29 `<svg>` headers was read directly and compared to
`preflight_textures.EXPECTED_EXACT`: **29/29 agree, zero drift.** The drift detector fired on
nothing, and no asset failed check 4. The export has not moved since the 3 Aug transcription.

## Imported assets — my own read-back, not the script's summary

Fresh MCP session, expectations computed from the SVG `viewBox`, never from pipeline output.

| asset | size read back | expected (viewBox @4x) | `lODGroup` | `compressionSettings` | `mipGenSettings` | `sRGB` |
|---|---|---|---|---|---|---|
| `HUD_Reticle_AR` | 172x172 | 172x172 (43x43) | `TEXTUREGROUP_UI` | `TC_EditorIcon` | `TMGS_NoMipmaps` | `true` |
| `HUD_Reticle_BR` | 172x172 | 172x172 (43x43) | `TEXTUREGROUP_UI` | `TC_EditorIcon` | `TMGS_NoMipmaps` | `true` |
| `HUD_Reticle_Magnum` | 144x144 | 144x144 (36x36) | `TEXTUREGROUP_UI` | `TC_EditorIcon` | `TMGS_NoMipmaps` | `true` |

4/4 settings verified on each, zero drift. The other 26 names were probed and returned no
texture — confirming the quarantine actually held and nothing leaked into the editor.

**Sample honesty:** "a sample across families" was not achievable. All three survivors are
the Reticle family. Nine of the ten HUD families have zero imported assets, so the settings
evidence covers one family only.

`AssetTools.list_assets` does not exist ("Unknown tool"), so `/Game/UI/HUD` was not
enumerated. Per-name probing of all 29 was used instead, which is stronger for
"nothing unexpected landed" than a listing would be.

## Quarantine — 26 assets, `Tools/gen_ui/quarantine/`, verbatim reasons

Two failure classes. Neither is a rasteriser defect; both are properties of the source frames.

### Class A — check 6, coloured ink (23 assets)

Verbatim, e.g. `ink is coloured (RGB spread 189) — icons must ship neutral and be tinted in UMG`.

**Real defect: yes, but it splits in two, and only one half is fixable by desaturation.**

*A1 — single-colour, genuinely tintable (14):* `HUD_Weapon_{AR,BR,Magnum,Rocket,Shotgun,Sniper}`
(all `#35D0F2`), `HUD_Grenade_{Frag,Dynamo,Plasma,Spike}_Sel` (all `#36D1F2`),
`HUD_Feedback_ShieldBreak`, `HUD_Feedback_Medal`, `HUD_Reticle_EnemyState` (all `#FF4A3D`).
One flat colour each. These are exactly what check 6 was written for: ship white, tint in UMG.

*A2 — multi-colour in one texture (9):* `HUD_Ammo_{Readout,Low,Battery}`,
`HUD_Minimap_{Clear,Contacts,Disabled}`, `HUD_MotionTracker`,
`HUD_Vitals_{ShieldHealth,ShieldBroken}`. `HUD_Minimap_Contacts` alone carries four distinct
fills. **A white master cannot reproduce these by tint** — a single multiply has one colour.
Desaturating them is not the fix; splitting them into per-colour layers, or shipping them
pre-coloured, is the decision. Check 6 is reporting a true fact about them and the remedy it
names does not apply.

`HUD_Minimap_Disabled` additionally trips `ink is grey (brightest opaque value 107)`. Correct
and unsurprising — it is the disabled state, authored dark (`#1A2129`/`#4A596B`). Under a
multiply tint it renders near-black against every palette colour.

`HUD_Ability_Grapple_Ready` carries `stroke="black"` on its glyph path — an opaque black
outline baked into a texture intended for tinting. Flagging separately; not a check-6 failure
(black has zero chroma spread) and not something any check catches.

*A3 — the reason A2 is coloured: several A2 frames are composite MOCKUPS, not textures.*
Their layer ids say so outright:

| asset | layer ids |
|---|---|
| `HUD_Ammo_Readout` / `_Low` / `_Battery` | `Mag`, `Reserve` — **the ammo numerals are baked in** |
| `HUD_Minimap_Contacts` | `Enemy Blip 1`, `Enemy Blip 2`, `Ally Blip 3`, `Range`, `Callout` |
| `HUD_MotionTracker` | `Self Chevron`, **`59 m`** — a baked range readout |
| `HUD_Vitals_ShieldHealth` | `Health (nested, hidden until damaged)` — a conditional state layer |

An ammo readout with the digits painted on cannot display ammo; a minimap with contacts
painted on is a picture of a minimap. The colour failure is the *symptom* — the blips are red
because they are blips. These frames need decomposing into a static plate plus widget-driven
elements before "what colour should the texture be" is even the right question. **No check in
the pipeline detects this**; it was found by reading the layer ids.

### Class B — check 5, clipping (14 assets, 11 also in class A)

Verbatim, e.g. `66 edge pixels at alpha>128 — glyph is clipped` (`HUD_Reticle_Shotgun`);
`91 edge pixels` (each Minimap); `165` (`HUD_Ability_Grapple_Ready`); `2`
(`HUD_Feedback_DamageDir`, `HUD_Feedback_ShieldBreak`).

**Real defect: partially. The ink is not truncated — it is tangent.** Check 5's premise is
"the source frames all carry deliberate padding, so ink at the edge is a bug." That premise
holds for the Lucide-style icon library and **does not transfer to HUD art**, which Figma
authored bbox-tight.

Evidence, run as a throwaway diagnostic (nothing written to the repo): each of the 16
candidates was re-rendered with the viewBox grown 4 units on every side at the same device
scale, and the padding ring measured. **All 16: zero pixels above alpha 128 in the ring,
max ring alpha 0 — except the three Minimaps at max alpha 2.** No geometry exists outside any
frame. Confirmed in the sources: `HUD_Reticle_Shotgun` is `circle r=25.3 stroke-width=1.4`
at centre (26,26) → outer radius exactly 26.0 in a 52-unit frame, perfectly inscribed;
`HUD_Reticle_Sniper` bars 1 and 3 start at exactly x=0 / y=0.

So nothing is missing — but the outermost AA feather has nowhere to land, so those edges
rasterise harder than authored, and there is no bleed margin for any future filtering. That
is a genuine and cheap-to-fix export property, not a corrupted asset.

**Not suppressed, not exempted.** All 26 remain quarantined. Fixing this belongs to whoever
owns `Export/` or `rasterize_svg.py` — neither is in this packet's owner path.

## Check 7 (anti-aliasing)

Fired on nothing. The anticipated axis-aligned exemptions were never needed: every HUD render
came back with real partial alpha (`HUD_Reticle_AR` 241 levels, `BR` 88, `Magnum` 76).
`HUD_Reticle_Sniper`'s five rects and the two 4-rotated-rect indicators were stopped by
check 5 before check 7 could have exempted them.

## Observation, not a finding

4x on this family produces large textures: `HUD_Vitals_ShieldHealth` would land at 1108x140
and each Minimap at 560x552. A 277pt-wide bar at 4K with a 2x DPI scale draws at ~554px, so
4x is roughly one power of two more than the HUD needs. The packet specified 4x and 4x is what
ran. Raising it as a texture-budget question for whoever owns the UI memory budget.

## Rung honesty

- **Rung claimed: "the asset exists in the editor with verified settings."** Three assets, four
  settings each, verified by an independent read-back in a separate MCP session against
  expectations derived from the source SVG headers.
- **Not PIE, not multiplayer, not packaged.** Nothing was rendered in a widget. No build was
  run. That these three textures look correct on screen is unproven.
- **26 of 29 did not land.** This packet did not deliver the HUD texture set; it delivered three
  reticles and a diagnosis of why the other 26 are blocked.
