# Worked example — the Menu Row buttons

Figma `12:724` → nine widget assets, 4 Aug 2026. Everything below actually happened, including
the parts that went wrong; the failures are the useful half.

---

## What was being built

`12:724` is a component set of **27 variants on three axes** — Status × Alignment × Type. Ten
Types: Default, Disabled, Drop Down, Dig Down, Icon Only, Slider, Checkbox, Radio, Map Voting,
Image.

The C++ class already existed and already modelled it: `UBRMenuRow` collapses the 27 onto three
axes, where Type is `EBRMenuRowType`, Status is CommonUI's own button state, and Alignment is
`EBRMenuRowAlignment`. **The gap was the assets, not the design.**

---

## Phase 0 — measurement

`get_metadata` on `12:724` returned the 27 symbols with their boxes. Drilling into
`12:894` (Dig Down) gave the hatch construction: 69 `<line>` children, each with a **36×36
bounding box** — equal sides *is* the 45°, and 36·√2 = 50.9 is the "51 long" in the notes — at a
horizontal pitch of exactly 3.

Result: `Content/UI/Components/Buttons/Assets/02-MenuRow.md`, which every later step cites.

### The measurement that needed a render

The hatch frame reported 143 wide; its children spanned 204. Neither matched the 246-wide plate.
Rather than pick one, the *rendered* node was measured: per-column standard deviation over the
text-free rows was 20.4 at x=10, decayed monotonically, hit 0.47 by x=110, and was **exactly 0.00
for every column out to 240**.

So the hatch is 110 wide — precisely `Rectangle 154`'s box — and fades left-to-right. The first
implementation had it 246 wide fading the wrong way. Both errors were visible the moment the PNG
was actually looked at.

---

## Phase 1 — five textures

Out of 50 variants on the page, **three things** needed art. For Menu Row: the diagonal hatch,
the 6×6 disclosure triangle, the Dig Down chevrons, the slider handle, the slider tick.

`gen_menurow_art.py` authors each as SVG and rasterises through `svg_pillow.py`. The hatch bakes
two things into alpha — the `Highlight` frame's own 0.5 opacity and the `Rectangle 154` gradient
— because the generator writes brushes and never colours, and `UBRMenuRow` has no hatch bind to
drive a tint from.

The Dig Down arrows were built from **metadata alone**: `get_screenshot` on `12:972` returned a
9×7 PNG with alpha 255 everywhere — the page background composited over a glyph with no fill.

---

## Phase 2–3 — plan and build

Eight `PLAN` entries, all `parent_class: /Script/Breachpoint.BRMenuRow`. Parenting to the real
class is what makes them *work* rather than merely look right: hover, press, select, disabled and
gamepad focus all arrive from `UCommonButtonBase`. An orphan WBP would have had to re-implement
every one of them in a graph it is not allowed to have.

`Disabled` is deliberately not an asset. It is a Status, not a body — `NativeOnDisabled` dims the
row to 0.5 and changes no geometry, so the asset would be a byte-identical copy.

---

## Phase 4 — the audit, and the six defects

The first build reported PASS on all eight. **PASS meant "matches the plan", not "matches
Figma".** A data audit — reading each built asset's live tree and diffing it against
`02-MenuRow.md` — found six real defects:

| # | Defect | Why it happened |
|---|---|---|
| 1 | `Selection` present on four Types that have none | Reused the shared tree without checking which variants carry it |
| 2 | Icon Only had `BUTTON` + `SELECTION` | It is a 40×40 shell with one icon and no text node at all |
| 3 | Every label read `Text Block` | UMG's placeholder — the plan never set the strings |
| 4 | Checkbox tick and radio fill painted permanently | Idle/Hover are an EMPTY box; only Active carries the mark |
| 5 | The slider was three objects in a row | The handle sits ON the track and the tick 6px above it |
| 6 | Map Voting's `SELECTION` right-justified | There it is the second line of `Text Stacked`, not a right-edge value |

Four needed C++, because the asset had nowhere to put the behaviour: `TypeBody` +
`ApplyInversionToSubtree` (per-type bodies were staying white on the white hover plate, i.e.
vanishing exactly when focused), `TypeCheckMark` (driven by selection, *not* by hover — pointing
at an unchecked box must not make it look checked), `InversionExempt`, and a `RowType`-aware
justification.

### The fix that mattered most

Defect 3's first fix silently missed two assets. `with_text()` wraps the shared shell — but Map
Voting and Image declare their own `Label` in an appended list, *outside* that shell. The audit
caught it; a screenshot would not have, because those two captures were showing the wrong tab.

**Final state: 8/8 against the measurements.**

---

## What the same process later found in shipped code

Wiring the buttons into the settings screen surfaced a live defect nobody had noticed:
`UBRSettingsRow::RefreshFromSetting` had always resolved a Scalar setting to `Slider` and a
two-option Discrete to `Checkbox` — and then rendered both as a plain label-and-value row, because
`WBP_SettingsRow` has no per-type body and `SetRowType` only drives height.

**A volume slider and a resolution dropdown were pixel-identical.** The type was computed and
thrown away. Three assets sharing the same measured bodies closed it.

That is the argument for this whole pipeline in one paragraph: the measurement is what makes the
defect *visible*. Nobody was going to notice by looking.

---

## Honest final state

| Claim | Status |
|---|---|
| Nine assets build and compile | yes — build receipts |
| Match the measurements | yes — 8/8 data audit, read back from the editor |
| Render correctly | six of eight, captured and looked at |
| Work at runtime | **partly** — see below |
| Multiplayer | not applicable, not claimed |

Sounds are unwired: `UBRButtonStyle_MenuRow` carries `PressedSlateSound` / `ClickedSlateSound` /
`HoveredSlateSound`, and the project has no UI audio at all. Nothing was invented to fill it.

---

## The PIE run, and the defect only it could find

`BR.ShowSettings` in a PIE session on `BR_Arena01`, driven through the editor's Cmd box. The
settings screen pushed, built its rows, and **rendered all three typed bodies correctly** —
Mouse Sensitivity and Field of View as sliders with a handle on the track, Invert Vertical Look
and Damage Numbers as checkboxes, Colour Blind Mode with a dropdown caret. That is the shipped
defect fixed and observed, not inferred.

**It also immediately exposed a defect nothing else had caught.** Every settings slider rendered
`Text Block` beside its value. The cause: `slider_body(static_value=False)` omitted the
`properties` key rather than writing an empty string, so UMG fell back to its placeholder. The
plan validated, the build receipt passed, the structural audit passed — because all three check
that what you asked for was written, and nobody had asked for the right thing. Two seconds of
running it was worth all of them.

**What the PIE run could NOT establish:** hover inversion and click-to-toggle. The Slate
inspector does not surface the game viewport's widget tree, so there is no widget to click, and
`PressKey` reaches the editor rather than the game — verified by diffing frames, zero pixels
changed. Those two claims still need a human at the keyboard.
