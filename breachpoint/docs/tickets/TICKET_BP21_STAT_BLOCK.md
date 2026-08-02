# TICKET — BP21: The per-player stat block — nothing counts anything yet

> STATUS: open — cut by the UI design pass, 2 Aug 2026. Recorded as gap 1 of 4 in
> `docs/UI-DESIGN-SYSTEM.md` §6. The entire post-game carnage report is designed and has
> nothing to bind to. Needs BP04's match spine to be the writer.

Founder directive: the carnage report is the match's memory. It is drawn and it is dead, because
**no object in this repo holds a per-player number.** Match meta is a NAMED exception in
`gas-purity.md` — server-only mutation in GameMode, replicated by PlayerState RepNotify, never
read back by the combat sim. Build it that way or it becomes a second damage pipeline with a
scoreboard on top.

**Ordering law:** step 1 (the replicated block) gates 2 and 3. Step 4 (the VM) needs 1 only.
Step 2 (the counters) crosses into BP03's fire path — read the owner-path note in Notes BEFORE
claiming, because it decides whether this is one packet or two.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- Ticket BP04 is DONE through step 2 — `ABRGameMode` resolves kill attribution and calls a
  single scoring site. This ticket has no author for its numbers until that site exists.
- `Source/Breachpoint/Match/BRPlayerState.h` compiles and `Breachpoint.Sim.Match` is green
- `Content/Data/DT_Medals.csv` re-imports clean against `FBRMedalRow` (`BRDataRows.h:211`) —
  the medal *catalogue* is the input to step 3's earned-set
- owner_path: `Source/Breachpoint/Match/`, `Source/Breachpoint/UI/`

## Steps (in order)

1. `FBRPlayerStatBlock` (USTRUCT, `Match/BRPlayerState.h`) + one `ReplicatedUsing=OnRep_Stats`
   member on `ABRPlayerState`: `Score`, `Kills`, `Deaths`, `Assists`, `ShotsFired`, `ShotsHit`,
   `TArray<FName> MedalsEarned` (row names into `DT_Medals`). ONE RepNotify for the struct, not
   seven properties — the scoreboard redraws as a unit. Server-only mutators
   (`ServerAddKill`/`ServerAddDeath`/…), authority-gated, mirroring
   `ABRGameState::ServerAddTeamScore`'s shape. Owner: **netcode-builder**.
2. Accuracy counters. `ShotsFired`/`ShotsHit` increment at exactly two sites in
   `BRGA_WeaponFire`: fired on the **server-validated** commit (never the predicted one — see
   the open question), hit on the `GE_Damage` application. Owner: **netcode-builder**, and see
   the owner-path note — this file is BP03's.
3. Medal award path: `ABRGameMode` maps `FBRMedalRow::TriggerId` to the match events it already
   raises and appends to `MedalsEarned`. Zero new gameplay events — if a medal needs an event
   that does not exist, file it, do not invent it. Owner: **builder**.
4. `UBRVM_Scoreboard` (`UI/BRViewModels.h`, third VM beside `UBRVM_Combat`/`UBRVM_Match`):
   FieldNotify `TArray<FBRScoreboardRow>` sorted server-side-identically on every client,
   `EBRUIDataState` for join-in-progress, fed by `ABRPlayerState::OnRep_Stats` +
   `GameState->PlayerArray` change delegates. No polling, no `NativeTick`. Owner: **ui-builder**.
