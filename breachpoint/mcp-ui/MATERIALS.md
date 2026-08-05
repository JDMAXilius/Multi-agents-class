# Materials — what to copy, and what not to

The UI material pipeline is the same plan/executor split as the widgets:
`gen_ui/material_plan.py` declares, `gen_ui/build_materials.py` executes over MCP,
`gen_ui/selftest_materials.py` proves the graph math with no engine.

```bash
python3 mcp-ui/gen_ui/material_plan.py                          # validate, no editor
python3 mcp-ui/gen_ui/selftest_materials.py                     # graph math, no editor
python3 mcp-ui/gen_ui/build_materials.py --asset M_UI_MyThing   # build, editor OPEN
python3 mcp-ui/gen_ui/build_materials.py --probe                # reconcile the MCP schema
```

---

## Read an existing material before authoring a new one

`MaterialTools` and `ObjectTools` will tell you everything about a material already on disk.
This is worth doing before writing a plan entry, because the answer is often "someone already
solved this".

```
ObjectTools.get_properties(mat, ["materialDomain","blendMode","shadingModel"])
MaterialTools.get_expressions(mat)          -> refPaths; the node TYPE is in the name
ObjectTools.get_properties(expr, ["parameterName","defaultValue","group"])
```

`get_expressions` returns bare `refPath`s like
`…M_SimpleGlow:MaterialExpressionLinearInterpolate_6`. The class is in the object name — strip
the trailing `_N` and the `MaterialExpression` prefix to classify a graph without opening it.

---

## Case study — `M_SimpleGlow`

78 expressions, 27 parameters, 14 distinct node types. A deliberate mega-material: one asset
serving many looks. Four ideas worth taking.

### 1. State belongs in the material, not only in C++

```
ScalarParameter  Hover    default 0     VectorParameter  HoverColor
ScalarParameter  Pressed  default 0     VectorParameter  PressedColor
```

…feeding **10 `LinearInterpolate` nodes**. Button state is driven by
`GetDynamicMaterial()->SetScalarParameterValue("Hover", t)` and blended on the GPU.

**Contrast with `UBRButton`.** `ApplyInvertedState` swaps colours on widgets from C++, so hover
is instant and binary — correct, and what COMPONENT-SPECS measures, but it cannot ease. A
material taking `Hover` as a 0..1 scalar can be eased, because the widget layer only has to move
a float. That is exactly what `M_UI_MenuRowPlate` now does — see "Animating a material parameter
from a widget" below for how the `WidgetAnimation` drives it and how the two avoid fighting.

### 2. `StaticSwitchParameter` compiles the unused branch OUT

`WithBorder`, `CircularGlow`, `Mask`, `AlphaBorder` are static switches. The dead branch is gone
at compile time, so one material covers many variants at **zero runtime cost**. That is the
right tool when variants differ by *whether an effect exists*; a scalar is right when they
differ by *how much*.

The cost, so it is a choice and not a habit: every switch doubles the shader permutations the
editor compiles, and a 27-knob material with four switches is genuinely hard to review.

### 3. Repeated parameter names are ONE parameter

`Background Falloff` appears on three expressions, `CircularGlow` on three, `Mask` on two. In
UE, expressions sharing a parameter name are the same parameter. That is how a single knob feeds
several points in a graph — it reads like duplication and is not.

### 4. It is `MD_UI` + `MSM_DefaultLit`

Shading model is largely ignored for the UI domain, so this is probably harmless rather than
wrong. `M_UI_RadialSweep` sets `MSM_Unlit`, which states the intent instead of leaving it to a
reader to know the domain overrides it. Prefer the explicit one.

---

## Two things from `M_SimpleGlow` NOT to copy

**Spaces in parameter names.** `"Background Falloff"`, `"Diagonal Gradient Amount"`. Legal, but
every call site becomes `SetScalarParameterValue(TEXT("Background Falloff"), …)` and a typo is
silent — the set simply does nothing.

`material_plan.py` has a gate for exactly this class of bug, and it is the reason the file
exists. A plan entry can bind a parameter to a C++ constant:

```python
"cpp_constants": {
    "Sweep": ("Source/Breachpoint/UI/Screens/BRScreen_DeathRespawn.cpp",
              "UBRScreen_DeathRespawn::RespawnRingSweepParameterName"),
},
```

`validate()` parses the `.cpp`, reads the string literal, and fails if it does not match the
parameter the plan declares. Rename either side and the build stops. **Use it for every
parameter C++ drives** — a name only a human compares is a name that will drift.

