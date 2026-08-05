# TICKET — BP80: Archive 27, delete 5, build 9 — the button module's editor half

> STATUS: open — cut 4 Aug 2026. **editor-live, and gated by a compile.** The files-only half
> already landed (`BRButton.h/.cpp`, the `button()` factory, the six ini repoints). This ticket
> is everything that needs redirectors, an editor, or eyes on a render.

Founder directive, 4 Aug 2026: one modular button class with the type as data; the sourced
third-party pack and our 4 Aug button WBPs are **archived, not deleted**; everything else in the
button family goes. `WBP_ButtonCheckbox` is the reference case — it is rebuilt first and it is
what "identical" gets measured against.

The full move/delete/keep ledger with per-file counts is
`docs/ui/ue-frontend/BUTTON-MODULE-LEDGER.md`. This ticket executes it.

**Ordering law:** step 1 gates everything (a WBP cannot be created against a class that does not
compile). Step 5 gates step 6 — **the art delete is the only irreversible step and it happens
last, after a render proves the replacement.**

## Kickoff (machine-checkable)

- requires: **engine-installed** for step 1, then **editor-live** with the Unreal MCP reachable
  (`mcp-ui/gen_ui/build_wbp.py` reports BLOCKED and exits 3 if it is not). R21: one editor,
  one driver.
- `Source/Breachpoint/UI/Components/BRButton.h` exists and declares **every button class in the
  project**: `UBRButton`, `UBRSettingsRow`, `UBRHighlightButton`, two enums and four
  `UBRButtonStyle_*` — 774 lines, with `BRButton.cpp` at 903
- `python3 mcp-ui/gen_ui/wbp_plan.py` prints `PLAN OK` — verified on the files-only commit, and
  that pass already checks every `BindWidget` name in all nine trees against the merged header
- `git lfs pull` has run — every `.uasset` in a fresh clone is a pointer stub
- owner_path: `Content/UI/`, `Config/DefaultGame.ini`, `docs/ui/receipts/`,
  `docs/tickets/TICKET_BP80_BUTTON_MODULE_ASSETS.md`

## Steps (in order)

1. **Compile the merge.** `BRMenuRow`/`BRSettingsRow`/`BRButtonStyles`/`BRHighlightButton` were
   merged into `BRButton.h/.cpp` and **eight files deleted**; the class renamed `UBRMenuRow` → `UBRButton`, the
   enum `EBRMenuRowType` → `EBRButtonType`, the property `RowType` → `ButtonType`. **Twenty-four
   files referenced the old names** and were repointed blind, without a compiler. Expect the
   errors to cluster in `BRScreen_FrontEnd`, `BRModal_Options`, `BRScreen_Settings` and
   `BRLeftRail` — the four with real code references rather than doc mentions.
   - Two edits were made by hand after a bad global rename and are the first things to check if
     something looks wrong: `BRTableRow.h:241` (`ApplyRowType` — **its own** method, wrongly
     renamed and reverted) and `BRModal_Options.cpp:134` (`SetRowType` → `SetButtonType`, a real
     call site into the merged class).
2. **Archive 17 sourced assets → `Content/UI/Reference/Buttons/`.** Use the editor's Move so
   redirectors are created. **This is a NEW tracked folder — NOT the existing
   `Content/Reference/`, which is gitignored (`.gitignore:42`) and would silently untrack all
   seventeen.** Founder confirmed the tracked path on 4 Aug.
   - The pack: `W_AbilityCooldownButton` `W_AbilityReady` `W_ButtonChangeSelection` `W_CheckBox`
     `W_ConfiguratorButton` `W_DialogPrompt` `W_Dropdown` `W_EditableText` `W_EscMenu`
     `W_GlassRectangleButton` `W_GlassSquareButton` `W_Icon` `W_IconButtons` `W_PlayButton`
     `W_ProgressBar` `W_RoboButton` `W_TypeText`
3. **Archive our 10 button WBPs → `Content/UI/OldWidgets/Buttons/`.** They are correct and
   audited 8/8 (`73d4d85`); they are simply built on the pre-modular shape. **They are the
   visual reference for step 5** — do not delete them until the founder signs off on the rebuild.
4. **Delete 5 assets** — `WBP_MenuRow`, `WBP_SettingsRow`, `WBP_SettingsRow_{Checkbox,DropDown,Slider}`.
   Their trees were byte-identical (or one `Selection` node apart) from their `Buttons/` twins.
   **The six ini refs that pointed at them were already repointed in the files-only commit** —
   verify they resolve rather than repointing again.
