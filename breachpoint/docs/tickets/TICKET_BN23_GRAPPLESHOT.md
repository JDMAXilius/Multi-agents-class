# TICKET — BN23: the Grappleshot, self-pull first cut

> STATUS: landed cloud-side 27 Aug 2026 — ~~WRITTEN, NOT COMPILED~~ **COMPILED and WIRED,
> 28 Aug 2026.** All targets clean; the saved-move path compiles for the first time in BN.
> Player input is bound to key **"1"** (UE name `One`), three cues are registered live
> (Fire / Rope / Hit), a **C++ camera shake** landed, and real assets are wired — tracer
> beam as the rope, muzzle flash, impact burst. **STILL UNSET: rumble, the taut-line loop,
> the hit sound, and `FireMontage`. NOTHING about the feel is proven — no PIE hook, no
> multi-process pass, no threes.** From the founder's
> grapple discussion. Scope RULING recorded there: SELF-PULL ONLY this cut;
> WeaponAttract is the safe second packet; PawnReel awaits its own founder ruling on
> whether BREACHPOINT wants enemy-yanking at all. Contracts: netcode.md (clients send
> intent; the server re-derives), gas-purity.md (cooldown is a GE, the ability decides,
> the component moves), cmc-prediction skill (CORRECTED BY THIS PACKET where they
> disagree — the compiled BR precedent overrode its unverified draft, see Log).

## What landed (transcribed from the COMPILED legacy pair, not the draft skill)

- `Characters/BNCharacterMovementComponent.h/.cpp` — BN's first custom CMC, grapple
  half of the compiled `UBRCharacterMovementComponent` port: `FSavedMove_BN`
  (FLAG_Custom_0 = grapple; bits 1-3 free, the budget documented), the prediction-data
  factory, `StartGrapplePull` as `FRootMotionSource_MoveToForce` (Override accumulate,
  priority 5, off-ground first), jump-cancel + dropped-intent-replay stop in
  BeforeMovement, arrival + natural-expiry cleanup in AfterMovement. Tuning is Config
  (PullSpeed 1800uu/s, Arrival 120uu, MaxPull 1.5s) — BN's ini idiom replaces the BR
  curve-table read.
- `BNCharacter` ctor → ObjectInitializer form with `SetDefaultSubobjectClass` (the
  compiled BRCharacter.cpp:21 pattern).
- `AbilitySystem/Abilities/BNGA_Grapple.h/.cpp` — LocalPredicted body verb: each side
  traces its OWN eyes-forward line (client = prediction, server = truth), the authority
  re-validates range + LOS before anything moves, commit happens ONLY after both pass
  (a whiff costs nothing; a rejection rolls the predicted cooldown back), then
  `StartGrapplePull` and a fire-and-forget clean end. DATED DELTA from the BR original:
  the BR ability stayed active with nothing recorded to end it — BN ends immediately,
  and only a CANCELLED end (death, the sweep) stops a running pull. Pawns under the
  reticle refuse (self-pull scope). ECC_Visibility, the projectile-LOS precedent —
  the BR bespoke grapple channel deliberately not ported.
- `UBNGE_GrappleCooldown` (grenade-cooldown shape, SetByCaller duration 4s Config,
  Cooldown.Grapple on the spec), tags `Input.Grapple`/`Cooldown.Grapple`, the C++
  grant beside Melee/Grenade/ADS (no ability-set asset edit), controller bind +
  handler (a tap, the grenade's shape).

## The ONE asset step (terminal)

`IA_Grapple` exists (`/Game/Input/Actions/IA_Grapple`, BR-era) and the key mapping
with it; what is missing is the ROW in BN's `UBNInputConfig` DataAsset mapping
`Input.Grapple` → IA_Grapple. Until it lands, the Bind logs the designed loud error
("Input.Grapple has no InputAction in ...") and humans cannot press it — BOTS never
need it (the adapter presses tags directly), which also makes bot-grapple (a later
packet: Verb.Grapple, the Gantry for AIB) independent of this step.

## Done when (terminal)

- [x] Rung 1 — the saved-move path compiles for the first time in BN (28 Aug, clean).
      `BreachpointServer` unsatisfiable on this launcher install (environmental, not
      ticked as a pass — AIB1's precedent)
- [x] InputConfig row added — `Input.Grapple` → `IA_Grapple`, bound to key **"1"**
      (`One`). The designed loud error ("Input.Grapple has no InputAction in ...") no
      longer fires; humans can press it
- [ ] PIE host: hook a surface — pulled at ~1800uu/s, detach at arrival; jump mid-pull
      keeps momentum; a sky shot costs NO cooldown; a wall-blocked claim logs REFUSED
- [ ] The Gantry reachable by grapple from the mid deck (the arena's design promise)
- [ ] Multi-process PIE (Run Under One Process OFF): remote client's pull predicts
      without rubber-band; `-PktLag=120 -PktLoss=5` forces a correction and the pull
      replays or cleanly stops — REPORTED AS "editor multi-process", never rung 4
- [ ] Threes at rung 5 with the rest of the stack: server/acting/observing agree on
      the path; a corpse never keeps flying (the cancel path)

## Log

### 27 Aug — the build, and the skill correction it owes

The cmc-prediction skill's own header demands the first real packet correct it:
the compiled BR pair settled the skill's open question FOR THIS PROJECT — compressed
flags (FLAG_Custom_*), not the FCharacterNetworkMoveData path; BN transcribes the
compiled answer. Bit budget after BN23: Custom_0 grapple, 1-3 free (BN sprint is a GE
and spends nothing). The skill's §2-§4 API shapes matched the compiled code exactly
except: BR's saved move carries sprint (BN's does not — GE sprint), and BR read tuning
from curve tables (BN reads Config).

### 2026-08-28 — board-hygiene pass: wired end to end, felt by nobody

Corrections and this session's verified facts; nothing re-measured.

**Closed since the build:**

- **It compiles**, all targets — the header's "WRITTEN, NOT COMPILED" is stale, and with
  it goes the packet's biggest transcription risk: `FSavedMove_BN`, the prediction-data
  factory and `FRootMotionSource_MoveToForce` are BN's first custom CMC and they build.
- **The one asset step is done.** `Input.Grapple` maps to `IA_Grapple` and the key is
  **"1"** (`One`).
- **The presentation half landed further than this ticket asked for**: three cues
  registered live (**Fire / Rope / Hit**), a **C++ camera shake**, and real assets wired —
  the tracer beam doing duty as the rope, muzzle flash, impact burst.

**Explicitly UNSET, so nobody reports the feel as finished:** rumble, the taut-line loop,
the hit sound, and `FireMontage`. A grapple with no rope loop and no hit sound is a
silent pull.

**Every remaining box is a rung nobody has climbed, and they stay `[ ]`:**

- the PIE host proof — pulled at ~1800uu/s, detach at arrival, jump mid-pull keeps
  momentum, a sky shot costs NO cooldown, a wall-blocked claim logs REFUSED. This is the
  cheapest one and it is the one that would tell us whether the whiff-costs-nothing design
  actually holds;
- the Gantry reachable by grapple from the mid deck. **Related evidence exists and must
  not be mistaken for this box**: AIB19 got BOTS grappling live (5 ACTIVATED / 6 REFUSED)
  with roughly half of attempts falling short of the standoff point — an open finding on
  route geometry. If that is geometry rather than bot aim, a human firing at the same
  anchors will fall short too. Read AIB19's finding before running this row;
- multi-process PIE with `-PktLag=120 -PktLoss=5`, reported as "editor multi-process" and
  never as rung 4;
- threes at rung 5, including the corpse-never-keeps-flying cancel path.
