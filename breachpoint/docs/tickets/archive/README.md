# Ticket archive — what is in here, and why it left the board

`/tickets list` globs `docs/tickets/*.md`. Anything in this folder is **off the board on
purpose**. Nothing here was deleted: every file was moved with `git mv`, so its Log, its
findings and its authorship survive, and `git log --follow docs/tickets/archive/<file>` still
reaches every commit that touched it. **The sweep is reversible with one `git mv` back.**

## The 25 August 2026 sweep — sixteen BP tickets

The board carried sixteen `TICKET_BP*.md` from the **old module's** gameplay rework
(`docs/BREACHPOINT-GAMEPLAY-REWORK.md`, cut 7 Aug). It also carried two live BN tickets. A
board where fourteen of eighteen entries are open tickets nobody intends to work is a board
that has to be re-read and re-dismissed every session, which is how R10's blocker went two
days undetected.

**The dates settle it.** `Source/BreachpointNext/` was created **12 Aug**. The old module's
source was last touched **14 Aug**. `TICKET_BP91_FOUNDATION` reads PAUSED **14 Aug, mid-step-5**.
Everything BP91–BP102 describes building in `Source/Breachpoint/` — foundation, input, the GAS
spine, damage, match host, character, equipment, fire, match frame, movement/melee, projectile,
grapple — `Source/BreachpointNext/` has since built and shipped under R1–R10. They are
superseded, not abandoned mid-flight.

BP79 / BP81 / BP82 are old-module UI and animation **asset** work, superseded by BN's own HUD
and icon paths.

Each archived file carries a dated `ARCHIVED` banner above its original STATUS line naming its
own reason. The banner is the only edit; the ticket body below it is untouched.

## What this sweep does NOT do

- **It does not delete or deprecate `Source/Breachpoint/`.** Both modules are still in
  `Breachpoint.uproject` and both still compile. Retiring the old module is a separate
  decision the founder has not made, and archiving its tickets does not make it.
- **It does not throw away BP80's work.** `TICKET_BP80_BUTTON_MODULE_ASSETS` reached step 6:
  it produced `Source/Breachpoint/UI/Components/BRButton.{h,cpp}` and
  `Content/UI/Widgets/Buttons/WBP_Button_Default.uasset`, **both of which exist on disk today**
  and are exactly the button atom `TICKET_BN11_HUD_SLOTS` wants for the pause row. Its four
  unticked boxes are blocked on a **source-built engine**, which the working machine does not
  have — that is an environment fact, not a task waiting for a volunteer. Recorded here so the
  next person looking for a button finds it instead of building a second one.

## Older residents

`TICKET_BP90_DEMOLITION.md` and `TICKET_BP82_UI_SOURCE_RUNG1_UNBLOCK.md` were archived before
this sweep, under the same convention.

## The live board

`docs/tickets/` now holds exactly: `HANDOFF.md`, `TICKET_TEMPLATE.md`, and the open BN tickets.
BN tickets are cut as `TICKET_BN<n>_<NAME>.md` and carry a `requires:` line
(`files-only` / `engine-installed` / `editor-live`) so the board says up front whether a
session can even take the work. See the 24 Aug update in `HANDOFF.md`.
