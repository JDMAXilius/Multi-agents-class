# `Export/UI/` — the vector source of truth

Committed SVG, straight out of Figma and cleaned. **The PNGs under `Content/UI/Icons/`
are generated FROM these files**, so re-rasterising the whole set at a different scale is
a local command, not a Figma round trip. Nothing here is disposable; if an SVG is missing,
someone has to go back to Figma.

Pipeline that produces and consumes this folder: `mcp-ui/gen_ui/README.md`.

## Families on disk (137 SVGs, counted 2026-08-03)

| Folder | SVGs | Figma page | Source dimensions | Notes |
|---|---:|---|---|---|
| `Ranks/` | 16 | `48:2` Art/Insignia | 76×76 | `T_UI_Rank_01_Recruit` … `_16_Vanguard` |
| `Modes/` | 28 | `68:2` Art/Icons | 14 × 40×40, 12 × 16×16, **2 × 17×16** | 14 modes × two tiers, see below |
| `ModeGlyphs/` | 14 | `68:2` Art/Icons | 23×23 | bare mode marks, no container |
| `Glyphs/` | 57 | `80:2` Art/UI Glyphs | 29 × 24×24, 28 × 40×40 | 28 pairs + `Add_24` (no 40) |
| `Gametype/` | 8 | `68:2` Art/Icons | 40×40 | two series: `Gametype_<Name>_40` and `GametypeV2_<Name>` |
| `Difficulty/` | 8 | `68:2` Art/Icons | 4 × 120×120, 4 × 40×40 | Recruit / Regular / Heroic / Legendary |
| `Currency/` | 2 | `68:2` Art/Icons | 24×40 | Credit, Token — the only non-square family |
| `Containers/` | 4 | `68:2` Art/Icons | 40×40 | Hex, Notched, Octagon, Ring — frames, not glyphs |

The two 17×16 files are `T_UI_Icon_Mode_Assault_16.svg` and
`T_UI_Icon_Mode_Extraction_16.svg`. Both are currently quarantined downstream for edge
clipping — the odd width and the clipping are the same bug.

Page attribution comes from the `PAGES` table in `mcp-ui/gen_ui/figma_export.py` plus the
header of `mcp-ui/gen_ui/figma_nodes.txt`. It is not recorded per file; there is no export
receipt for the SVG pass.

## Naming

```
T_UI_<Family>_<Name>[_<size>]
```

`Family` is one of `Rank`, `Glyph`, `Icon`. `Icon` carries a subfamily, so the real shapes
in use are:

```
T_UI_Rank_<NN>_<Name>            T_UI_Rank_07_Chief
T_UI_Glyph_<Name>_<size>         T_UI_Glyph_Filter_24
T_UI_Icon_<Sub>_<Name>[_<size>]  T_UI_Icon_Mode_Slayer_16 · T_UI_Icon_Container_Hex
```

`docs/ui/ue-frontend/ASSET-PIPELINE.md` §naming states the two-token form
`T_UI_<Family>_<Name>` only; the `Icon` subfamily token and the `_<size>` suffix are on
disk but not in that document. The filenames are the ones the importer and `Content/`
already use — treat the doc as the stale half.

## Why mode icons keep BOTH tiers as separate files

The 16 is **not** the 40 scaled down. It is a separate drawing with geometry deleted until
the small size still reads. `docs/ui/ICON-CONSTRUCTION-SPEC.md` §1:

> The line stays at 2–3 px everywhere on the sheet regardless of whether the icon is
> 18 px or 238 px wide. So when you halve an icon, the ink does not halve with it — the
> *relative* line weight doubles, adjacent strokes collide, and counters fill in. A
> straight scale-down at this stroke policy produces mud. The only way out is to remove
> geometry until what remains still has 2 px of clear air between every pair of lines.

and §6.1:

> **Draw three icons, not one.** Every icon in the system is authored three times: at
> **64**, at **40**, at **16**. Never export one and scale it.

So `Difficulty` (120 + 40) and `Modes` (40 + 16) each keep both files, and a rasteriser
scale factor is never a substitute for the missing tier. The 64 tier of that ladder is not
yet drawn in Figma for any family.

## Regenerate the PNGs

```
python3 mcp-ui/gen_ui/rasterize_svg.py Export/UI --scale 4 --out Content/UI/Icons
```

Vector in, RGBA PNG out, folder structure preserved. Every output goes through
`preflight_textures.py` before it lands; failures go to `mcp-ui/gen_ui/quarantine/`
(gitignored) with the reason beside them and never reach `Content/`. `--scale` is the only
knob you need for a different target resolution.

Do not edit an SVG by hand to fix a preflight failure — fix it in Figma and re-export, or
the next export silently reverts it.
