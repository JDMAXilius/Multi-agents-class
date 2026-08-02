# TICKET — BP22: The reticle does not know what it is pointing at

> STATUS: open — cut by the UI design pass, 2 Aug 2026. Gap 2 of 4 in
> `docs/UI-DESIGN-SYSTEM.md` §6, and the **only** thing on the in-match HUD that cannot be bound
> today. BP03 owns the weapon trace, so the field belongs there.

Founder directive: the reticle turning red is the cheapest threat signal in the game and we
already run the trace that would tell us. This is a **client-local cosmetic read of a trace the
client already performs** — it is not a new replicated surface, and it must never become one.
Telling a client that an enemy is under its crosshair through a wall is a wallhack we shipped
ourselves (`netcode.md` law 5).

**Design constraint, binding — this is the whole point of the ticket:** the reticle's **geometry
must not change with state, only its colour.** The aim point may never appear to move. A state
that swaps components, resizes, or nudges a tick is a `high` finding, not a style note.

**Ordering law:** step 1 (the enum + the VM field) gates step 2 (the producer). Step 3 is written
WITH step 2.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- Ticket BP03 is DONE through step 2 — `BRGA_WeaponFire` exists and performs a client trace
  from the muzzle. This ticket has no producer without it. (BP03's Log records step 2 as
  **not restarted**: `FBRWeaponRow` still has no trace range and no spread.)
- `Source/Breachpoint/UI/BRViewModels.h` compiles and `UBRVM_Combat` binds to the ASC
- owner_path: `Source/Breachpoint/UI/`, `Source/Breachpoint/AbilitySystem/Abilities/`

## Steps (in order)

1. `EBRReticleTargetState : uint8 { Unknown, None, Enemy, Ally, Neutral }` in
   `UI/BRUITypes.h` beside `EBRHitMarkerKind`, plus a FieldNotify
   `EBRReticleTargetState ReticleTargetState` on `UBRVM_Combat` with
   `SetReticleTargetState()` — same construction as the existing `SetGrenadeCount()`.
   `Unknown` is the honest pre-first-trace value, matching `EBRUIDataState`'s existing pattern.
   Owner: **ui-builder**.
2. The producer, in the ability path: the existing client trace classifies its hit actor
   (team compare against the local `ABRPlayerState`) and raises a delegate the VM listens to.
   **`gas-purity.md` law 7 — events over calls at the seams:** the ability does not reach into
   the UI. No new trace, no `NativeTick` (law 4) — piggyback the trace the fire path already
   runs, or a timer if the fire path only traces on pull. Owner: **sim-builder**.
3. **The refutation ships with the feature.** A test that stands an enemy behind geometry and
   asserts the state reads `None`, and a test that the state is derived only from a trace the
   client could run itself — i.e. **no server ever sends this**. Owner: **netcode-builder**.
4. Verify: rung 2 (classification table: enemy / ally / neutral / nothing / occluded / dead
   body / bot), rung 4 for the three views. Owner: **verifier**.
5. **Critic REFUTER:** occlusion bypass, team-swap mid-trace, the state leaking a target the
   player cannot see, and the geometry constraint (does the widget swap a *component* or a
   *token*?). Owner: **critic**.

## Done when

- [ ] `UBRVM_Combat` exposes `ReticleTargetState` as FieldNotify and rung 1 is green on all
      three targets
- [ ] Classification table spec-proven at rung 2, including **occluded enemy reads `None`**
- [ ] Rung 4, honestly in threes: on the **listen host** and on the **acting client** the state
      flips crossing an enemy; on an **observing client** the value is never received — there is
      no replicated property to receive, and the check is that grep finds no `DOREPLIFETIME` for
      it. *Stated plainly: the third view here proves an absence, which is the only honest form
      of a three-view claim for client-local state.*
- [ ] **Geometry unchanged across states**: the reticle widget swaps a colour token only
      (`Shield #35D0F2` at rest → `Enemy #FF4A3D` over threat, `UI-DESIGN-SYSTEM.md` §2), proven
      by a pixel diff of the two states showing zero non-colour delta
