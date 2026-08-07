# TICKET — The ASC host: BRPlayerState and BRPlayerController

> STATUS: open — cut 7 Aug 2026. Blocked on BP93 DONE. Gates BP96.

Founder directive: the ASC lives on the PlayerState, and that single decision is what makes
respawn free — attributes, granted abilities and score survive the pawn by construction, not
by copying. The controller is the input relay and the boundary UI is allowed to talk to;
nothing above it reaches past it into abilities or attributes.

**Ordering law:** `BRPlayerState` lands before `BRPlayerController` (the controller resolves
its ASC through the PlayerState).

## Kickoff (machine-checkable)

- requires: engine-installed
- BP93 DONE — `UBRAbilitySystemComponent`, `UBRAttributeSet` exist and their specs are green
- BP92 DONE — `UBRInputComponent` forwards tags to a stub this ticket takes over
- owner_path: `Source/Breachpoint/Match/`

## Steps (in order)

1. **[netcode-builder]** `Match/BRPlayerState.h/.cpp`:
   - Constructs and owns `UBRAbilitySystemComponent` + `UBRAttributeSet`; implements
     `IAbilitySystemInterface`
   - **`SetNetUpdateFrequency(100.f)`** — the 1 Hz default is unusable for an FPS scoreboard
     and unacceptable for an ASC host. State the number in the Log; it is a tuning value that
     a later packet may move loudly.
   - Team as `FGenericTeamId` + `IGenericTeamAgentInterface` (`GetGenericTeamId` /
     `SetGenericTeamId`) so `BRTeams::GetAttitude` and AI perception both read one source
   - K/D/A as replicated ints with `OnRep` broadcasting a delegate — **no polling surface**
   - `CopyProperties` carries team + K/D/A across seamless travel
2. **[netcode-builder]** `Match/BRPlayerController.h/.cpp`:
   - `InputTagPressed(FGameplayTag)` / `InputTagReleased(FGameplayTag)` → forward to the
     PlayerState's ASC. This replaces BP92's stub. The controller holds **no** input buffer
     (it lives in the ASC) and **no** gameplay logic.
   - Pushes `IMC_Default` on `OnPossess` / `OnRep_Pawn`, pops it on unpossess — the pawn never
     pushes its own mapping context (it leaks across respawn if it does)
   - Death cam: on `Event.Death` for the owned pawn, `SetViewTargetWithBlend` to the killer.
     Cosmetic only; the pawn's death handling is BP96's and does not live here.
   - **`FOnCombatSurfaceReady` delegate** — broadcast once the PlayerState has replicated AND
     the pawn is possessed AND the ASC's actor info is initialised. This is the seam `UI/`
     binds to instead of polling for a pawn (gap 4 in the rework doc). Declare and broadcast
     it here; wiring UI to it is out of scope.
3. **[netcode-builder]** Replication review: every new replicated property or `Server` RPC in
   this packet is enumerated in the Log against `BREACHPOINT-ARCHITECTURE.md` §6.1. Anything
   not already in that table is an addition to the replication surface and says so out loud.
   Every `_Validate` is real — no `return true;` stubs.
4. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 2: a spec
   asserting the ASC resolves through `IAbilitySystemInterface` from both the PlayerState and
   (once BP96 lands) the pawn. Rung 4a: dedicated server + 2 clients join; assert **in
   threes** — server view, client A view, client B view — that both PlayerStates replicate
   team, K/D/A, and a non-null ASC.
5. **[critic REFUTER]** Attack surface: what happens to the ASC when the PlayerState is
   destroyed on disconnect mid-ability? Does `CopyProperties` drop the ASC on seamless travel?
   Can a client mutate its own K/D? Is `FOnCombatSurfaceReady` broadcast more than once, or
   never, on a late join?

## Done when

- [ ] `UBRAbilitySystemComponent` is constructed on `ABRPlayerState` and nowhere else
- [ ] `NetUpdateFrequency` is raised explicitly, with the value recorded in the Log
- [ ] Team is `FGenericTeamId` via the engine interface — no bespoke team int, no team subsystem
- [ ] K/D/A changes reach UI via `OnRep` delegate; `grep` finds no per-frame read of them
- [ ] Every `Server` RPC has a real `_Validate`; every new replicated property is listed in the Log
- [ ] `FOnCombatSurfaceReady` fires exactly once per possession, including on a late join
- [ ] Rung 1 as above; rung 2 green; **rung 4a green, asserted in threes**
- [ ] Critic REFUTER pass recorded with findings verbatim
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: netcode-builder owns both classes and every replicated symbol; critic REFUTERs.
- Binary files this ticket OWNS: none.
- Out of scope: `BRGameMode` and `BRGameState` (BP99), the pawn (BP96), any UI binding to
  `FOnCombatSurfaceReady`. Do NOT put respawn, scoring rules, or phase logic here — those are
  GameMode's, and a PlayerState that decides things is the drift this packet exists to avoid.

## Log

(append findings here, dated, newest last)
