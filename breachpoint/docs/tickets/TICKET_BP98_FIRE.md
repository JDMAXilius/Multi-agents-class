# TICKET — The fire path: predicted hitscan, server validation, reload and swap

> STATUS: open — cut 7 Aug 2026. Blocked on BP97 DONE. **The golden-triangle packet.**

Founder directive: this is the shot heard on three machines. It must be predicted (it has to
feel instant), server-authoritative (a client's word is never damage), and **one packet on the
wire** — activate + TargetData + end batched into a single RPC. No Target Actors: TargetData
is built inline in a prediction window. The reference audit measured the alternative at ~600
lines for the same result.

**Ordering law:** `BRGA_WeaponFire` hitscan path lands and goes green on rung 4a BEFORE the
projectile branch is written (that is BP101). Ship one fire mode working end to end.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP97 DONE — equipping grants an AbilitySet; ammo replicates `COND_OwnerOnly`
- BP94 DONE — `BRDamage::ApplyPoint` exists and `Breachpoint.Sim.Combat` is green and pinned
- `DT_Weapons.csv` has a row with `EBRFireMode::Hitscan` and a populated `DamageFalloff` curve
- owner_path: `Source/Breachpoint/AbilitySystem/Abilities/`, `Source/Breachpoint/Tests/`,
  `Content/Data/`

## Steps (in order)

1. **[sim-builder]** `Abilities/BRGA_WeaponFire.h/.cpp`. **One ability serves every weapon** —
   it reads `EBRFireMode` off the row and branches. This packet builds the **hitscan** branch;
   the projectile branch is a `checkNoEntry()` stub BP101 fills.
   Client path, in order:
   - predicted: cue (`OnActive`), recoil, **ammo −1** — GAS rolls all three back on rejection,
     which is *why* the cost went through the GE path
   - `FScopedPredictionWindow` → trace from the camera → build
     `FGameplayAbilityTargetData_SingleTargetHit`
   - batched RPC (activate + TargetData + end) — assert in the Log that it is ONE packet,
     measured, not assumed
2. **[netcode-builder]** The server validation list, all four, each a separate early-out with
   its own `LogBRNet` verbose line:
   - **rate** ≤ row RPM + tolerance (name the tolerance and justify it in the Log)
   - **ammo** > 0 on the server's instance
   - **cone** — the client's direction is within N° of the server-known muzzle forward
   - **range** ≤ row `RangeMax`
   Rejection is a **silent drop**: the client sees a whiff, a cheater sees nothing. No
   correction RPC, no log to the client. Accept → `BRDamage::ApplyPoint`.
3. **[sim-builder]** `Abilities/BRGA_WeaponUtility.h/.cpp` — two sibling `UCLASS`es in one
   pair: `UBRGA_Reload` and `UBRGA_Swap`. Both **commit on montage notify**
   (`Event.Weapon.ReloadCommit` / `Event.Weapon.SwapCommit`), never on a timer — the animation
   is the timing, which is what keeps 1P and 3P honest and makes an interrupted reload
   correct for free. Reload moves ammo between `ReserveAmmo` and `Ammo` on the server only.
4. **[sim-builder]** `DT_Weapons.csv`: fill the hitscan row properly (damage, RPM, mag,
   reserve, spread, range, falloff curve, body-section mods). Numbers are curator-proposed;
   any number that moves later moves **loudly**, with the reason in a Log.
5. **[sim-builder]** Extend `Breachpoint.Sim.Combat` with the fire path, PINNED: shots-to-kill
   at 0 % / 50 % / 100 % range · headshot STK · shields-then-health ordering · ammo reaches
   zero and the ability then fails `CheckCost` (it must not fire and must not decrement below zero).
6. **[critic REFUTER] — cheat tests, mandatory.** Each is a test, not a thought:
   - client sends TargetData for a target behind a wall → server rejects (cone/trace re-run)
   - client sends fire twice within one RPM interval → second rejected
   - client sends fire with server ammo at 0 → rejected, no damage, no cooldown consumed
   - client sends a hit 3× beyond `RangeMax` → rejected
   - client sends malformed/empty TargetData → no crash, no damage
7. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 2 as above.
   **Rung 4a**: A shoots B — assert in threes that server health, A's view and B's view agree;
   re-run under `-PktLag=120 -PktLoss=5` and assert the same three agree. **Rung 4b REQUIRED**
   (predicted ability — the host runs prediction and authority in one call stack): assert
   server-authority, host-local and remote-client views **separately**.

## Done when

- [ ] One ability class handles both fire modes; the projectile branch is a visible stub
- [ ] Fire is ONE RPC — measured (packet count or a batching assert), stated in the Log
- [ ] All four validations exist, each with its own early-out; rejection is silent
- [ ] Reload and swap commit on montage notify — `grep` finds no timer in either
- [ ] `Breachpoint.Sim.Combat` GREEN and PINNED, including the ammo-exhaustion case
- [ ] **All five cheat tests pass and are committed as tests**, not described in prose
- [ ] Rung 1 as above; **rung 4a green (clean AND emulated) and rung 4b green**, each asserted
      in threes — 4a green is not 4b evidence
- [ ] Critic REFUTER pass recorded with findings verbatim
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: sim-builder owns the abilities; netcode-builder owns validation + both rung-4 axes;
  critic owns the cheat tests and REFUTERs; curators propose the CSV numbers.
- Binary files this ticket OWNS: `Content/Data/DT_Weapons.uasset`; fire/reload montages are
  Tier 4 and belong to an anim packet — this packet tolerates a null montage by falling back
  to a timer **only in the editor**, never in a shipping path, and says so in the Log.
- Out of scope: projectiles (BP101), melee (BP100), grenade (BP101), lag compensation
  (Phase 2 by name — the seam is the server validation function).

## Log

(append findings here, dated, newest last)
