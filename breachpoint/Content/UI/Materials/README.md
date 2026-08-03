# `Content/UI/Materials/`

~4 total. Gradients, scanlines, radial fills — cheaper and recolourable versus a texture
(`docs/ui/ue-frontend/ASSET-PIPELINE.md:29,50`). Used as a material brush on an `Image`.

**Naming:** `M_UI_<Effect>` — `M_UI_Scanline`, `M_UI_GradientFade`
(`ASSET-PIPELINE.md:83`). Instances: `MI_UI_<Effect>_<Variant>`.

Material graphs are Tier 4 (`BREACHPOINT-AUTHORING-MATRIX.md` §2, Tier 4) because a material
graph has no C++ authoring path. That is the whole justification; it does not extend to anything
else in this tree.

**Never here:**
- A material that carries a gameplay number or makes a gameplay decision. Parameters are driven
  at runtime from C++ via `UMaterialInstanceDynamic`; values come from `Content/Data/*.csv`
  (law 3).
- A material standing in for a flat fill. A solid colour is a `Border` brush with a colour from
  `UBRUISettings` — zero assets.
- Textures. `T_*` belongs in `Icons/` or `Art/`.
