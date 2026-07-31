# TICKET — BP06: Grappleshot — the netcode packet

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP05 (triangle landed, M2
> passed). The single most netcode-sensitive system in the build; treated accordingly.

Founder directive: three uses, one ability — pull to geometry, pull a weapon, pull an enemy.
The pull is a **root-motion source through the CMC** so prediction/reconciliation ride saved
moves. A rejected grapple leaves ZERO state. This ticket does not land without the REFUTER
pass and the emulation rung.

**Ordering law:** 1 → 2 → 3 strictly; 4 and 5 gate landing.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- Ticket BP05 is DONE **and the M2 golden-triangle fun gate passed**, with the verdict
  written in BP05's Log (this is a human judgment gate, not a green suite)
- BP00 rung 4 runs under net emulation (`-PktLag`/`-PktLoss`) — a predicted movement
  source is unprovable without it
- owner_path: `Source/Breachpoint/Character/`,
  `Source/Breachpoint/AbilitySystem/Abilities/`

## Steps (in order)

1. CMC side: `FSavedMove_BR` grapple flag + RMS apply/clear helpers in
   `BRCharacterMovementComponent`; detach rules (arrival radius, jump-cancel, timeout).
   Owner: **netcode-builder**.
2. `BRGA_Grapple`: aimed trace → surface classification (geometry / `ABRWeaponPickup` /
   pawn) → server validates target (range ≤ 20 m from row, LOS, rate) → per-mode execution:
   self-pull RMS · weapon attract (pickup flies to hand via its own replicated motion) ·
   pawn reel (target unaffected in slice? DECIDE: reel pulls SELF to enemy — one-body motion
   only, two-body pull is Phase 2 — log the decision). Cooldown via `GE_Cooldown` (20 s).
   Cues: predicted rope `OnActive`, confirmed hit `Executed`. Owner: **netcode-builder**.
3. Feel pass with anim-builder: pull arc, camera handling, arrival pop — numbers in
   `CT_Combat`, before/after logged. Owner: **netcode-builder** + **anim-builder**.
4. Verify: rung 4 scenario `SmokeTS2C_Grapple` — A grapples across all three modes, assert
   position/state in threes, then under `-PktLag=120 -PktLoss=5`; whiff/reject leaves zero
   state (cooldown NOT consumed on server rejection — spec-proven). Owner: **verifier**.
5. **Critic REFUTER (mandatory gate):** forged grapple RPC to out-of-range target, grapple
   through geometry, spam under packet loss, mid-pull damage/death, grapple during
   sudden-death phase, host-vs-remote asymmetry. Owner: **critic**.

## Done when

- [ ] All three modes work on dedicated + 2 clients, asserted in threes, incl. emulation
- [ ] Rejection path: zero state residue (position, cooldown, cues) — spec-proven
- [ ] Upper arena level reachable ONLY by grapple in the blockout (with BP07)
- [ ] Playtest: grapple used offensively (weapon-yank or enemy-reel), not just travel
- [ ] Critic findings addressed or waived in the Log

## Notes

- Crew: netcode-builder owns · anim-builder feel pass · verifier proves · critic gates
- Contracts: `netcode.md` (prediction reconciles; rejection leaves zero state — the binding law here) · `gas-purity.md` (movement is a NAMED exception: GAS decides, CMC moves) · `animation.md` (step 3 feel pass, proxy honesty) · `testing.md` (rung 4 + emulation)
- Binary files owned: grapple GA/cue assets
- Out of scope: two-body enemy pull (Phase 2), grapple-swing (never — not Halo)

## Log

(append findings here, dated, newest last)
