# TICKET — Author `InvertAnim` so the Menu Row plate fades instead of popping

> **ARCHIVED 25 Aug 2026** — moved off the live board, contents untouched below.
> SUPERSEDED — old-module (`Source/Breachpoint/`) UI polish. BreachpointNext owns the HUD; the live UI ticket is `TICKET_BN11_HUD_SLOTS`.
> Reversible: `git mv` kept the history, `git log --follow` still reaches it.
>
> STATUS: open — cut by lead, 4 Aug 2026. Needs a live editor and a human; no MCP toolset can
> author a widget animation. Everything else is already landed and waiting on this one asset edit.

Founder-directed. The hover plate is currently a step function: `Hover` goes 0 → 1 in one frame,
so a mouse sweep across a list strobes. `M_UI_MenuRowPlate` and the C++ handoff are both built and
pushed; the only missing piece is the animation itself, which UE reserves for the editor.

**Binding laws.** R18/R26 — this adds an ANIMATION to an existing WBP, not a graph node and not a
Blueprint class; zero event-graph nodes may appear. `AUTHORING-MATRIX` Tier 4 lists WBP "layout,
anchors and animation ONLY" as editor-authored, which is what makes this a legal hand edit.
Law 4 — no Tick; the animation is the tween, C++ only calls play.

**Ordering law:** none. Nothing gates this and it gates nothing. If it is never done, the plate
keeps snapping, which is the behaviour shipped today.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: editor-live
- `Content/UI/Materials/M_UI_MenuRowPlate.uasset` exists, and
  `python3 mcp-ui/gen_ui/material_plan.py` prints `PLAN OK`
- `WBP_ButtonDefault`'s `TextFrameFill` brush resourceObject is
  `/Game/UI/Materials/M_UI_MenuRowPlate.M_UI_MenuRowPlate` — verify with
  `ObjectTools.get_properties` on
  `/Game/UI/Components/Buttons/WBP_ButtonDefault.WBP_ButtonDefault:WidgetTree.TextFrameFill`
- `./Tools/run-ubt.sh BreachpointEditor` exits with `BreachpointEditor` PASS (the handoff in
  `UBRButton::ApplyPlateMaterialState` must be compiled in, or C++ will fight the animation)
- owner_path: `Content/UI/Components/Buttons/` — one asset, `WBP_ButtonDefault.uasset`

## Steps (in order)

1. **Open `WBP_ButtonDefault`.** Animations panel → **+ Animation** → rename to exactly
   **`InvertAnim`**. The name is the contract: `UBRButton` binds it as `BindWidgetAnimOptional`
   and `BindWidgetAnim` resolves by name. `Invert_Anim`, `InvertAnimation` or any other spelling
   binds nothing, fails silently, and leaves the plate snapping with no error anywhere.
2. **Set the animation's display rate to 30 fps.** `MOTION-INTERACTION.md` §1: 30 ms per frame,
   and every timing in this project snaps to that grid. At the default 60 fps the frame numbers
   below are wrong by half.
3. **Select `TextFrameFill`** in the hierarchy, then **+ Track → TextFrameFill**.
4. **On that track: + → Material Parameters → `Hover`.** UMG exposes a brush's material
   parameters as a `MovieSceneWidgetMaterialTrack`. If `Hover` is not offered, the brush is not
   carrying the material — go back to the kickoff check rather than keying something else.
5. **Key `Hover = 0` at frame 0 and `Hover = 1` at frame 3.** 3 frames × 30 ms = **90 ms**, which
   is `MOTION-INTERACTION.md` §4.4's hover value: *"colour-only… exists mainly to stop a mouse
   sweep from strobing."* Do not key `Pressed`; COMPONENT-SPECS §2 gives it the same plate.
6. **Compile and save.** Do not touch the widget tree, the `Style`, `RowType`, or any other
   property while in there — this ticket owns one animation and nothing else.
7. **Verify in PIE** (see Done when). `BR.ShowSettings` is not the surface for this —
   `WBP_ButtonDefault` is a standalone button asset with no host screen, so verification needs it
   placed somewhere it can be hovered, or a temporary host. Recording that as the real cost of
   this step rather than pretending it is a two-second check.

## Done when

- [ ] `WBP_ButtonDefault` contains an animation named exactly `InvertAnim`, 30 fps, with a
      `Hover` material-parameter track keyed 0 → 1 across frames 0 → 3
- [ ] The asset compiles with **zero** event-graph nodes (R26 unchanged — check the Graph tab is
      still empty)
- [ ] In PIE: hovering the row **fades** the plate in over ~90 ms rather than popping, and
      unhovering fades it out rather than sticking on
- [ ] No flicker on the first frame of hover — if the plate jumps to full and then animates from
      zero, the C++ handoff is not compiled in; re-run rung 1
- [ ] `python3 mcp-ui/gen_ui/build_wbp.py --asset WBP_ButtonDefault` is **NOT** run after this
      (it rebuilds from scratch and would delete the animation) — see Out of scope
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: a human in the editor. No agent can do step 1–6; all 23 MCP toolsets were checked and
  none creates a `UWidgetAnimation`, adds a track, or writes a key.
- Binary files this ticket OWNS (lock before editing):
  `Content/UI/Components/Buttons/WBP_ButtonDefault.uasset`
- **Out of scope, and this one is a trap:** re-running `build_wbp.py --asset WBP_ButtonDefault`.
  The generator DELETES the asset and rebuilds it from the plan, and the plan cannot author
  animations — so a regeneration silently destroys this work. Anyone changing `WBP_ButtonDefault`'s
  tree afterwards must re-key the animation. That coupling is the price of a hand-authored asset
  in a generated pipeline, and it is written down here so it is discovered on purpose.
- Also out of scope: rolling the material plate out to the other eight button assets. One asset
  opts in deliberately, so the tint path stays exercised and comparable.

## Log

(append findings here, dated, newest last — this is what the next session reads)

**4 Aug 2026 — cut.** Prerequisites landed in `9ab15b8` (material + opt-in + C++) and `74c139e`
(the `bAnimationDrivesHover` handoff). Procedure and its spec basis are in
`mcp-ui/MATERIALS.md` → "Animating a material parameter from a widget".

Two things recorded there worth repeating here. The motion table's nine entries do **not** include
the row inversion — §4.4 is where hover lives, because hover is folded into focus. And a
`WidgetAnimation` cannot be verified by read-back the way a property can: the generator does not
write it and no audit reads it, so the Done-when list is eyes-on by necessity, not by laziness.
