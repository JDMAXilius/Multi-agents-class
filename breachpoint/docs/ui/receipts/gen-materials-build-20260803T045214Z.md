# RECEIPT — UI material generator (build) · 2026-08-03T04:52:14.336038+00:00

**Command:** `Tools/gen_ui/build_materials.py`
**Plan:** `Tools/gen_ui/material_plan.py` sha256 `d588b1f8c2d427de`
**Transport:** `build_wbp.MCP`, imported. Raw HTTP `http://127.0.0.1:8000/mcp`.
**Caveat:** every `MaterialTools.*` call in this run is UNVERIFIED — written against an inferred schema without editor access (R29.2). Every unverified write is read back; an unverified call that no-ops fails the run.


## `M_UI_RadialSweep`  →  `/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep`

domain MD_UI · blend BLEND_Translucent · 34 nodes · Angular ring mask, clockwise from 12. Sweep 0 = nothing, 1 = closed circle.

- `AssetTools.exists` → absent; creating fresh
- **FAILED** `MaterialTools.create_material (UNVERIFIED)` — **FAILED** Function "create_material", input param "folder_path" is required by the function input schema Json, but is missing from the incoming function input params Json.
Function schema Json -
{"ty

## Findings
- **high** — MaterialTools.create_material (UNVERIFIED): **FAILED** Function "create_material", input param "folder_path" is required by the function input schema Json, but is missing from the incoming function input params Json.
Function schema Json -
{"ty

## Verdict
FAIL — see the failed calls above. 1 of 1 did not build.

## Rung honesty — what this PASS does not mean
- **Not a rung.** A compiled material is not a RENDERED one. Nothing here proves the sweep goes clockwise, starts at 12, or antialiases — that is a screenshot of `WBP_Screen_DeathRespawn` with `Sweep` stepped 0 / 0.25 / 0.5 / 1, and this generator cannot take it.
- **Does not prove the ring is the right THICKNESS.** `Thickness` defaults to a PROVISIONAL 0.0769: `figma_screen_layout.json` measures the 104px box and no stroke, and no respawn-ring art was ever exported. Filed as a contract_gap.
- **Not a multiplayer claim, not a PIE claim.** Law 6.
- **Unverified schema.** Every `MaterialTools` call was written without editor access. A PASS means the read-backs agreed, not that the calls were the right ones — run `--probe` and reconcile before trusting this receipt twice.
