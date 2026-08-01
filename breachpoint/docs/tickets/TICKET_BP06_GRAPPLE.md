# TICKET — BP06: Grappleshot — the netcode packet

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP05 (triangle landed, M2
> passed). The single most netcode-sensitive system in the build; treated accordingly.

Founder directive: three uses, one ability — pull to geometry, pull a weapon, pull an enemy.
The pull is a **root-motion source through the CMC** so prediction/reconciliation ride saved
moves. A rejected grapple leaves ZERO state. This ticket does not land without the REFUTER
pass and the emulation rung.

**Reference skill:** `cmc-prediction` (saved moves, the compressed-flag budget, the pull as a
root-motion source, and why zero-residue rejection comes free from three lawful choices).
**UNVERIFIED draft** — if BP02 has not already corrected it, this packet does.

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

**31 Jul 2026 — PRE-FILED CONTRACT_GAP (lead, from the BP01 session).** Fourth and last of the
four mismatches BP01's Log flagged as *"not written down in this repo"*; all four are now filed
in the tickets that will hit them.

Current: `Source/Breachpoint/Character/`, `Source/Breachpoint/AbilitySystem/Abilities/`

| Deliverable | Lives in | Status |
|---|---|---|
| Weapon-attract mode — the pickup "flies to hand via **its own replicated motion**" (step 2) | `Source/Breachpoint/Weapons/` | BLOCKED |
| Feel-pass numbers land in `CT_Combat` (step 3) | `Content/Data/CT_Combat.csv` | BLOCKED |
| The zero-residue spec — cooldown NOT consumed on server rejection (step 4) | `Source/Breachpoint/Tests/` | BLOCKED |

This ticket's core is well-scoped: `FSavedMove_BR` + the RMS helpers are `Character/`, and
`BRGA_Grapple` is `AbilitySystem/Abilities/` — both owned. The gaps are all at the edges where
the grapple reaches into someone else's system, which is the honest shape of the feature.

The first row is the one to think about before claiming rather than during. Grapple-a-weapon
means motion code on `ABRWeaponPickup`, a class **BP03 authors and owns**. That is not merely an
owner_path amendment — it is two packets writing the same file across a milestone boundary, which
law 7's one-owner rule exists to forbid. Cleanest resolution is probably that BP03 lands the
pickup's replicated-motion seam (an interface or a server-called `AttractTo`) as part of its own
packet, and BP06 only *calls* it — but that is a design call for the lead, and it has to be made
**before BP03 closes**, or BP06 arrives to find the seam absent and its owner archived.

*Unowned, not blocked:* step 4's rung-4 scenario `SmokeTS2C_Grapple` lives with the Gauntlet
harness, outside `Source/` and `Content/`, so the hook does not confine it — but no ticket's
`owner_path` names it either. BP00 owns the harness; whoever adds a scenario is writing in BP00's
folder. Worth settling when BP00 lands rather than at BP06's claim.

See BP03's Log for the systematic `Tests/` finding, and BP05's for the projectile-has-no-home
ARCHITECTURE gap.
