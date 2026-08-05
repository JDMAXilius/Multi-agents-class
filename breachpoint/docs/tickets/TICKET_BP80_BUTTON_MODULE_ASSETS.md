# TICKET — BP80: Archive 27, delete 5, build 9 — the button module's editor half

> STATUS: in-progress — mac terminal 4 Aug 2026 (bbb2ceb). Step 1 (compile) only; steps 2–7
> need editor-live and the MCP is refused (no editor running).

> *(as cut, 4 Aug 2026)* — **files-only half COMPLETE and handed off.** editor-live, and
> gated by a compile. This ticket is everything left that needs redirectors, an editor, or eyes
> on a render. Nothing else can be done from a cloud container.
>
> **What landed before this ticket starts** (all pushed to main, all unverified by a compiler):
> | Commit | What |
> |---|---|
> | `7bef7fa` | `BRButton.h/.cpp` — `BRMenuRow` + `BRSettingsRow` + `BRButtonStyles` merged; 24 consumers repointed; `button()` factory; 5 plan entries deleted; 6 ini repoints |
> | `2078499` | `BRHighlightButton` merged too — button source is **one file pair**, 8 files → 2 |
> | `b0e50aa` | `build_wbp.py --parent / --filter / --list` — rebuild ONE family, exit 3 on no match |
> | `b79d6a8` | every button declares the same way (`shell + body`); `insert_after` replaced the `[:4]` z-order slice; 8/9 trees proven byte-identical; 4 dead helpers removed |
> | `7b4d44b` | **the node reduction was RETRACTED** — every chrome node is load-bearing |
> | *(this)* | ~50 stale doc/code pointers to the deleted headers repointed |

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
- **There is no node reduction, and the ledger's old "154 → 66" is RETRACTED.** It assumed a
  `FSlateBrush` RoundedBox could draw the plate, border and corners. It cannot: the side lines are
  0 × 20 **centred ticks** (a RoundedBox outline is a continuous rectangle), the bottom edge dims
  independently 0.3 → 1.0 (a RoundedBox outline is uniform), the plate is a **material target**
  for the measured 330 ms ease (a state brush swaps instantly), and `UBRHairlineBorder` is an
  `SLeafWidget` that cannot hold the tick. **`UBRHairlineBorder` exists because Slate brushes
  cannot draw this border** — replacing it with one undoes the reason it was written.
  **The nine build at 101 nodes. Do not "optimise" the chrome away in this session.**
- Why the moves could not be done from the cloud container, recorded so it is not re-asked:
  redirectors are editor-only, and every `.uasset` there is an LFS pointer stub — no analysis
  could enumerate in-content references.

## Log

**4 Aug 2026 — files-only half complete, handed to the terminal.**

Everything achievable without a compiler, an editor or LFS is done and pushed. The five commits
are in the STATUS block. Three things the terminal should know before starting:

1. **Nothing has been compiled.** Two merges (six files, then two more) and 24 consumer repoints
   were made blind. `PLAN OK` proves the plan and the header agree on every `BindWidget` name
   and type; it proves nothing about whether the module builds. Step 1 is the real gate.
2. **`WBP_ButtonDefault` will look different.** It used `with_plate_material(MENU_ROW_TREE)` and
   never `with_text()`, so alone among nine it shipped UMG's `"Text Block"` placeholder instead
   of the reference's `"BUTTON"` / `"SELECTION"`. The refactor made it uniform. That is the one
   asset whose design-time appearance changes — check it in step 5's render, do not assume it is
   a regression.
3. **Do not reduce the node counts.** The 154 → 66 figure in the ledger's v1/v2 was retracted on
   evidence; §4 there carries the per-node reasons. The nine build at **101 nodes**.

**4 Aug 2026 — STEP 1 COMPLETE. The blind merge compiles. Zero fixes needed.**

Ran `./Tools/run-ubt.sh` (all three targets) on the mac terminal at `c7a2fe7`.

| Target | Result | Evidence |
|---|---|---|
| `BreachpointEditor` | **PASS** (exit 0) | relinked `libUnrealEditor-Breachpoint.dylib`, mtime > start |
| `Breachpoint` | **PASS** (exit 0) | touched `CodeResources`, mtime > start |
| `BreachpointServer` | **FAIL** (exit 6) | `Server targets are not currently supported from this engine distribution` |

**None of the 24 repointed files needed a fix.** The `Done when` box asks which ones did: the
answer is none. Both hand edits flagged in step 1 were already correct — `BRTableRow.h:241`
(`ApplyRowType`, its own method, correctly left alone) and `BRModal_Options.cpp:134`
(`SetButtonType`, a real call into the merged class) both compiled. `UBRButton`,
`EBRButtonType` and `ButtonType` resolve everywhere. No conflict surfaced between `UBRButton`
and `UBRHighlightButton`, so the separation note in ## Notes stands untested but unviolated.

This was a real compile, not a no-op: `[1/5]`–`[4/5]` are the four unity blobs of the whole
`Breachpoint` module — every `.cpp` including `BRButton.cpp` — then the link. Rung 1, and only
rung 1: **it compiles; nothing here says it works.**

**Two environment findings the next session should not re-derive:**

1. **This machine is a LAUNCHER install of UE 5.8, not a source build**
   (`Engine/Build/SourceDistribution.txt` absent). Launcher ships no server binaries, so
   `BreachpointServer` **can never link here** — the failure is the distribution, not the code,
   and `run-ubt.sh` warns about exactly this before it runs. The `Done when` box "all three
   targets compile" is **not satisfiable on this machine**; it needs a source-built engine.
   Two of three is what this terminal can prove, and it is stated that way.
2. **`run-ubt.sh:86`'s "an Unreal editor is running" warning is a FALSE POSITIVE here.** Its
   `pgrep -f "UnrealEditor"` matches `UnrealEditorServices` — the macOS Finder/QuickLook helper
   (PID 888), always resident, not an editor. No editor was open; the link succeeded. Do not
   go hunting for an editor to close on this warning alone; check the PID first.

**Steps 2–7 NOT started — this session is not `editor-live`.** No editor process exists and the
Unreal MCP at `127.0.0.1:8000/mcp` refuses the connection (the server runs inside the editor
process). Per the CONTEXT gate the ticket stays in-progress at step 1; the archive, the delete,
the nine rebuilds, the renders and the PIE pass all wait for an open editor.
