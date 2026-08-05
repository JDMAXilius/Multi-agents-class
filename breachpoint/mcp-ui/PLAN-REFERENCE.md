# `wbp_plan.py` reference

Everything the plan accepts. Extracted from the source, not remembered — `build_wbp.py` reads
exactly the keys listed here and ignores anything else, silently.

---

## Asset spec keys

```python
"WBP_MyThing": {
    "folder":         ASSET_FOLDER["WBP_MyThing"],   # required — /Game/... path
    "parent_class":   "/Script/Breachpoint.BRMyThing",  # required
    "class":          "UBRMyThing",                  # the C++ class, for the bind contract
    "header":         "Source/.../BRMyThing.h",      # parsed for BindWidget members
    "notes":          "one line, appears in the receipt",
    "tree":           [ ... ],                       # required — list of nodes
    "class_defaults": {"RowType": "Slider"},         # optional — written to the CDO
}
```

**`class_defaults` is the escape hatch for anything that lives on the class, not the tree.**
`RowType` and `Style` are properties of `UBRButton`, so no widget node can carry them. They are
written after the first compile (the CDO does not exist before that) and the asset is recompiled.

---

## Node keys

```python
{"name": "Label",              # required — MUST match the BindWidget member name exactly
 "class": TEXT,                # required — a /Script/... path, see constants below
 "parent": "TextFrame",        # required — None for the root
 "slot": box_slot(fill=1.0),   # slot properties, shape depends on the parent panel
 "bind": True,                 # assert this satisfies a BindWidget on the parent class
 "font": "Label/Button",       # a style name from figma_tokens.json
 "brush": brush(path, w, h),   # a texture that must exist on disk
 "properties": {"text": "BUTTON"}}   # widget properties, camelCase, written verbatim
```

`font` and `brush` are consumed through `art_properties(node)`. `properties` is the general
door — anything settable by `ObjectTools.set_properties` on that widget class.

**Property names are camelCase and are NOT guessable.** `ObjectTools.list_properties` against a
real instance is the only reliable source. `letterSpacing` lives *inside* the `font` struct, not
beside it. A wrong name fails silently, which is why every write is read back.

---

## Class constants

| Constant | Class |
|---|---|
| `OVERLAY` `CANVAS` `VBOX` `HBOX` | `UMG.Overlay` `CanvasPanel` `VerticalBox` `HorizontalBox` |
| `SIZEBOX` `SCALEBOX` `SAFEZONE` `INVALIDATION` | sizing / scaling wrappers |
| `TEXT` | `CommonUI.CommonTextBlock` — not `UMG.TextBlock` |
| `IMAGE` `PROGRESS` `SCROLLBAR` `TILEVIEW` `NAMED_SLOT` | stock UMG |
| `STACK` `ACTION_GLYPH` | `CommonActivatableWidgetStack`, `CommonActionWidget` |
| `HAIRLINE` `RULE` `SCRIM` | project leaves — `BRHairlineBorder`, `BRRule`, `BRScrim` |

`wbp_class("WBP_Other")` references another planned asset as a child class. Order matters: a
hosted asset must appear in `PLAN` **before** its host, and `validate_all()` enforces it.

---

## Slot helpers

```python
FILL                      # HAlign_Fill + VAlign_Fill, no padding
CENTER                    # HAlign_Center + VAlign_Center
margin(l, t, r, b)        # a padding struct
inset(2.0)                # uniform padding, Fill/Fill — Figma's "(2,2) 246x24 in 250x28"
box_slot(fill=None, padding=None, h=..., v=...)   # H/VerticalBox slot; fill=None is Auto
canvas_slot(left, top, w, h, anchor, align)       # CanvasPanel only
```

**`box_slot(fill=None)` is Auto (hug); a float is Fill at that weight.** Both `sizeRule` and
`value` are always written, because a partial struct write reads back as a mismatch rather than
as a helpful error.

---

## Tree helpers

| Helper | Use |
|---|---|
| `sized(w, h)` | `properties` for a SizeBox pinned to exact w×h. Writes the `bOverride_*` flags too — a value without its flag is a silent no-op. |
| `with_text(nodes)` | Stamps the Figma strings onto `Label`/`Selection`. Returns copies. |
| `without(nodes, *names)` | Drops nodes by name. Only legal for `BindWidgetOptional` members. |
| `unbound(nodes)` | Strips `bind` flags — for a subclass that INHERITS its binds. |
| `menu_row_shell(cls)` | The five shell nodes with `TextFrame`'s class left to the caller. |
| `dropdown_body()` `slider_body()` `checkbox_body()` | The three measured Menu Row bodies, shared by the buttons and the settings rows. |

### Why `unbound()` exists

`required_bind_widgets` parses **one header sliced to one class**. It cannot see a base class's
members. So `UBRSettingsRow`, which inherits every bind from `UBRButton`, would fail validation
if its nodes claimed `bind: True`. The widgets are still created by name and the engine still
resolves them at compile — only the plan's own assertion is dropped.

---

## What the plan CANNOT do

1. **Author a widget animation.** A `WidgetAnimation` is not a widget. `BindWidgetAnimOptional`
   members are left null and the C++ guards on null.
2. **Author a graph node.** `BindToEventProperty` is deliberately never called — it would add an
   event-graph node, which is the artifact R18/R26 forbid.
3. **Set `DesignSizeMode`.** It is `WITH_EDITORONLY_DATA` on `UUserWidget` with a bare
   `UPROPERTY()` — invisible to the details panel, to `set_properties` and to config. The only
   way in is a C++ constructor on your class.
4. **Move an asset after creation.** The rename modal auto-cancels under automation, so every
   asset is born at its final path.

---

## Validation

```bash
python3 mcp-ui/gen_ui/wbp_plan.py            # validate_all() over every entry
python3 mcp-ui/gen_ui/selftest_no_editor.py  # prove the logic with no engine
```

`validate()` checks: every required bind is satisfied; every bound node's class is compatible
with its declared type; parents exist and are panels; the root has no parent; hosted assets
precede hosts; brush targets exist on disk (a miss is a NOTE and a skipped brush, never a dead
asset reference).
