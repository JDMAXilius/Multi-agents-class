# TICKET — The grapple: a predicted pull through CMC's root-motion source

> STATUS: open — cut 7 Aug 2026. Blocked on BP101 DONE. **The netcode packet.
> Critic REFUTER is mandatory, not optional. Rungs 4a AND 4b.**

Founder directive: this is the single most dangerous thing in the slice, and it is dangerous
in a specific way — a mispredicted *position* is the one error a player feels instantly and
cannot forgive. The design answer is to refuse to write reconciliation code: the pull is a
**root-motion source applied inside CMC**, so it rides the same saved-move pipeline as
walking, and a rejected grapple leaves **zero** state behind.

**Ordering law:** the CMC side (`ApplyGrappleRootMotion`, declared in BP96) is implemented and
green on a synthetic test before the ability calls it. Do not debug an ability and a movement
mode at the same time.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP101 DONE
- BP96 DONE — `FSavedMove_BR` already declares the **grapple** compressed flag, and
  `UBRCharacterMovementComponent::ApplyGrappleRootMotion(FVector, float)` is declared
- BP99 DONE — respawn is green, so a grapple interrupted by death has a defined outcome
- owner_path: `Source/Breachpoint/AbilitySystem/Abilities/`, `Source/Breachpoint/Character/`,
  `Source/Breachpoint/Tests/`

## Steps (in order)

1. **[netcode-builder]** Implement `ApplyGrappleRootMotion` in
   `BRCharacterMovementComponent`: build an `FRootMotionSource_MoveToForce` (or
   `_JumpForce`, whichever the feel test picks — state which and why in the Log), apply it
   through `ApplyRootMotionSource`, and set the grapple compressed flag in `FSavedMove_BR`.
   Detach rules live here: **arrival radius**, **jump-cancel**, **death**, **timeout**. Each
   detach path removes the root-motion source by handle — never by clearing all sources.
2. **[netcode-builder]** Synthetic test first, before any ability exists: apply the source
   directly in a spec, assert the pawn arrives, assert a forced client correction mid-pull
   replays to the same end position. **This is the step that de-risks the packet** — if the
   RMS does not survive a correction, nothing above it can.
3. **[sim-builder]** `Abilities/BRGA_Grapple.h/.cpp`:
   - Client traces, sends the hit as TargetData in a prediction window (same shape as fire —
     **no Target Actor**), batched RPC
   - Server validates: range ≤ table max, line of sight from the server-known muzzle, surface
     classification is grappleable, cooldown not active. **Rejection leaves zero state**: no
     RMS applied, no flag set, no tag granted, no cooldown consumed. Assert this.
   - On accept, the server calls `ApplyGrappleRootMotion`; the client predicted the same call
     in the same prediction window
   - Cooldown is `GE_Cooldown` (SetByCaller duration from the table), never a timer member
   - Lists the sprint tag in `CancelAbilitiesWithTags`
4. **[sim-builder]** Values (range, pull strength, arrival radius, cooldown, timeout) are
   `DT_*` rows. Zero literals in the ability or the CMC.
5. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 2:
   `Breachpoint.Sim.Grapple` — accept applies exactly one RMS; reject applies none and
   consumes no cooldown; each of the four detach paths removes the source by handle.
   **Rung 4a** (dedicated + 2 clients): A grapples; assert in threes that A's final position,
   the server's and B's observed position agree within tolerance. Re-run under
   `-PktLag=120 -PktLoss=5` **and** at `-PktLag=250` — a grapple is the packet where the
   tolerance must be stated as a number, not as "looks fine".
   **Rung 4b REQUIRED and load-bearing**: on a listen server the host runs prediction and
   authority in one call stack, so a prediction bug here is **invisible to 4a by
   construction**. Assert server-authority, host-local and remote-client views separately.
6. **[critic REFUTER] — mandatory, with the attack surface named:**
   - Does a rejected grapple leave an RMS, a flag, a tag, or a cooldown? (all four checked)
   - Does dying mid-pull leave the RMS on the corpse or on the respawned pawn?
   - Jump-cancel during a correction replay — does the flag replay consistently?
   - Two grapples in flight (double-press before the first resolves)
   - Grapple to a moving target; grapple to an actor destroyed mid-pull
   - Does the pull ever move the pawn through geometry? (RMS does not sweep by default —
     state the mitigation)
   - Client claims a target 2× beyond range → rejected with zero state

## Done when

- [ ] The synthetic RMS test (step 2) is committed and green **before** the ability exists in
      the history — verifiable from the commit order
- [ ] Rejection leaves zero state: no RMS, no flag, no tag, no cooldown — all four asserted
- [ ] All four detach paths remove the source **by handle**; `grep` finds no blanket clear
- [ ] Cooldown is `GE_Cooldown`; `grep` finds no timer member in the ability
- [ ] Zero literals in `BRGA_Grapple.cpp` and the grapple path of the CMC
- [ ] No Target Actor anywhere — TargetData is built inline in a prediction window
- [ ] Rung 1 as above; rung 2 `Breachpoint.Sim.Grapple` GREEN and PINNED
- [ ] **Rung 4a green at 0 ms, 120 ms/5 % and 250 ms**, with the position tolerance stated as
      a number in the Log
- [ ] **Rung 4b green, asserted in threes across two processes** — 4a green is explicitly not
      accepted as 4b evidence
- [ ] Critic REFUTER pass recorded with **every** attack surface above answered individually
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: netcode-builder owns the CMC side and both rung-4 axes; sim-builder owns the ability;
  critic REFUTERs and holds the gate. **This ticket does not land on a critic summary — it
  lands on itemised answers.**
- Binary files this ticket OWNS: none. Grapple montage is Tier 4 (anim packet).
- Out of scope: grapple-to-player, swing physics, chained grapples, lag compensation. Each is
  a design change with its own packet. If the feel test wants swing, that is a new ticket and
  a new REFUTER pass — not an amendment to this one.

## Log

(append findings here, dated, newest last)
