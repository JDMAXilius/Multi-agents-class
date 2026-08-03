# TICKET — BP72: Verify the built assets, rebuild at digest, and author the twelve menu WBPs

> STATUS: open — cut 3 Aug 2026. **editor-live, and gated by BP71.** Two jobs that share one
> editor session: prove what the ten existing assets actually contain, and create the twelve
> that do not exist at all.

Founder directive: the main menu has **zero** of its twelve WBPs built, and the seven HUD assets
were last built against a plan digest that no longer matches the plan on disk. Until this runs,
"the HUD works" and "the menu exists" are both unprovable claims — and BP70's D1 has no artifact
either way.

**Ordering law:** BP71 green gates the claim (a WBP whose parent class does not compile cannot be
created). Within the ticket, step 1 gates step 2 — verify the old before overwriting it, because
**the delete-then-create build destroys the only evidence D1 could ever be read from.**

## Kickoff (machine-checkable)

- requires: **editor-live** — UE 5.8 editor OPEN on this project with its MCP reachable
  (`Tools/gen_ui/build_wbp.py` reports BLOCKED and exits 3 if it is not). R21: one editor,
  one driver, no overlapping build.
- BP71 is DONE (all boxes) — its Log records three green targets
- `python3 Tools/gen_ui/wbp_plan.py` prints `PLAN OK`
- `Config/DefaultGame.ini` carries `[/Script/Breachpoint.BRUISettings]` (landed `2a190df`)
- owner_path: `Content/UI/`, `docs/ui/receipts/`, `Tools/gen_ui/`,
  `docs/tickets/TICKET_BP72_WBP_BUILD_AND_VERIFY.md`

## Steps (in order)

1. **`python3 Tools/gen_ui/build_wbp.py --verify` FIRST, before anything writes.** This is the
   no-delete gate that landed in `0118b2e`, and this run is the **only chance** to read the
   ten on-disk assets as an earlier generation left them. Commit the receipt whatever it says.
   - `extra` names in that receipt **are** BP70 D1 — a widget in the asset the plan never
     created, which is exactly the shape of the duplicate ammo readout and of the killfeed
     double `04efb2a` already fixed once.
   - `missing` names mean the asset predates a plan node.
   - A clean pass is equally valuable: it retires D1 as *not reproducible in the assets*, and
     moves the founder render's blank rectangle onto the un-rebuilt-asset explanation.
2. **Prove the gate itself**, per BP70's Done-when: pick one asset, add a widget to it by hand
   in the editor, re-run `--verify`, and confirm it reports that widget as `extra`. Undo. That
   is the "proven once against a deliberately stale asset" box, and it has never been tickable
   before because the build path deleted its own evidence.
3. **Rebuild everything at the current digest**: `python3 Tools/gen_ui/build_wbp.py`. Commit the
   receipt. This closes D1 by construction for the seven HUD assets and creates the twelve menu
   assets in one pass — `WBP_MenuRow` has been planned and validating since `f65dfbd` and has
   never been built.
4. **Author the eleven remaining menu WBPs into the plan as they build.** `MCP-BUILD-PLANS.md`
   carries a per-asset tree, values, omissions and a packet prompt for each; work them in its
   §11 order (leaf-up: NavTab → ButtonPrompt → NavBar; RosterHeader → RosterRow → RosterPanel;
   FeatureCard → RecordPanel → LeftRail; ProfileBar; Screen_FrontEnd last). **The nine open
   DECIDE values are BP73's** — do not invent one to unblock a build; skip the node and log it.
5. **Point the config at what now exists**: `MenuRowWidgetClass` on `UBRScreen_FrontEnd`,
   `TabWidgetClass` on `UBRNavBar`, `RowWidgetClass` on `UBRRosterPanel`,
   `KillfeedEntryClass` in `BRUISettings`. Each is a soft class that resolves to null today.
6. **PIE and look at it** (`ui-presentation` §11 — a screen that was not rendered was not
   built). Two screenshots into this ticket: the HUD in the arena, and the main menu. This is
   the moment the HUD shows real numbers for the first time; BP70's remaining defects either
   reproduce here or they do not.

## Done when

- [ ] A `--verify` receipt exists for the ten pre-existing assets, committed **before** any rebuild
- [ ] The gate is proven against a deliberately stale asset (BP70's open box), recorded in both Logs
- [ ] A full rebuild receipt at the current plan digest is committed
- [ ] All twelve menu WBPs exist in `Content/UI/Components|Screens/` and appear in a receipt
- [ ] The four soft-class config paths resolve to real assets
- [ ] Two PIE screenshots in this ticket's Log; rung named honestly (**rung 2** at best — PIE
      is single-process)
- [ ] BP70 D1 and D2 are each either CLOSED with evidence or re-filed with what the render showed
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: **builder** owns the editor session (R21 — one driver); **ui-builder** consults on any
  plan edit; **verifier** reads the receipts rather than the editor.
- Binary files this ticket OWNS: every `.uasset` under `Content/UI/Components/`,
  `Content/UI/Screens/`, `Content/UI/Layouts/`, `Content/UI/HUD/`. Lock before editing.
- Out of scope: authoring art (D2's `T_UI_Weapon_Unknown` does not exist and is not this
  ticket's to draw), any rung-4 claim, and **hand-editing a WBP the generator owns** — if the
  plan is wrong, fix the plan and re-run, or the next rebuild silently reverts you.
- **The LFS caveat:** in a fresh cloud checkout every `Content/UI/*.uasset` is a pointer stub, so
  brush writes get SKIPPED with a loud line. On a real box with LFS pulled they land. If a
  receipt shows skipped brushes, check `git lfs pull` before believing the plan is at fault.

## Log

(append findings here, dated, newest last)
