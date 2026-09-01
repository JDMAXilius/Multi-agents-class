# TICKET — BN41: front-end C++ — compile it, then own it

> STATUS: open — cut 1 Sep 2026 by the cloud lead. OWNER: **terminal**.
> requires: engine-installed (rung 1) — no editor needed for step 1.
> Plan: docs/ui/ue-frontend/BN-FRONTEND-PLAN.md · Layout truth: 01-MENU-MEASURED.md.

The cloud landed the whole M1 front-end flow WRITTEN, NOT COMPILED:

| New | What |
|---|---|
| `UI/BNScreen_FrontEnd.h/.cpp` | main menu — PLAY / QUIT |
| `UI/BNScreen_PlaySetup.h/.cpp` | map/mode/bots cyclers + START (roster from ini) |
| `Match/BNFrontEndGameMode.h/.cpp` | spectator + menu push at PostLogin |
| edits | BNUIManager (2 config classes) · BNGameMode::InitGame (URL options) · DefaultGame.ini |

## Do

1. Rung 1: all three targets. Fix what a compiler finds; anything ARCHITECTURAL goes back
   to the cloud as a finding, not a rewrite.
2. Grep-check the two InitGame overrides do not fight the founder's ini values when
   ABSENT: launch BR_Spillway from PIE with no options — TargetPlayers must read 8 from
   ini, Teams from ini, byte-identical logs to yesterday.
3. Sanity: `AGameModeBase::PostLogin` fires for the standalone local player BEFORE world
   BeginPlay — the push relies on the controller existing there. If the menu does not
   appear on the FE map (BN42), THIS ordering is suspect #1; the fix is a deferred push
   on the next tick, and it goes in BNFrontEndGameMode with a comment, not in the manager.

## Done when
- [ ] Rung 1 clean ×3 targets, recorded here
- [ ] No-options launch byte-identical (log excerpt here)
- [ ] Findings (if any) filed for the cloud

## Log