**Ungrouped parameters.** Only two of 27 carry a `group`. Set `group` and `sortPriority` on
every parameter; the details panel is the authoring surface and an ungrouped wall of 27 knobs is
unusable.

---

## The rung, for materials specifically

`build_materials.py`'s own receipt is blunt about its limits, and it is right:

- **A compiled material is not a rendered one.** Read-backs prove the parameters exist and the
  domain/blend landed. Nothing about a graph's *output* is proven by a successful build.
- **Many `MaterialTools` calls report `UNVERIFIED`** — they were written without editor access.
  A PASS means the read-backs agreed, not that the calls were the right ones. Run `--probe` and
  reconcile before trusting a receipt twice.

### How to actually see a material

The asset thumbnail renders at DEFAULT parameter values, which is often nothing. `M_UI_RadialSweep`
thumbnails as an empty checkerboard — correct, because `Sweep` defaults to 0 and the plan states
"NOTHING DRAWN". An empty thumbnail is not evidence of failure.

To see it, make a throwaway instance, set the parameter, capture, delete:

```
MaterialInstanceTools.create(folder_path, asset_name, parent)
MaterialInstanceTools.set_scalar_parameter(instance, name, value)   # note: `name`, not `parameter_name`
AssetTools.save_assets([path])
EditorAppToolset.CaptureAssetImage(assetPath)                       # materials DO support this; WBPs do NOT
AssetTools.delete(path)
```

That is how `M_UI_RadialSweep` was confirmed to sweep **clockwise from 12 o'clock**, ending at
~126° at `Sweep=0.35`, with antialiased edges — a claim the build receipt explicitly could not
make.

---

## Animating a material parameter from a widget

The scalar is only half of it. A `Hover` parameter that C++ snaps 0→1 is still a step function —
what makes it a *transition* is a `UWidgetAnimation` keying that scalar over time.

**No MCP toolset can author an animation.** All 23 toolsets were checked: nothing in `UMGToolSet`
or anywhere else creates a `UWidgetAnimation`, adds a track, or writes a key. This is Tier 4 hand
work by design — `BREACHPOINT-AUTHORING-MATRIX.md` lists WBP "layout, anchors and **animation**
only" as editor-authored. The generator builds the tree and the material; a human keys the curve.

### The C++ side is already done

`UBRButton` declares `InvertAnim` as `BindWidgetAnimOptional` and `ApplyInvertedState` plays it
forward on invert and reverse on release. Nothing needs writing for the animation to take effect —
it starts working the moment an animation with that exact name exists in the asset.

### The handoff, which is the part that bites

If C++ sets the scalar **and** the animation keys it, they fight: C++ snaps the plate to full,
then the animation restarts it from zero — one flicker frame on every hover, which reads as a
broken material rather than a race.

`ApplyPlateMaterialState(bInverted, bAnimationDrivesHover)` resolves it. When `InvertAnim` exists
and will play, C++ sets the brush tint and **does not touch the scalar**; the animation owns it.
When there is no animation, C++ sets it directly and the result is the binary snap. Both paths
reach the same two measured states.

### Authoring `InvertAnim` (editor, ~2 minutes)

Timing is not a choice — `MOTION-INTERACTION.md` §4.4 and its proposal table give hover
**90 ms [P]**, "colour-only… exists mainly to stop a mouse sweep from strobing", and §1 sets the
authoring display rate to **30 fps / 30 ms per frame**. 90 ms is exactly **3 frames**.

1. Open `WBP_ButtonDefault`. Animations panel → **+ Animation**, rename it **`InvertAnim`**
   (exact — `BindWidgetAnim` resolves by name, like every other bind).
2. Set the animation's display rate to **30 fps**.
3. Select `TextFrameFill` in the hierarchy, then **+ Track → TextFrameFill**.
4. On that track: **+ → Material Parameters** (UMG exposes brush material parameters as
   `MovieSceneWidgetMaterialTrack`) → **Hover**.
5. Key `Hover = 0` at frame 0 and `Hover = 1` at frame 3. Interpolation: the Standard curve
   §2.1 defines; linear is defensible for 3 frames and should be said in the ticket Log if used.
6. Compile and save.

Nothing else changes. C++ already plays it, and the handoff already stops the two from fighting.

### What to check afterwards

A `WidgetAnimation` cannot be verified by read-back the way a property can — the generator does
not write it and no audit reads it. This one is eyes-on: hover the row in PIE and confirm the
plate fades in rather than popping, and that leaving it fades out rather than sticking.
