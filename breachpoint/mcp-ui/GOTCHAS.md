# Gotchas

Thirteen things that cost real time on this project. Read before your first build.

---

## 1. The editor must be CLOSED to build C++ and OPEN to build assets

The running editor holds `libUnrealEditor-Breachpoint.dylib` open, so the link fails. Assets need
the editor for MCP. The loop is therefore:

```
close editor  ->  ./Tools/run-ubt.sh BreachpointEditor  ->  open editor  ->  build_wbp.py
```

Budget for it. A session that changes C++ and assets together pays this several times.

**Launch with an absolute path.** `UnrealEditor Breachpoint.uproject` resolves the relative path
against the *engine binaries* directory and dies with "Unable to find project file" after the log
already looks like it started.

## 2. A brushless `UImage` is a solid white rectangle

Not empty — white. An art-bearing image with no texture is worse than no image, because it looks
like a deliberate plate. Only add a `UImage` when (a) a texture exists, or (b) C++ tints it from
a token, in which case the engine's default white brush is the correct input.

## 3. Leaves have no desired size

`UBRHairlineBorder` draws lines at whatever geometry it is handed; its desired size is the stroke
weight. So `Overlay(hairline, tick)` collapses to the tick, and a 100×6 slider track collapses to
1px. Wherever Figma gives a box an explicit size and its contents cannot imply one, wrap it in a
`SIZEBOX` with `sized(w, h)`.

## 4. `BindWidget` resolves by NAME

The widget must be *named* exactly like the C++ member. This produces some odd tree names —
`InversionExempt` is the slider handle, named for the bind rather than the shape. Comment those,
or the next reader deletes them.

## 5. Design time is not runtime, and the designer is what people look at

`NativeOnInitialized` does not run at design time — only `NativePreConstruct` does. Anything the
idle look depends on must be applied in both, or every asset opens looking broken. This project
shipped nine assets that previewed as a solid white slab for exactly this reason.

The mirror image: anything design-time-only must be gated on `IsDesignTime()`. The 250px
component-board width is applied in the designer and cleared at runtime, so the row still fills
its real rail.

## 6. Set a property, then read it back, then compare

`set_properties` can report a refusal as *text inside a successful result*. A wrong camelCase
name fails silently. Two real examples from this project:

- `Style` written as a bare path string read back as `{"refPath": ...}` — the write succeeded,
  the comparison shape was wrong.
- `designSizeMode` read back `null` — the property is not exposed at all.

Without read-back, the first looks like a failure and the second looks like a success. Both are
wrong.

## 7. `save_assets` is not scoped

It flushes **every dirty package**, not just the one you created. A single import re-serialised
45 pre-existing `.uasset` files here. Check `git status` after any run that saves, and revert
what you did not intend to touch.

## 8. `OpenEditorForAsset` does not repaint an already-open docked tab

Screenshot straight after it and you capture the *previous* asset. Three separate capture passes
were discarded before this was diagnosed. Either close the asset-editor window between captures,
or verify the active tab before trusting the image. Structural read-back does not have this
problem, which is a good reason to prefer it.

## 9. `get_screenshot` composites the page background

An exported PNG's transparency cannot be trusted. A component with no fill returned an opaque
plate with an opaque share of 1.000. Use the render to measure *relative* things — where ink
stops, how contrast decays — not to judge alpha.

## 10. Preflight is calibrated for padded icons

`preflight_textures.py`'s "background is baked in" and "glyph is clipped" checks assume padding
around the art. An unpadded shape at its exact Figma box size fails both, legitimately — as does
every border export already in this repo. When the rasteriser starts from a transparent canvas, a
baked background is structurally impossible. Read the finding; do not obey the exit code blindly.

## 11. `guard_laws.py` only guards the file tools

It hooks `Edit|Write|MultiEdit|NotebookEdit`. A shell `rm`, `mv`, `cp` or `>` redirect walks
straight past it with no owner-path check. If you are relying on law 5 to confine an agent, know
that it confines only what that agent happens to do with the file tools.

---

## 12. An empty string is not the same as an absent one

Omitting a node's `properties` entirely makes UMG fall back to its `"Text Block"` placeholder.
Writing `{"text": ""}` gives you a genuinely blank widget. The settings slider shipped with
`Text Block` rendered verbatim beside every value because the plan simply left the key off —
invisible to the plan validator, invisible to the build receipt, invisible to the structural
audit, and obvious within two seconds of running it.

## 13. PIE input cannot be driven through the Slate inspector

The observer sees editor chrome only. The game viewport's UMG tree does not surface, so there is
no ref to click and `PressKey` goes to the focused *editor* widget — verified by diffing frames
before and after: zero pixels changed. You can **launch** PIE, **run console commands** (via the
editor's own Cmd box), and **screenshot** the result. You cannot hover a row or click a checkbox.
Interaction claims need a human at the keyboard.

---

## The meta-lesson

Every one of these was found by **checking rather than assuming** — reading a property back,
looking at a render, grepping for what actually references a path. The audit that found six
defects in the Menu Row set was a data audit, not a visual one: it read each built asset's live
tree and diffed it against the measurements. A screenshot would have caught maybe two of them,
and it lied about which asset it was showing.
