# TICKET — BP23: The respawn clock exists, and only the server can see it

> STATUS: open — cut by the UI design pass, 2 Aug 2026. Gap 3 of 4 in
> `docs/UI-DESIGN-SYSTEM.md` §6. BP04 already computes the number; nothing replicates it, so the
> death screen has no timer to draw. Owner BP04.

Founder directive: this is the smallest of the four gaps and the one with the clearest answer.
`ABRGameMode` already holds the exact float — it is private, server-side, and never leaves the
box. Replicate **one server time and let the client render the clock locally**, the same shape
BP04 already proved with `MatchEndServerTime`. No ticking replication, no countdown RPC.

**Ordering law:** 1 → 2 → 3. Step 1 is the only one with a netcode decision in it.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- Ticket BP04 is DONE through step 2 — `ABRGameMode::StartRespawnTimer` runs and
  `EarliestRespawnServerTime` is populated. This ticket replicates a number BP04 must already
  be computing.
- `Breachpoint.Sim.Match` is green (the respawn path is already under spec)
- `UBRVM_Match`'s rocket countdown works end to end — it is the pattern this ticket copies, and
  if it is broken, fix it there rather than inventing a second countdown shape
- owner_path: `Source/Breachpoint/Match/`, `Source/Breachpoint/UI/`

## Steps (in order)

1. One `UPROPERTY(ReplicatedUsing = OnRep_RespawnReadyServerTime)` **float**
   `RespawnReadyServerTime` on `ABRPlayerState`, `DOREPLIFETIME_CONDITION(..., COND_OwnerOnly)`.
   Written **only** by `ABRGameMode::StartRespawnTimer` behind an authority gate; cleared to 0
   on respawn. Zero means "not waiting" — the same zero-means-unset convention `gas-purity.md`'s
   movement amendment already establishes, so a joining client never renders a phantom timer.
   Owner: **netcode-builder**.
2. `UBRVM_Match`: `SetRespawnReadyServerTime(float)`, FieldNotify
   `RespawnSecondsRemaining` (int32, `INDEX_NONE` when not waiting) and `RespawnCountdownText`
   (FText), driven off the existing `UpdateClocks()`/`ScheduleNextClockUpdate()` timer.
   **Copy `RocketSpawnServerTime` → `RocketSecondsRemaining` + `RocketCountdownText` exactly** —
   it is the same problem already solved twelve lines above, and a second clock idiom in one
   ViewModel is the thing to avoid. Owner: **ui-builder**.
3. `ABRPlayerController`: on `OnRep_RespawnReadyServerTime`, push to the VM and raise the death
   overlay. Fed by the existing death-cam path (BP04 step 3), not a new one. Owner: **builder**.
4. Verify: rung 2 extends `Breachpoint.Sim.Match` — timer set on death, cleared on respawn,
   zero during Warmup, behaviour when `bAllowRespawnInSuddenDeath` is false. Rung 4 asserts the
   three views incl. the negative one. Owner: **verifier**.
5. **Critic REFUTER:** a client forging its own respawn time to skip the wait; the timer
   surviving seamless travel; two deaths inside one respawn window; the clock rendering negative
   after a host hitch. Owner: **critic**.

## Done when

- [ ] `ABRPlayerState::RespawnReadyServerTime` replicates `COND_OwnerOnly`, rung 1 green on all
      three targets
- [ ] Rung 4 in threes: the **server** sets it, the **acting client** (the player who died)
      renders a countdown that reaches zero at the same instant the server respawns them, and an
      **observing client** (a third player) **never receives the value** — proven by a packet/
      property check, not by "the UI doesn't show it". `COND_OwnerOnly` is a claim about the
      wire, so it is tested on the wire.
- [ ] The clock is derived, not replicated: grep proves exactly one replicated float and zero
      per-second RepNotifies for this feature