5. **Build the nine — and ONLY the nine.**
   ```
   python3 mcp-ui/gen_ui/build_wbp.py --list   --parent BRButton   # confirm the selection
   python3 mcp-ui/gen_ui/build_wbp.py --verify --parent BRButton   # read the OLD assets first
   python3 mcp-ui/gen_ui/build_wbp.py          --parent BRButton   # rebuild
   ```
   **Never run it unflagged here.** With no selector it deletes and recreates all forty-six,
   which rebuilds the HUD and the front end at a plan digest this ticket never reviewed — and
   the receipt still says PASS, because every asset matches the plan it was just built from.
   `--parent` selects by the C++ contract, so it cannot drift from what the module is.
   Commit each receipt. All nine come from the `button()` factory, so `parent_class` appears
   exactly once in the plan.
   Then **render `WBP_ButtonCheckbox` beside its archived twin** and compare. That comparison is
   this ticket's real deliverable.
6. **Only now, the art.** Delete `Assets/Sides/` (40 textures, 112 files), `ButtonBorder_*`
   (6 sets, 18 files, **referenced zero times**) and `MenuRow_Tick` (duplicate of
   `Icons/Glyphs/T_UI_Glyph_Check_24`). **Gate:** the 4 currently-referenced `Sides/` textures get
   an eyes-on comparison first — they carry `Fade` variants and `Tab` shapes a plain RoundedBox
   outline may not express. If the outline cannot reproduce them, keep those four and log it;
   the other 43 still go.
7. **PIE and exercise it.** The 4 Aug build shipped with hover/press/click **never tested** —
   "these components have no host screen yet." The options modal and the settings screen both
   build rows from `WBP_ButtonDefault` now, so a real host exists. Click a checkbox and watch
   the mark.

## Done when

- [ ] All three targets compile; the Log names which of the 24 repointed files needed a fix
- [ ] 17 sourced assets in `Content/UI/Reference/Buttons/`, **tracked by git** (`git status`
      shows them moved, not deleted)
- [ ] 10 archived in `Content/UI/OldWidgets/Buttons/`
- [ ] 5 deleted; the six `DefaultGame.ini` soft refs each resolve to a real asset
- [ ] A build receipt for the nine at the current plan digest is committed
- [ ] **Two screenshots in this Log: the new `WBP_ButtonCheckbox` and its archived twin**
- [ ] The art delete is done, or the exceptions are named with the reason
- [ ] Hover/press/click exercised in PIE — **rung 2 at best**, and said that way
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: **builder** owns the editor session (R21 — one driver); **ui-builder** consults on any
  plan edit; **verifier** reads the receipts.
- Binary files this ticket OWNS: everything under `Content/UI/Components/Buttons/`,
  `Content/UI/Reference/Buttons/`, `Content/UI/OldWidgets/Buttons/`, and the five named assets
  in `Content/UI/Components/`. Lock before editing.
- Out of scope: `UBRHairlineBorder` (12 includers, 11 not buttons), `BRComponentTokens.h`
  (23 consumers), `BRUITokens.h`/`BRTextStyles.h` — none is button source; they are the drawing
  layer and the design system buttons CONSUME. Also out: the chrome-via-style-brush rewrite
  (a separate C++ packet — this ticket ships the merge as-is, not the node reduction).
- **`UBRHighlightButton` DID merge (4 Aug), and the merge is file-only.** Its header's warning —
  *"two components, two rules — do not unify them on the assumption that 'inversion' means one
  thing"* — is about BEHAVIOUR and still binds: a menu row inverts to white, a highlight button
  fills with a per-type accent, and `EBRHighlightButtonType` is deliberately not `EBRButtonType`.
  Nothing was folded into `UBRButton`. If the compile surfaces a conflict between the two, the
  fix is to separate them again, never to unify the rules.
- **The node reduction is NOT in this ticket.** The ledger's 154 → 66 assumes state brushes draw
  the plate, border and corners. That is a `UBRButton` change plus a plan change, and doing it in
  the same editor session as an archive + rebuild would make a failure unattributable. **Build
  the nine at their current node counts first; reduce second.**
- Why the moves could not be done from the cloud container, recorded so it is not re-asked:
  redirectors are editor-only, and every `.uasset` there is an LFS pointer stub — no analysis
  could enumerate in-content references.

## Log

(append findings here, dated, newest last)
