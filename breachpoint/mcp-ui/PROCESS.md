# The five phases

Every widget in this project goes through these in order. Skipping phase 0 is how a "1:1" rebuild
turns out to be 80% right in ways nobody can name.

---

## Phase 0 — Measure. No scripts.

**Never eyeball a screenshot when the metadata is available.** The Figma MCP gives you exact
geometry; a screenshot gives you an impression.

```
get_metadata(fileKey, nodeId)      # the node tree: names, x, y, width, height
get_screenshot(fileKey, nodeId)    # what it actually looks like
```

`get_metadata` with no `nodeId` lists top-level pages. With one, it returns the XML tree — for a
whole page that can exceed 100k tokens, so drill to a component set, not a page.

**Write the numbers down in a markdown file next to the assets**, e.g.
`Content/UI/Components/Buttons/Assets/02-MenuRow.md`. That file becomes the thing your plan cites
and your audit diffs against. Without it, "1:1" has no referee.

Record for every element: box size, position, auto-layout direction, gap, padding, fills,
strokes, font family/weight/size, and **which states change which properties**.

### Two traps in phase 0

**`get_screenshot` composites the page background into the PNG.** Verified on this project: a
component with no fill came back with an opaque `#F5F5F5` plate and an opaque share of 1.000.
Never trust an exported PNG's transparency; `check_transparency.py` exists for exactly this.

**A node's own reported size can disagree with its children.** The Menu Row hatch frame reported
143 wide while its 69 children spanned 204. Neither was trusted — the answer came from measuring
the *rendered* image: per-column standard deviation decayed to zero at exactly x=110, so 110 it
is. When two pieces of metadata disagree, measure the render.

---

## Phase 1 — Art, and only for what UMG genuinely cannot draw

**The rule: export nothing UMG can draw.** Plates, borders, rules, ticks, rectangles, text — all
drawn natively. Out of the 50 variants on the Menu Row page, exactly three things needed art.

If you do need a shape, author it as SVG and rasterise:

```bash
python3 mcp-ui/gen_ui/gen_menurow_art.py     # your own gen_<component>_art.py
```

Model it on `gen_menurow_art.py`: a list of `(stem, svg_text, w, h, post_processor)` and a loop.
It calls `svg_pillow.render(path, w, h)`, which supports **absolute `M`/`L`/`H`/`V`/`Z` only** —
no curves, no transforms, no `<circle>`. That is why the slider handle is a 24-gon: at 6 px it is
indistinguishable from a circle and, unlike a curve, it actually renders.

Then gate and import:

```bash
python3 mcp-ui/gen_ui/preflight_textures.py <dir>
python3 mcp-ui/gen_ui/import_textures.py <src_dir> /Game/UI/<Target>     # editor OPEN
```

`import_textures.py` applies the four settings that matter — `TEXTUREGROUP_UI`,
`TC_EditorIcon`, `TMGS_NoMipmaps`, sRGB — and verifies each by read-back. Defaults are wrong for
UI and the failure is subtle: blurry icons and a texture budget three times bigger than needed.

**Read `preflight_textures.py`'s output, do not just obey its exit code.** Its corner-alpha and
edge-pixel checks assume a *padded icon*. An unpadded shape at its exact Figma box size fails
them legitimately — every one of this project's existing border exports fails identically. Judge
the finding, then decide.

---

## Phase 2 — C++ first, then declare the tree

**The class comes before the asset.** The widget's behaviour, state and bindings are C++; the
asset is layout only. Write the class, declare its `BindWidget` members, and build it:

```bash
./Tools/run-ubt.sh BreachpointEditor          # editor must be CLOSED
```

Then add two things to `mcp-ui/gen_ui/wbp_plan.py`:

```python
ASSET_FOLDER = {
    "WBP_MyThing": UI_COMPONENTS,             # where it is born
}

PLAN = {
    "WBP_MyThing": {
        "folder": ASSET_FOLDER["WBP_MyThing"],
        "parent_class": "/Script/Breachpoint.BRMyThing",
        "class": "UBRMyThing",
        "header": "Source/Breachpoint/UI/Components/BRMyThing.h",
        "notes": "one line a reviewer can check the asset against",
        "tree": [ ... ],
    },
}
```

Validate with no editor:

```bash
python3 mcp-ui/gen_ui/wbp_plan.py              # -> "PLAN OK"
```

This parses the header, extracts every `BindWidget`/`BindWidgetOptional`, and checks your tree
against it. A name typed differently than the header is a plan error here, not a runtime mystery
later. It also checks every brush target exists on disk and reports missing ones as NOTE lines.

---

## Phase 3 — Build

```bash
python3 mcp-ui/gen_ui/build_wbp.py --asset WBP_MyThing     # editor OPEN, MCP up
python3 mcp-ui/gen_ui/build_wbp.py                          # every asset in PLAN
python3 mcp-ui/gen_ui/build_wbp.py --dry-run                # no writes
```

What it does per asset: delete any existing asset (explicitly — the clobber guard is
`FindPackage`, which checks *memory*, not disk), create, strip UMG's default root, add each node
in plan order, write slot properties, write art, write node properties, compile, write class
defaults, recompile, save, then read the tree back and compare against the plan.

Every write goes through `write_verified` — set, get, compare. It writes a receipt to
`docs/ui/receipts/` line-buffered, so a run that dies leaves a record of where.

---

## Phase 4 — Verify, and be honest about the rung

Structural verification is cheap and worth doing every time: read the built tree back over MCP
and diff it against the phase-0 breakdown. Check widget membership, exact text, class defaults,
and any measured size.

```
UMGToolSet.GetWidgets                    -> the real tree
ObjectTools.get_properties               -> text, sizes, CDO values
EditorAppToolset / SlateInspector        -> a render
```

**A compiled WBP is not a rendered one, and a rendered one is not a working one.** State which
you have:

| Claim | What backs it |
|---|---|
| "builds" | UBT exit 0 and a relinked binary |
| "matches the plan" | build receipt, every write read back |
| "matches the measurements" | tree read back and diffed against the breakdown |
| "renders correctly" | a capture, looked at by a human |
| "works" | exercised in PIE |
| "works in multiplayer" | two processes, asserted in threes |

The Menu Row buttons reached "matches the measurements" for all nine and "renders correctly" for
six. They have never reached "works" — nothing instances them at runtime yet, and the write-up
says so rather than rounding up.