- [ ] Respawning clears it to 0 and the overlay dismisses — no stale timer after the pawn returns
- [ ] Critic findings addressed or waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: netcode-builder step 1 · ui-builder step 2 · builder step 3 · verifier · critic
- Contracts: `netcode.md` (law 1 authority gate on the write, law 3 OnRep is cosmetic — the
  respawn *happens* on the server timer regardless of what any client renders, law 4
  `COND_OwnerOnly` and one float instead of a ticking property, law 5 — an enemy knowing your
  exact respawn instant is information no design document has granted, law 7 join honesty) ·
  `gas-purity.md` (**Match meta** named exception; death itself is `GE_Death` + `State.Dead` and
  this ticket must not add a second death signal) · `testing.md` (rungs 2 + 4)
- **Blocks:** the death screen timer. Figma page `HUD / Death & Respawn`.
  `UI-DESIGN-SYSTEM.md` §5 rules this surface follows Infinite's arena lineage (Campaign Evolved
  has no PvP death screen, so there is no measured reference — the layout is ours by necessity).
- **Bindable today without this:** the death overlay's *non-timer* content. The killer's name
  and the killfeed row are already on `UBRVM_Match` (`FBRKillfeedViewEntry`), vitals read
  `EBRUIDataState` honestly at zero, and the death cam is BP04 step 3's view target. **Only the
  countdown number is missing** — the screen can be built and shipped with the timer slot empty.
- Binary files owned: none
- Out of scope: the killer cam itself (BP04 step 3), the respawn *placement* algorithm
  (BP04 step 2's farthest-from-threat), spectate, the rematch flow (BP10 step 3)

## Log

(append findings here, dated, newest last — this is what the next session reads)

**2 Aug 2026 — filed. The number already exists; this is a plumbing ticket.**

*Verified on disk:* `Match/BRGameMode.h` holds three private, server-only members —
`TMap<TWeakObjectPtr<AController>, float> EarliestRespawnServerTime` (line 135),
`TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers` (137), and
`TSet<TWeakObjectPtr<APlayerState>> PendingRespawnPlayers` (133), plus
`RespawnDelaySeconds = 5.f` (67) and `bAllowRespawnInSuddenDeath = true` (73). **The exact float
the death screen needs is computed and discarded.** `ABRPlayerState` (61 lines) carries nothing
match-related at all. `UBRVM_Match` has no respawn field; it does have the rocket countdown,
which is the working precedent for the derived-clock half.

*Open questions — stated, not guessed:*

1. **Does the countdown start at death, or after the death cam?** BP04 step 3 specifies a death
   cam of **5 s** (view target = killer) and `BRGameMode::RespawnDelaySeconds` is **5.f**. Nothing
   in the repo says whether these are the *same* five seconds or *sequential* ten. The design
   pass drew a timer; it did not say what the timer counts. **A founder call, and it changes what
   step 1 writes into the float.**
2. **What does the screen show when respawn is disallowed?** `bAllowRespawnInSuddenDeath` can be
   false, and `CanRespawnNow()` can refuse. A countdown that reaches 0:00 and does nothing is
   worse than no countdown. The honest states are probably "waiting" vs "eliminated" — but
   nothing designs the second one, and `HUD-AUDIT.md` §5.4 confirms no death/respawn screen
   exists in the audited Figma file at all.
3. **Whose clock?** `UBRVM_Match::SetTimeSource(AGameStateBase*)` already exists and the match
   clock derives from `GetServerWorldTimeSeconds`. This ticket assumes the same source. If
   respawn timing ever moves to a different clock the two countdowns will drift against each
   other on a client with a bad time sync, and nothing today would notice.
4. **The Figma page is taken on trust.** As with BP21: the 2 Aug read-only audit of file
   `yznvnVdOFDADaugZSeomfP` says death/respawn *does not exist* (§5.4), while the design pass
   names a page `HUD / Death & Respawn`. Confirm against the live file before scheduling the
   widget work; the C++ in steps 1–2 is worth landing either way.