5. Verify + refute: `Breachpoint.Sim.Match` gains a stat-block table (kill, death, assist,
   double-KO both-credit, self-kill −1 per BP04's ruling, shot fired but rejected by validation).
   Rung 4 asserts the three views. **Critic REFUTER:** a client that forges its own stat block;
   stats mutating during Warmup; a PlayerState destroyed mid-match taking the carnage report's
   row with it. Owners: **verifier**, **critic**.

## Done when

- [ ] `ABRPlayerState` carries `FBRPlayerStatBlock` and rung 1 is green on all three targets
- [ ] A kill on a listen host updates the block identically in **three views** — server, the
      acting client (the killer), and an observing client (a third player's scoreboard) — proven
      at rung 4 in `BRGauntlet.SmokeTS2C`, not in PIE
- [ ] Accuracy is arithmetic, not a guess: `ShotsHit ≤ ShotsFired` holds after a
      rejected-fire cheat run from BP03 step 3 (a rejected shot increments neither)
- [ ] Zero client writes: every mutator refuses without authority, spec-proven
- [ ] `UBRVM_Scoreboard` renders an honest empty state for a null/late PlayerState (netcode law 7)
- [ ] Critic findings addressed or waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: netcode-builder owns steps 1–2 · builder step 3 · ui-builder step 4 · verifier · critic
- Contracts: `netcode.md` (law 1 authority gate, law 3 RepNotify-as-cosmetic, law 4 minimum
  replication — the block is public scoreboard data so NOT `COND_OwnerOnly`, law 7 join honesty,
  law 8 the forge-your-own-stats attack) · `gas-purity.md` (the **Match meta** named exception is
  what permits this to exist outside GAS at all; its bound — "nothing in the combat sim reads it"
  — is a review criterion, not a footnote) · `testing.md` (rungs 2 + 4)
- **Blocks:** Figma page `HUD / Scoreboard & PGCR` — the entire post-game carnage report, plus
  the in-match scoreboard. `UI-DESIGN-SYSTEM.md` §5 rules the PGCR follows Infinite's structure
  restyled into the reference file's language.
- **Bindable today without this:** the whole in-match HUD except reticle colour state
  (`UI-DESIGN-SYSTEM.md` §6). Specifically `UBRVM_Match` already serves `Team0Score`,
  `Team1Score`, `MatchClockText`, `RocketCountdownText` and the killfeed — **team** totals bind
  now; **no per-player row binds at all**, which is the whole carnage report.
- Binary files owned: none
- Out of scope: XP/progression (the reference file's "Post Game XP" is a progression screen, not
  a scoreboard — `UI-DESIGN-SYSTEM.md` §5), the coach line, medal *art*, WBP layout (BP10)
- **Owner-path warning (read before claiming).** Step 2 writes `BRGA_WeaponFire`, which lives in
  `Source/Breachpoint/AbilitySystem/Abilities/` and belongs to BP03 — and BP03's own Log records
  that folder as already missing from its `owner_path`. Two honest options: split step 2 into a
  BP03 packet, or extend this ticket's grant by exact file. **Do not edit shared code to unblock
  (law 5) — file the `contract_gap` and stop.**

## Log

(append findings here, dated, newest last — this is what the next session reads)

**2 Aug 2026 — filed. What was verified on disk, and what is genuinely open.**

*Verified:* `ABRPlayerState` (`Match/BRPlayerState.h`, 61 lines) carries **only** the ASC, the
AttributeSet and the startup-loadout handles. There is no TeamID, no K/D/A, no score — **BP04
step 1's ticket text promises "`BRPlayerState`: TeamID, K/D/A RepNotify" and that half never
landed.** `ABRGameState` has `TeamScores` (team totals) and the killfeed ring buffer, so the
*team* half of BP04 step 1 is real; the *player* half is not. `FBRMedalRow` exists in
`BRDataRows.h:211` with `MedalName`/`Description`/`TriggerId` — the catalogue is authored and
**nothing in `Source/` reads `TriggerId`**, so no medal can be awarded today.

*Open questions — stated, not guessed:*

1. **Is `Score` a separate economy from `Kills`?** BP04 scores the *match* by kills to 25. A
   per-player `Score` that is just `Kills` is a duplicate field; a `Score` that weights assists,
   medals or objectives is a design decision nobody has made. **Needs a founder call before step 1
   fixes the struct.**
2. **Which shot counts as fired — the predicted one or the validated one?** The fire path is
   client-predicted and server-validated (`netcode.md` fill-in). Counting the predicted shot makes
   accuracy match what the player *felt* and lets a mispredict inflate the denominator; counting
   the validated one is honest and will read as "lower accuracy than I shot". The Done-when box
   above assumes **validated**, because a rejected cheat shot incrementing anything is a finding —
   but this is a call, not a derivation.
3. **Assists have no definition.** BP04 decided kill *attribution* (last damaging instigator
   ≤ 5 s); it decided nothing about assists. Damage threshold? Time window? Shield-break credit?
4. **Nothing counts a shot for a bot.** If bots share `BRGA_WeaponFire` they will populate
   accuracy for free; if the bot fire path diverges, the scoreboard's bot rows will read 0/0.
   Unchecked — `Source/Breachpoint/Bots/` was not read for this filing.
5. **The Figma page name is taken on trust.** `HUD-AUDIT.md` §5.4 (2 Aug, read-only pass over
   file `yznvnVdOFDADaugZSeomfP`) states plainly: *"scoreboard, death/respawn and pause menu do
   not exist."* The design pass that cut this gap names a page `HUD / Scoreboard & PGCR`. Either
   it was authored after the audit or it lives in a different file. **"Fully designed and waiting"
   should be confirmed against the live file before anyone schedules step 4 against it.**
