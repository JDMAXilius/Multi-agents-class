# `docs/mcp-ui/` — building UI from Figma through MCP, repeatably

**What this folder is.** The complete method for turning a Figma design into a working UE 5.8
widget, without hand-placing anything in the editor. It was extracted from building the nine
Menu Row buttons against Figma `12:724`, and it is written so the same process runs for any
other component.

**What it is not.** It is not a description of the Menu Row. That lives in
`Content/UI/Components/Buttons/Assets/02-MenuRow.md`. This folder is the *method*.

---

## Read in this order

| File | What it answers |
|---|---|
| `PROCESS.md` | The five phases, with the exact commands. Start here. |
| `PLAN-REFERENCE.md` | Every key, helper and class constant `wbp_plan.py` accepts. |
| `GOTCHAS.md` | The eleven things that cost real time. Read before your first build, not after. |
| `WORKED-EXAMPLE.md` | Menu Row end to end, including the six defects the audit caught. |

---

## The one-paragraph version

A widget is declared as **data** in `Tools/gen_ui/wbp_plan.py` and built by
`Tools/gen_ui/build_wbp.py`, which drives the editor over MCP. The plan half imports no engine,
so a tree can be validated against its C++ `BindWidget` contract before an editor is ever
launched. The build half sets every property, reads it back, and compares — an unverified write
is not a write. Art that UMG genuinely cannot draw is generated from committed SVG by
`gen_menurow_art.py`, gated by `preflight_textures.py`, and imported by `import_textures.py`.
Nothing in the chain is hand-placed, so every asset is reproducible from a clean checkout.

## The pipeline at a glance

```
  Figma node
      |
      |  get_metadata / get_screenshot        <- MCP, no scripts
      v
  <COMPONENT>.md  (measured breakdown)
      |
      +--> art UMG cannot draw ---> gen_*_art.py -> svg_pillow.py -> PNG
      |                                  |
      |                          preflight_textures.py   (gate)
      |                                  |
      |                          import_textures.py      (editor OPEN)
      |                                  v
      |                             .uasset texture
      v
  wbp_plan.py    PLAN entry + ASSET_FOLDER entry
      |
      |  python3 Tools/gen_ui/wbp_plan.py     -> "PLAN OK"    (no editor)
      v
  build_wbp.py   --asset WBP_<Name>            (editor OPEN)
      |
      v
  .uasset widget  ->  read back, diff against the measured breakdown
```

## Which script owns what

| Script | Lines | Role | Editor |
|---|---|---|---|
| `wbp_plan.py` | 3,173 | Declares trees; parses the C++ bind contract; validates | no |
| `build_wbp.py` | 475 | Executes the plan over MCP; verifies every write | **yes** |
| `selftest_no_editor.py` | 232 | Proves plan logic with no engine at all | no |
| `gen_menurow_art.py` | 208 | Authors SVG for shapes with no texture | no |
| `svg_pillow.py` | 232 | Pillow-only SVG rasteriser (no native deps) | no |
| `preflight_textures.py` | 307 | Rejects bad textures before import | no |
| `import_textures.py` | 174 | Imports PNGs with the four settings that matter | **yes** |
| `check_transparency.py` | 94 | Catches baked-in backgrounds specifically | no |
| `figma_export.py` | 298 | Figma REST export when raw art is genuinely needed | no |

The same plan/executor split repeats for other domains — `material_plan.py`/`build_materials.py`,
`input_plan.py`/`build_input.py`, `arena_plan.py`/`build_arena.py`. Learn one, you know all four.

## Where the scripts live, and why they were not moved here

`Tools/gen_ui/`. They stay there because `build_wbp.py` imports `wbp_plan` by path, four other
scripts write receipts relative to the repo root, and roughly twenty documents cite those paths.
Moving executable code to document its own method would trade a working pipeline for a tidier
tree. This folder points at them instead.
