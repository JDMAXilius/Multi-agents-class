# `Tools/gen_ui/` — the Figma → UE asset pipeline

Five steps, in order. Each one exists because the step before it can produce something that
looks perfect in an image viewer and is wrong in the engine.

| # | Script | In → Out | Runs where |
|---|---|---|---|
| 1 | `figma_export.py` | Figma REST → `Export/UI/**.svg` | anywhere, needs `FIGMA_TOKEN` |
| 2 | `clean_svg.py` | SVG → SVG (backdrop stripped) | anywhere |
| 3 | `rasterize_svg.py` | SVG → `Content/UI/Icons/**.png` | needs Chrome/Chromium |
| 4 | `preflight_textures.py` | PNG → pass / `quarantine/` | anywhere |
| 5 | `import_textures.py` | PNG → `.uasset` | **live editor only, ONE driver** |

Steps 3 and 4 are joined: `rasterize_svg.py` calls `preflight()` itself, so nothing reaches
`Content/UI/` unvalidated. Step 4 is also runnable standalone over an existing folder.

## 1. `figma_export.py` — REST exporter

```
# one-time: Tools/env.local (gitignored)
#   FIGMA_TOKEN=figd_xxxxxxxxxxxxxxxxxxxx        (read-only scope is enough)

python3 Tools/gen_ui/figma_export.py --list-pages
python3 Tools/gen_ui/figma_export.py --audit 48:2
python3 Tools/gen_ui/figma_export.py --export 80:2 --family Glyphs --scale 4
```

File key `yznvnVdOFDADaugZSeomfP`. Pages: `48:2` Art/Insignia → Ranks, `68:2` Art/Icons,
`80:2` Art/UI Glyphs.

**Why this exists and the MCP does not:** Figma MCP tools are session-scoped. A subagent
gets "No such tool available" — verified, two crew lanes failed exactly that way. That
forces every export through one conversation relaying base64 by hand, which is expensive
and silently corruptible (one transcription slipped a character and produced an invalid
PNG). This script is plain HTTP, so bytes go Figma → disk without passing through a model,
and lanes can run in parallel. **It is the only export path a subagent can run.**

## 2. `clean_svg.py` — strip the baked page backdrop

```
python3 Tools/gen_ui/clean_svg.py Export/UI            # clean + verify
python3 Tools/gen_ui/clean_svg.py Export/UI --check    # verify only
```

**Figma's asset export composites the node against its PAGE BACKDROP.** In PNG that is
corner alpha 255. In SVG it is a full-canvas rect emitted before the content group:

```xml
<svg width="76" height="76" viewBox="0 0 76 76" ...>
  <rect width="76" height="76" fill="#F5F5F5"/>     <!-- the backdrop, not the art -->
  <g id="Rank / 01"> ... </g>
```

**41 PNGs were exported that way and discarded before anyone caught it.** It is invisible in
every image viewer; the defect only shows composited over a panel at runtime. In SVG it is
removable with certainty because it is identified structurally — a `<rect>` that is a direct
child of `<svg>`, covers the whole canvas, carries a solid fill, and sits before the first
`<g>` — not by guessing at the colour.

Post-clean it verifies: no full-canvas rect survives · width/height/viewBox present and in
agreement · at least one drawing element left, so "everything was stripped" cannot pass
silently.

Current state: `137/137 clean · 0 backdrop rect(s) removed` (2026-08-03).

## 3. `rasterize_svg.py` — SVG → PNG

```
python3 Tools/gen_ui/rasterize_svg.py Export/UI --scale 4 --out Content/UI/Icons
python3 Tools/gen_ui/rasterize_svg.py --probe       # which backends survive
python3 Tools/gen_ui/rasterize_svg.py --selftest    # prove the backend, no assets
```

UE cannot import SVG, so this is the last mile — and it is where the corruption creeps back
in, because most rasterisers composite onto an opaque page.

**A backend is never trusted because it is installed; it is probed.** The probe renders a
known 24×24 SVG at 4× and rejects anything that is not 96×96 with all four corners at
alpha 0, ink present, and real anti-aliasing. Live probe result:

```
no  cairosvg       No module named 'cairosvg'
no  rsvg-convert   not installed
no  qlmanage       corner alpha [255,255,255,255] — flattens transparency onto a background
no  sips           only 2 partial-alpha levels — strokes come out jagged
ok  chrome
chosen: chrome
```

The two macOS built-ins are kept in the list on purpose, so the rejection prints out loud
instead of being rediscovered inside an icon at runtime:

- **`qlmanage` flattens transparency onto white.**
- **`sips` quantises STROKE edges to 2 alpha levels while antialiasing FILLS to ~30.** This
  is the trap: a filled probe passes sips, and sips would then quarantine every arrow and
  checkmark in the set. The probe shape is therefore a stroke-only 45° checkmark — the
  hardest case in the real glyph set — held to preflight's own `MIN_AA_LEVELS`.

Scale is applied by **rewriting the SVG header to the target pixel size with the viewBox
left alone** (into a temp copy; `Export/` is never touched), so the backend re-renders the
vector at final resolution. Not by resampling: `sips --resampleWidth 304` on a 76px SVG
renders at 76 then upsamples, and the blur is measurable — 5980 partial-alpha pixels versus
816 for a true 4× render of the same file.

