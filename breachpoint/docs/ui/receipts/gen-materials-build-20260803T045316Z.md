# RECEIPT — UI material generator (build) · 2026-08-03T04:53:16.502592+00:00

**Command:** `Tools/gen_ui/build_materials.py`
**Plan:** `Tools/gen_ui/material_plan.py` sha256 `d588b1f8c2d427de`
**Transport:** `build_wbp.MCP`, imported. Raw HTTP `http://127.0.0.1:8000/mcp`.
**Caveat:** every `MaterialTools.*` call in this run is UNVERIFIED — written against an inferred schema without editor access (R29.2). Every unverified write is read back; an unverified call that no-ops fails the run.


## `M_UI_RadialSweep`  →  `/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep`

domain MD_UI · blend BLEND_Translucent · 34 nodes · Angular ring mask, clockwise from 12. Sweep 0 = nothing, 1 = closed circle.

- PASS `AssetTools.delete` — removed for a clean rebuild
- PASS `MaterialTools.create_material (UNVERIFIED)` — {"returnValue":{"refPath":"/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep"}}
- PASS `material domain/blend/shading (property NAMES unverified)` — wrote {"materialDomain": "MD_UI", "blendMode": "BLEND_Translucent", "shadingModel": "MSM_Unlit"}; read {"materialDomain": "MD_UI", "blendMode": "BLEND_Translucent", "shadingModel": "MSM_Unlit"}
- PASS `create UV (TextureCoordinate) UNVERIFIED` — 
- PASS `props UV` — wrote {"coordinateIndex": 0}; read {"coordinateIndex": 0}
- PASS `create Centre (Constant2Vector) UNVERIFIED` — 
- PASS `props Centre` — wrote {"r": 0.5, "g": 0.5}; read {"r": 0.5, "g": 0.5}
- PASS `create P (Subtract) UNVERIFIED` — 
- PASS `create R (Distance) UNVERIFIED` — 
- PASS `create Px (ComponentMask) UNVERIFIED` — 
- PASS `props Px` — wrote {"r": true, "g": false, "b": false, "a": false}; read {"r": true, "g": false, "b": false, "a": false}
- PASS `create Py (ComponentMask) UNVERIFIED` — 
- PASS `props Py` — wrote {"r": false, "g": true, "b": false, "a": false}; read {"r": false, "g": true, "b": false, "a": false}
- PASS `create MinusOne (Constant) UNVERIFIED` — 
- PASS `props MinusOne` — wrote {"r": -1.0}; read {"r": -1}
- PASS `create NegPy (Multiply) UNVERIFIED` — 
- PASS `create Ang (Arctangent2) UNVERIFIED` — 
- PASS `create Tau (Constant) UNVERIFIED` — 
- PASS `props Tau` — wrote {"r": 6.283185307179586}; read {"r": 6.2831854820251465}
- PASS `create Turns (Divide) UNVERIFIED` — 
- PASS `create T (Frac) UNVERIFIED` — 
- PASS `create Sweep (ScalarParameter) UNVERIFIED` — 
- PASS `props Sweep` — wrote {"parameterName": "Sweep", "defaultValue": 0.0}; read {"parameterName": "Sweep", "defaultValue": 0}
- PASS `create Thickness (ScalarParameter) UNVERIFIED` — 
- PASS `props Thickness` — wrote {"parameterName": "Thickness", "defaultValue": 0.0769}; read {"parameterName": "Thickness", "defaultValue": 0.07689999788999557}
- PASS `create Soft (ScalarParameter) UNVERIFIED` — 
- PASS `props Soft` — wrote {"parameterName": "EdgeSoftness", "defaultValue": 0.004}; read {"parameterName": "EdgeSoftness", "defaultValue": 0.004000000189989805}
- PASS `create Color (VectorParameter) UNVERIFIED` — 
- PASS `props Color` — wrote {"parameterName": "Color", "defaultValue": {"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0}}; read {"parameterName": "Color", "defaultValue": {"r": 1, "g": 1, "b": 1, "a": 1}}
- PASS `create One (Constant) UNVERIFIED` — 
- PASS `props One` — wrote {"r": 1.0}; read {"r": 1}
- PASS `create SoftPlus1 (Add) UNVERIFIED` — 
- PASS `create SweepAdj (Multiply) UNVERIFIED` — 
- PASS `create SweepMinusT (Subtract) UNVERIFIED` — 
- PASS `create AngRamp (Divide) UNVERIFIED` — 
- PASS `create AngMask (Clamp) UNVERIFIED` — 
- PASS `create Half (Constant) UNVERIFIED` — 
- PASS `props Half` — wrote {"r": 0.5}; read {"r": 0.5}
- PASS `create OuterGap (Subtract) UNVERIFIED` — 
- PASS `create OuterRamp (Divide) UNVERIFIED` — 
- PASS `create OuterMask (Clamp) UNVERIFIED` — 
- PASS `create InnerR (Subtract) UNVERIFIED` — 
- PASS `create InnerGap (Subtract) UNVERIFIED` — 
- PASS `create InnerRamp (Divide) UNVERIFIED` — 
- PASS `create InnerMask (Clamp) UNVERIFIED` — 
- PASS `create Ring (Multiply) UNVERIFIED` — 
- PASS `create Wedge (Multiply) UNVERIFIED` — 
- PASS `create ColorA (ComponentMask) UNVERIFIED` — 
- PASS `props ColorA` — wrote {"r": false, "g": false, "b": false, "a": true}; read {"r": false, "g": false, "b": false, "a": true}
- PASS `create Alpha (Multiply) UNVERIFIED` — 
- PASS `wire UV -> P.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Centre -> P.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire UV -> R.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Centre -> R.B (UNVERIFIED)` — {"returnValue":null}
- **FAILED** `wire P -> Px.Input (UNVERIFIED)` — **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- **FAILED** `wire P -> Py.Input (UNVERIFIED)` — **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- PASS `wire Py -> NegPy.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire MinusOne -> NegPy.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Px -> Ang.Y (UNVERIFIED)` — {"returnValue":null}
- PASS `wire NegPy -> Ang.X (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Ang -> Turns.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Tau -> Turns.B (UNVERIFIED)` — {"returnValue":null}
- **FAILED** `wire Turns -> T.Input (UNVERIFIED)` — **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- PASS `wire One -> SoftPlus1.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Soft -> SoftPlus1.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Sweep -> SweepAdj.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire SoftPlus1 -> SweepAdj.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire SweepAdj -> SweepMinusT.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire T -> SweepMinusT.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire SweepMinusT -> AngRamp.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Soft -> AngRamp.B (UNVERIFIED)` — {"returnValue":null}
- **FAILED** `wire AngRamp -> AngMask.Input (UNVERIFIED)` — **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- PASS `wire Half -> OuterGap.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire R -> OuterGap.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire OuterGap -> OuterRamp.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Soft -> OuterRamp.B (UNVERIFIED)` — {"returnValue":null}
- **FAILED** `wire OuterRamp -> OuterMask.Input (UNVERIFIED)` — **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- PASS `wire Half -> InnerR.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Thickness -> InnerR.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire R -> InnerGap.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire InnerR -> InnerGap.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire InnerGap -> InnerRamp.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Soft -> InnerRamp.B (UNVERIFIED)` — {"returnValue":null}
- **FAILED** `wire InnerRamp -> InnerMask.Input (UNVERIFIED)` — **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- PASS `wire OuterMask -> Ring.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire InnerMask -> Ring.B (UNVERIFIED)` — {"returnValue":null}
- PASS `wire Ring -> Wedge.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire AngMask -> Wedge.B (UNVERIFIED)` — {"returnValue":null}
- **FAILED** `wire Color -> ColorA.Input (UNVERIFIED)` — **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- PASS `wire Wedge -> Alpha.A (UNVERIFIED)` — {"returnValue":null}
- PASS `wire ColorA -> Alpha.B (UNVERIFIED)` — {"returnValue":null}
- **FAILED** `output EmissiveColor <- Color (UNVERIFIED)` — **FAILED** Function "connect_to_output", could not convert incoming function input params Json to a UStruct. Incoming fu
- **FAILED** `output Opacity <- Alpha (UNVERIFIED)` — **FAILED** Function "connect_to_output", could not convert incoming function input params Json to a UStruct. Incoming fu
- PASS `MaterialTools.recompile (UNVERIFIED)` — {"returnValue":null}
- PASS `save_assets` — {"returnValue":true}
- PASS `parameter 'Sweep' present on the saved asset` — read 'Sweep'
- PASS `parameter 'Thickness' present on the saved asset` — read 'Thickness'
- PASS `parameter 'EdgeSoftness' present on the saved asset` — read 'EdgeSoftness'
- PASS `parameter 'Color' present on the saved asset` — read 'Color'

## Findings
- **high** — wire P -> Px.Input (UNVERIFIED): **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- **high** — wire P -> Py.Input (UNVERIFIED): **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- **high** — wire Turns -> T.Input (UNVERIFIED): **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- **high** — wire AngRamp -> AngMask.Input (UNVERIFIED): **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- **high** — wire OuterRamp -> OuterMask.Input (UNVERIFIED): **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- **high** — wire InnerRamp -> InnerMask.Input (UNVERIFIED): **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- **high** — wire Color -> ColorA.Input (UNVERIFIED): **FAILED** Failed to connect output "" on <Object '/Game/UI/Materials/M_UI_RadialSweep.M_UI_RadialSweep:MaterialExpressi
- **high** — output EmissiveColor <- Color (UNVERIFIED): **FAILED** Function "connect_to_output", could not convert incoming function input params Json to a UStruct. Incoming fu
- **high** — output Opacity <- Alpha (UNVERIFIED): **FAILED** Function "connect_to_output", could not convert incoming function input params Json to a UStruct. Incoming fu

## Verdict
FAIL — see the failed calls above. 0 of 1 did not build.

## Rung honesty — what this PASS does not mean
- **Not a rung.** A compiled material is not a RENDERED one. Nothing here proves the sweep goes clockwise, starts at 12, or antialiases — that is a screenshot of `WBP_Screen_DeathRespawn` with `Sweep` stepped 0 / 0.25 / 0.5 / 1, and this generator cannot take it.
- **Does not prove the ring is the right THICKNESS.** `Thickness` defaults to a PROVISIONAL 0.0769: `figma_screen_layout.json` measures the 104px box and no stroke, and no respawn-ring art was ever exported. Filed as a contract_gap.
- **Not a multiplayer claim, not a PIE claim.** Law 6.
- **Unverified schema.** Every `MaterialTools` call was written without editor access. A PASS means the read-backs agreed, not that the calls were the right ones — run `--probe` and reconcile before trusting this receipt twice.
