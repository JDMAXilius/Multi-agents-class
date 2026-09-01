# TICKET — BN42: the four WBPs and the FE map, built by script at the referee's boxes

> STATUS: open — cut 1 Sep 2026 by the cloud lead. OWNER: **terminal**, LIVE EDITOR.
> DEPENDS ON BN41 rung 1. requires: editor-live, unreal-mcp (`list_toolsets` first, the
> four failure rules apply). **Law 7: widgets land by committed Tools/bn/bn41_*.py
> scripts in the bn11 pattern (bn11_lib is the transport), never hand-placed.**
> Layout truth: `01-MENU-MEASURED.md` — if a number is not in the referee, re-read the
> node, do not eyeball it. The HUD was built this way and the founder called it perfect;
> same bar.

## Build — THE SCRIPTS ARE WRITTEN; you run and verify (cloud, 1 Sep, second pass)

The cloud transcribed the whole build into two committed scripts, so this ticket is now
RUN + VERIFY + FIX, not author:

1. **`python3 Tools/bn/bn41_frontend_wbps.py`** (live editor) — builds BOTH screens'
   WBPs delete-first at the referee's boxes, parented to the two BN classes, binds named
   to the C++ contract, verified writes printed for every property. `--verify` is the
   read-only on-disk-tree-vs-plan check; `--asset WBP_BNScreen_FrontEnd` narrows.
   Method transcribed from bn11_lib + mcp-ui/build_wbp (its three expensive lessons) +
   the R7 archive's geometry idiom (absolute 1280x720 canvas offsets; the DPI curve
   scales — nothing multiplied by 1.5).
2. **`python3 Tools/bn/bn41_fe_map.py`** via `-ExecutePythonScript` or the live console
   (BN21's lesson: `-run=pythonscript` cannot spawn actors) — builds
   `/Game/Maps/FE_MainMenu` with sun + sky and sets WorldSettings' DefaultGameMode to
   `BNFrontEndGameMode`. **One watch-list line inside**: the WorldSettings property write
   is unproven in this repo; the script attempts it, reads it back, and tells you exactly
   what to do by hand if it refuses. Do not silently hand-fix — log it.

**Known approximations to CHECK, not assume** (the cloud names its own thin ice):
- Row height 28 is achieved by content (font 14) + slot pitch, not a SizeBox — if rows
  render off-pitch, wrap each Button in a SizeBox h28 IN THE SCRIPT and re-run.
- `bIsEnabled`/`brushColor`/border `padding` camelCase keys are written-and-read-back but
  not previously proven in this repo — the transcript shows every readback; a `False`
  write with an unchanged readback is a wrong key, and the fix goes in the script.
- Panel grounds are flat tints; the referee's 88x4 notch language is BN43/M2 polish.
- CommonTextBlock defaults may want a style asset the project doesn't ship — if text
  renders with the engine default face, that is ACCEPTED for M1 (geometry over garb).

## Prove (the loop, not the look)

- [ ] PIE on FE_MainMenu: menu appears, mouse works, focus on PLAY
- [ ] PLAY → setup; Escape pops back; values cycle and print correctly
- [ ] START on Spillway/FFA/8 → match with YOU + 7 bots (scoreboard count = 8)
- [ ] START with TDM → two teams, 4v4, team colours in feeds
- [ ] Pause → LEAVE MATCH → back on the menu map with the menu up
- [ ] Repeat loop twice — the second lap catches the stale-subsystem class of bug
- [ ] Screenshots of both screens beside the Figma frames → founder

## Done when
- [ ] Both WBPs + the FE map landed by the two committed scripts; transcripts/receipts noted here
- [ ] The loop walks, twice, observations here
- [ ] Deviations from the referee listed with node ids (severity: blocks / cosmetic)

## Log