Run `clean_svg.py` first. A surviving backdrop rect shows up here as a corner-alpha failure.

## 4. `preflight_textures.py` — the 8-check gate

```
python3 Tools/gen_ui/preflight_textures.py [Content/UI/Icons] [--manifest m.json]
```

| # | Check | Fails when |
|---|---|---|
| 1 | RGBA | no alpha channel at all |
| 2 | Corner alpha | any corner ≠ 0 — a baked-in background (the 41-file bug) |
| 3 | Opaque share | >90% fully opaque — that is a filled plate, not a glyph |
| 4 | Dimensions | ≠ Figma source size × export scale, exactly |
| 5 | Clipping | ink touches the canvas edge — sources carry deliberate padding |
| 6 | Neutral ink | RGB spread > 12 — icons ship white and are tinted in UMG |
| 7 | Anti-aliasing | < 3 distinct partial-alpha levels — a 1-bit raster |
| 8 | Padding | **informational only**, never fails — it is a design call |

Failures go to `Tools/gen_ui/quarantine/<Family>/` (gitignored) as `<name>.png` plus a
`<name>.txt` holding the reason.

## 5. `import_textures.py` — UE import

```
python3 Tools/gen_ui/import_textures.py Content/UI/Icons/Glyphs /Game/UI/Icons/Glyphs
```

Requires a LIVE editor with the MCP server on `http://127.0.0.1:8000/mcp`.
**ONE DRIVER ONLY (R29.2)** — two sessions importing at once interleave with no transaction
boundary and there is no rollback, because a `.uasset` is binary.

Four settings, from `ASSET-PIPELINE.md` §4. The defaults are wrong for UI and the failure is
subtle: blurry icons and a texture budget three times bigger than it needs to be.

| Property | Value | Why |
|---|---|---|
| `lODGroup` | `TEXTUREGROUP_UI` | default World group lets texture-quality scalability blur the HUD on low spec |
| `compressionSettings` | `TC_EditorIcon` | = "UserInterface2D (RGBA)". Default DXT fringes 1px strokes |
| `mipGenSettings` | `TMGS_NoMipmaps` | UI draws ~1:1; mips waste memory and soften |
| `sRGB` | `True` for colour | a mask read as sRGB gives wrong values |

**Each is read back after setting**, because a wrong camelCase property name fails
SILENTLY — the call returns success and the property is untouched.

## Gotchas (all of these cost real time once)

- **Property names are camelCase and NOT guessable.** Run `ObjectTools.list_properties`
  against a real imported asset first. `lODGroup` really is spelled with that capital run —
  it is UHT's mangling of `LODGroup`.
- **`ObjectTools.set_properties` takes `values` as a JSON STRING**, not an object, and the
  target param is **`instance`**, not `object`.
- **Asset refs need the full `/Game/X/Y.Y` form** — `{"refPath": "/Game/UI/Icons/Glyphs/T_UI_Glyph_Filter_24.T_UI_Glyph_Filter_24"}`.
  A short path makes the server report **"input params Json is empty"**, which names the
  wrong cause entirely and sends you looking at your payload encoding.
- **`AssetTools.delete` takes `path`; `AssetTools.save_assets` takes `asset_paths`.** Not
  interchangeable, not consistent with each other.
- Under `bEnableToolSearch=true` the toolsets are reached via `call_tool` and never appear
  by name in a session tool list. That is why these scripts speak raw HTTP.

## Known open items

- **BP68** — the 16 `PARAM Rank` nodes on page `48:2` are misaligned in the source: their
  vectors are pinned to (0,0). Workaround in `figma_nodes.txt`: export the `Rank / NN`
  nodes instead and graft the names. Not fixed in Figma.
- **No 64 tier.** `ICON-CONSTRUCTION-SPEC.md` §6 asks for 64 / 40 / 16; only 40 and 16 (and
  120 for Difficulty) exist in `Export/UI/`.
- **Quarantine, 2026-08-03** — 7 files:

  | File | Reason |
  |---|---|
  | `Modes/T_UI_Icon_Mode_Assault_16` | 42 edge pixels carry ink — glyph is clipped |
  | `Modes/T_UI_Icon_Mode_Extraction_16` | 56 edge pixels carry ink — glyph is clipped |
  | `Modes/T_UI_Icon_Mode_LandGrab_16` | 66 edge pixels carry ink — glyph is clipped |
  | `Modes/T_UI_Icon_Mode_Elimination_16` | only 0 partial-alpha levels — export is 1-bit |
  | `Ranks/T_UI_Rank_11_Major` | ink is coloured (RGB spread 14) |
  | `Ranks/T_UI_Rank_15_Marshal` | ink is coloured (RGB spread 14) |
  | `Ranks/T_UI_Rank_16_Vanguard` | ink is coloured (RGB spread 14) |

  All 7 are **source-art bugs, not pipeline bugs** — they need a Figma fix and re-export.
  Assault and Extraction are also the two files with a 17×16 viewBox instead of 16×16.

## Not part of this pipeline

`build_wbp.py` / `wbp_plan.py` (UMG widget generation), `selftest_no_editor.py`,
`decode_batch.py` and `check_transparency.py` (one-off forensics from the 41-PNG incident)
live in the same folder but are separate tools.
