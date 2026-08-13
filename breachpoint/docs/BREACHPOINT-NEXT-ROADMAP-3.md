# BREACHPOINT NEXT — Roadmap 3: Aim, Impact and Melee

**Cut:** 13 August 2026 · **Binds to:** the NEXT doc family only —
[ROADMAP-1](BREACHPOINT-NEXT-ROADMAP-1.md)'s **Operating rules** and
[ASSET-RULES](BREACHPOINT-NEXT-ASSET-RULES.md) govern this roadmap unchanged.

## The one-line goal

**The weapon points where you look, the shot is visible and leaves a mark, and you can hit
someone with the gun** — all of it replicated, all of it through GAS, all of it reusing the
template's existing content.

## What R2 proved and what it left

R2 shipped weapons in hand, switching, sprint, fire, reload, death and respawn — founder-verified
in the editor. The founder's playtest named exactly three gaps, and they are this roadmap:

| Observed | Root cause | Goal |
|---|---|---|
| **The gun is static** — it does not follow the look, and other players cannot see where you aim | The ABP's aim is per-bone spine rotation driven by `SetAimAndLeanInfo`, the output of the procedural component R2 deferred. `AimPitch`/`AimYaw` are published but nothing consumes them into the pose | **G1** |
| **No visible bullet, no bullet hole** | The tracer cue is unset because the template's tracer wants `User.ImpactPositions[]` + `User.Trigger`, not the single `BeamEnd` vector the R2 ticket specified — a wrong spec, not a missing asset. No impact decal exists at all | **G2** |
| **No melee** | Never built | **G3** |

Plus one carried item: **sounds**. The cue classes have no sound field, so nothing the weapons do
is audible. Folded into G2.

---

## G1 — Aim: the weapon points where you look

*The largest piece, and the one the founder asked for first.*

`MyCharacter` routes this through `BPC_FPST_Procedural_AimAndLean` and the `SetAimAndLeanInfo`
interface message. **BN builds the computation in C++** and publishes to the anim instance
through the established snapshot→publish path — no Blueprint component, no interface message.

| # | Task |
|---|---|
| 1.1 | `Animation/BNProceduralAim.{h,cpp}` (or a section of `BNAnimInstance` if it is small enough — the builder decides and justifies): compute the aim rotation and the per-bone spine distribution |
| 1.2 | **Source the aim from replicated state.** `GetBaseAimRotation()` already carries the owner's view on every machine (`RemoteViewPitch` for remotes), so other players seeing where you aim is free — *if* nothing reads locally-controlled-only state. This is the multiplayer requirement the founder named |
| 1.3 | Per-bone weights: `ABP_Mannequin_Base` carries `aimSpineWeights_UE5` (8 bones) and `_UE4` (5). **Mine the record for the real names and distribution** — `BRAnimLayerInstance.h` documents why a map keyed by bone beats eight positional floats |
| 1.4 | Publish through the two-stage discipline: game-thread snapshot, `NativeThreadSafeUpdateAnimation` the sole writer. The four asset-enum properties and the `bNativeOwnsTurnState` gate stay untouched |
| 1.5 | **Lean lands here too.** R2 built lean's abilities and tags but found nowhere to publish them (`LeanRotation`/`LeanOppRotation` are written only by the same procedural component). Same computation, same publish path — this is why lean was deferred rather than hacked |

**Objective (Checkpoint M):** the weapon and upper body follow the look, up/down and side to side; a second window sees the same aim on your character; lean visibly tilts.

## G2 — The visible shot: tracer, impact, decal, sound

| # | Task |
|---|---|
| 2.1 | **Fix the tracer cue to the template's actual contract:** `User.ImpactPositions[]` (array) + `User.Trigger`, read from the template's own FireEffect graph. My R2 spec said `BeamEnd`; it was wrong |
| 2.2 | **Impact decal — the bullet hole.** `MyCharacter::ImpactEffect` is the reference: surface-typed impact FX plus a decal. New cue class, template decal/material assets |
| 2.3 | **Sound.** Cue classes gain a soft sound field alongside the FX field, config-filled from the template's existing weapon sounds. **Author nothing** — per ASSET-RULES §1 |
| 2.4 | All of it through GameplayCues so every machine sees and hears it, including simulated proxies. The impact cue fires from the **server's** validated hit (R2 Wave 4 made that authoritative) — never the client's claim |

**Objective (Checkpoint N):** firing shows a tracer from muzzle to impact, leaves a bullet hole, and is audible — on every machine, at the place the server agrees the shot landed.

## G3 — Melee as an ability

| # | Task |
|---|---|
| 3.1 | `AbilitySystem/Abilities/BNGA_Melee.{h,cpp}` — `LocalPredicted`, input tag `Input.Melee`, plays the melee montage from the weapon row |
| 3.2 | **The trace is driven by the montage notify, not the input event.** `MyCharacter::MeleeTrace` + `HandleMontageNotifyBegin` is the reference, and the record notes `AN_FPST_Melee` is the one notify the template actually ships — so reuse it |
| 3.3 | Damage through the one door (`BNDamage`), server-validated like fire. No second damage path |
| 3.4 | Row gains a `MeleeMontage` soft ref and melee damage; the melee trace distance is data, not a literal |

**Objective (Checkpoint O):** melee swings, connects in the notify window, damages the target server-side, and is visible on both windows.

---

## Waves

| Wave | Goal | Ends at |
|---|---|---|
| 1 | G1 aim + lean publish | **Checkpoint M** |
| 2 | G2 tracer, decal, sound | **Checkpoint N** |
| 3 | G3 melee | **Checkpoint O** |

## Carried debts and open items

| # | Item | From |
|---|---|---|
| D1 | **`MaxWalkSpeed` is not in the CMC's saved-move state** — sprint start/stop puts the owning client ~15–30cm ahead for ~1 RTT. Converges; the cure is a saved-move compressed flag (`cmc-prediction` skill) and deserves its own packet | R2 W3 critic |
| D2 | `ABP_BNMannequin` is dead and should be deleted (E2) | R1 |
| D3 | `FPSTemplate/` is no longer read-only in practice; `BP_FPSCharacter` inherits `UBNAnimInstance` and was never exercised (E1) | R1 |
| D4 | Graph-clear day: clear `ABP_Mannequin_Base`'s event graph **and** flip `bNativeOwnsTurnState`, one atomic change (E3/E4) | R1 |
| D5 | The rate floor can drop honest fire claims queued by a client hitch; `CommitAbility` spends shot 0's round even if no claim follows | R2 W4 critic |
| D6 | The whole spine is founder-verified **standalone only** — listen+client is still owed for everything | R1/R2 |

## Still deferred beyond R3

ADS (needs G1's aim to exist first — it is the same machinery with a camera FOV blend), the rest
of the procedural layer (recoil, sway/lag, pose offsets), the real damage pipeline, projectiles,
pickups, grenades, the grapple, UI.