- [ ] Critic findings addressed or waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: ui-builder step 1 · sim-builder step 2 · netcode-builder step 3 · verifier · critic
- Contracts: `netcode.md` (**law 5 hidden state is the binding one** — this must be culled at
  the trace, not at the render; law 3 — a cosmetic read may not fork gameplay) ·
  `gas-purity.md` (law 2 abilities are the only action entry, law 7 events over calls at the
  seams — the ability raises, the VM listens) · `data-and-assets.md` (the colour is a token read
  by the widget, never typed into a WBP) · `testing.md` (rungs 2 + 4)
- **Blocks:** the reticle colour change. Figma page `HUD / Elements`, component set
  `SET Reticle / *` — `HUD-AUDIT.md` §3.5 records `SET Reticle / Enemy State` (`62:29`) as
  42.67², *"same build as AR"*. **The design already honours the constraint**: the enemy state is
  the identical geometry, which is exactly why this is a token swap and not a component swap.
- **Bindable today without this:** everything else on the in-match HUD — vitals, ammo, stowed
  weapon, grenades, grapple ring, score, clock, rocket countdown, killfeed. Every one of those
  getters is already on `UBRVM_Combat` / `UBRVM_Match` (`UI-DESIGN-SYSTEM.md` §6). Note also
  that `UBRVM_Combat` already has hit-marker *events* (`ShieldHitConfirmed`, `FleshHitConfirmed`,
  `HeadshotHitConfirmed`, `KillConfirmed`) — those are **post-hit confirmations** and are not a
  substitute for a pre-fire state. This ticket is the only missing piece of the reticle.
- Binary files owned: none (`WBP_HUDLayout` is BP10's)
- Out of scope: ADS/scope overlay, hit markers (built), reticle *shape* per weapon, the sniper
  centring defect (`HUD-AUDIT.md` §3.5, a Figma repair not a C++ gap)

## Log

(append findings here, dated, newest last — this is what the next session reads)

**2 Aug 2026 — filed. Verified on disk, plus four things nobody has decided.**

*Verified:* `UBRVM_Combat` (`UI/BRViewModels.h`, 160 lines) has no target-state field of any
kind. `UI/BRUITypes.h` declares `EBRUIDataState` and `EBRHitMarkerKind` and nothing reticle-
shaped. Grep for `Reticle` across `Source/` returns only `UBRReticleWidget` as an *unbuilt* row
in `UI-DESIGN-SYSTEM.md` §4's inventory — there is no such class.

*Open questions — each needs a decision, none should be guessed during execution:*

1. **Do allies get a state at all?** The colour system has `TeamThem #FF7A45` for the opposing
   team *in lists* and `Enemy #FF4A3D` for threat. It names no token for "your teammate is under
   your crosshair". If friendly fire is off, an ally state may be pure noise. The enum carries
   `Ally` because the design pass named it; whether the widget renders it is unanswered.
2. **What is `Neutral`?** Pickups, the rocket spawner, destructibles, a corpse — the design pass
   named the state and no document enumerates its members.
3. **Always, or only in ADS?** Unstated everywhere. This changes step 2's trace cadence, which is
   the only cost-bearing part of the ticket.
4. **Cadence when the fire path does not trace.** BP03 traces on pull. A reticle state that only
   updates when you shoot is useless. Whether that is a per-frame trace (forbidden — law 4), a
   timer, or a cheaper overlap query is an implementation call with a real perf number attached,
   and `BREACHPOINT-QUALITY-BARS.md` §2 should be read before it is made.

*One dependency stated honestly:* BP03's step 2 has **not** restarted. Its Log (1 Aug) records
`FBRWeaponRow` still carrying no trace range and no spread. **There is no trace to piggyback on
today**, so this ticket's step 2 is blocked on the same two data fields that block the fire path.
Step 1 is not — the enum and the VM field can land alone, and a widget can bind to `Unknown`.
