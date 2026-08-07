# TICKET — The projectile: time-parameterized flight, owner-local prediction, and the grenade

> STATUS: open — cut 7 Aug 2026. Blocked on BP100 DONE. **D-1 (the Tick exception) applies.**

Founder directive: a networked projectile that replicates its transform is a laggy projectile.
The reference audit found the better answer in ZoransResistance and this packet adopts it:
position is a **pure function of time**, the owner never receives the replicated copy (it runs
its own local one), and server truth is **lerped in over a capped window** rather than snapped.
This is also the packet that fills the "client ghost for feel" gap the architecture doc left
unspecified — the ghost learns the server's outcome through an ASC-minted `LocalIndex`.

**Ordering law:** `ABRProjectile` lands and is green on rung 4a before `BRGA_Grenade` uses it.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP100 DONE
- BP93 DONE — the ASC exposes `GenerateLocalProjectileId()`, `RegisterLocalProjectile()` and
  the `ProcessProjectileHit` client RPC (declared in BP93 step 3, consumed here)
- **D-1 has a dated ruling in `docs/DESIGN-RULINGS.md`.** If D-1 was taken, `gas-purity.md`'s
  Named Exceptions ledger already carries the projectile row — **this ticket does not add it**
- `DT_Weapons.csv` has a row with `EBRFireMode::Projectile`
- owner_path: `Source/Breachpoint/Equipment/` (or `Weapons/` per D-2),
  `Source/Breachpoint/AbilitySystem/Abilities/`, `Source/Breachpoint/Tests/`

## Steps (in order)

1. **[sim-builder]** `BRProjectile.h/.cpp` — `ABRProjectile`:
   - **Position is `f(SpawnPoint, LaunchDir, Elapsed, GravityScale)`** — a closed form, no
     `UProjectileMovementComponent`, no replicated transform, `SetReplicateMovement(false)`
   - Spawn params replicate `COND_InitialOnly`: spawn point, direction, **server spawn time**,
     `LocalIndex`
   - **`IsNetRelevantFor` returns false for the instigator** — the shooter never receives the
     replicated copy; it spawns and runs its own local one
   - Per-frame update: **D-1's bound applies** — Tick does position + sweep and **nothing
     else**; no gameplay decision in Tick; tick disabled the instant the projectile
     deactivates. If D-1 was refused, use the recursive next-tick timer and say so in the Log.
   - Divergence between the local and server-authoritative solution is **lerped over a capped
     sync window** (start ~0.25 s cap), never snapped. Name the cap in the Log.
   - Hit: server sweeps, applies damage via `BRDamage::ApplyPoint` (direct) or `ApplyRadial`
     (explosive), multicasts the cosmetic hit, and routes the outcome to the **owner** by
     `LocalIndex` through the ASC RPC so the local ghost resolves correctly
   - Deactivate ≠ destroy: stay alive ~2 s after a hit so late clients still receive the cue
2. **[sim-builder]** `Abilities/BRGA_WeaponFire` — fill the projectile branch stubbed in BP98.
   It mints the `LocalIndex`, spawns the local ghost on the owning client, and sends the same
   index to the server in the batched RPC. **Same ability, same validation list** (rate, ammo,
   cone, range) — a projectile weapon is a row, not a code path.
3. **[sim-builder]** `Abilities/BRGA_Grenade.h/.cpp`:
   - Cook on press, throw on release (WhileHeld policy)
   - Cost is **`GE_AbilityCost`** — never a hand-decremented counter (purity law 4)
   - Client spawns the ghost immediately; server spawns the authoritative `ABRProjectile`
   - Explosion routes to `BRDamage::ApplyRadial` with `Damage.Explosive` — **the same door**
     as every other damage source. `ApplyRadial`'s per-body-section visibility trace is what
     makes cover work; do not re-implement a radial query here.
   - Lists the sprint tag in `CancelAbilitiesWithTags` (BP100 step 3)
4. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 2:
   `Breachpoint.Sim.Projectile` — the closed-form solution at t=0, mid-flight and t=lifespan
   matches an analytic expectation; a 100 ms spawn-time discrepancy converges within the sync
   cap; radial damage through a blocker is zero. **Rung 4a**: A throws a grenade at B; assert
   in threes that the explosion location, damage and cue agree across server, A and B; re-run
   under `-PktLag=120 -PktLoss=5` and assert the ghost and the authoritative projectile land
   within a stated tolerance. **Rung 4b REQUIRED** — on a listen server the host's ghost and
   the authoritative projectile are in one process; assert authority / host-local / remote
   separately, because that collapse is exactly the bug this axis exists to catch.
5. **[critic REFUTER]** Attack surface: does the shooter ever see two grenades? What if the
   owner disconnects mid-flight (the `LocalIndex` registry must not leak)? Does a hit arriving
   after deactivation double-apply damage? Can a client spawn a projectile without the server
   (spawn must be server-authoritative — the ghost is cosmetic and deals no damage, ever)?
   What happens when two projectiles share a `LocalIndex` after the counter wraps?

## Done when

- [ ] No `UProjectileMovementComponent` and no replicated transform on `ABRProjectile`
- [ ] `IsNetRelevantFor` returns false for the instigator — asserted, not assumed
- [ ] The client ghost **deals no damage** under any path — a committed test proves it
- [ ] Server outcome reaches the owner by `LocalIndex`; the registry is cleaned on
      disconnect, on deactivate, and on counter wrap
- [ ] Divergence is lerped over a capped window; the cap is stated in the Log
- [ ] The projectile's per-frame update matches D-1's ruling exactly, and the Log says which
      ruling was taken
- [ ] Grenade cost is `GE_AbilityCost`; `grep` finds no hand-decremented grenade counter
- [ ] Explosion goes through `BRDamage::ApplyRadial` — the grep gate from BP94 stays clean
- [ ] Rung 1 as above; rung 2 `Breachpoint.Sim.Projectile` GREEN and PINNED; **rung 4a green
      (clean AND emulated) and rung 4b green**, each asserted in threes
- [ ] Critic REFUTER pass recorded with findings verbatim
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: sim-builder owns the projectile and grenade; netcode-builder owns relevancy, the
  `LocalIndex` correlation and both rung-4 axes; critic REFUTERs.
- Binary files this ticket OWNS: `Content/Data/DT_Weapons.uasset` (the projectile row).
- Out of scope: grapple (BP102), rocket-launcher tuning (a curator packet), lag compensation
  (Phase 2 by name). Do not add a second projectile class — bounce, stick and cluster
  behaviours are **rows and parameters**, not subclasses.

## Log

(append findings here, dated, newest last)
