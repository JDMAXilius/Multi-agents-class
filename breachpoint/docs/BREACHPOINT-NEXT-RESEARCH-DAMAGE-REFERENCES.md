# RESEARCH — how the references do damage, and where BN stands against them

**Cut:** 17 August 2026 by the cloud lead, on the founder's question: *"double check from the
references that we have — the damage pipeline they have, so we have an idea how to do it
correctly and making sure we are doing it correctly."*

Read directly from source in `references projects/`. Verdict up front: **BN's pipeline is the
right shape.** It matches the two GAS references on every structural decision. What it lacks are
three FEATURES the shipped references carry, one of which is a 4v4 blocker.

## 1. The five patterns found

| Project | How damage is applied | Verdict |
|---|---|---|
| **Lyra** | GE with an **ExecutionCalculation** (`ULyraDamageExecution`) → `Damage` meta attribute → `PostGameplayEffectExecute` converts to −Health | The benchmark |
| **OnSight** | GE with an ExecutionCalculation reading a **SetByCaller** → `Damage` meta attribute → PostGEE | Same shape, simpler magnitude |
| **Zorans** | GE with **SetByCaller**, exec calc present but a **stub** (reads the value, does nothing) | Same shape, exec unused |
| **ShooterCore** | `UGameplayStatics::ApplyDamage` — the engine damage API | Not applicable (single-player, no GAS) |
| **NewMoons** | `ApplyDamage` + **`ApplyRadialDamage`** for grenades | The exact API our law 2 bans |

**Two of five use the engine damage API — and those two are the two without GAS.** Every project
that has GAS routes damage through a GameplayEffect into a meta attribute. That is what BN does.
The ban in our contract is not our invention; it is what the GAS references actually practise.

## 2. BN vs the GAS references, decision by decision

| Decision | Lyra | OnSight | Zorans | **BN** |
|---|---|---|---|---|
| Damage lands via a GE | ✔ | ✔ | ✔ | ✔ |
| Meta attribute drained in `PostGameplayEffectExecute` | `Damage` | `Damage` | ✔ | `IncomingDamage` ✔ |
| Server-only application | `#if WITH_SERVER_CODE` | exec = server | ✔ | authority check + **loud refusal** ✔ (stricter) |
| Instigator carried in the effect context | ✔ | ✔ | ✔ | ✔ |
| Hit result carried in the context | ✔ | ✔ | ✔ | ✔ |
| One entry point for all damage | via the exec | via the exec | scattered call sites | **one namespace, `BNDamage`** ✔ (tighter than all three) |
| Death from the drain, not a separate path | `OnOutOfHealth` | `Event_Death` | health comp | `OnDeath` → death GA ✔ |
| Positional damage multiplier | physical material tags | — | **bone→section map** | head bone × row multiplier ✔ (simplest of the three) |
| Distance falloff | **curve per weapon** | — | — | **MISSING** (grenade only) |
| Team / friendly-fire gate | **`CanCauseDamage`** | **`GetAttitude` == Friendly → return** | — | **MISSING** ← the 4v4 blocker |
| Client-claim validation | — | — | — | **BN only** (server re-trace + bounds + hits-first) |

Two rows deserve calling out:

- **BN validates the client's claim; none of the references do.** Lyra trusts its own targeting;
  Zorans trusts the client's hit result outright. Our fire and melee re-trace on the server and
  reject a claim the server's own trace does not confirm. That is stricter than the benchmark,
  and it is the right call for a competitive shooter.
- **BN has exactly one door.** Lyra's equivalent is the execution calc, but Zorans applies damage
  from at least three different call sites with duplicated context-building. Ours is one
  namespace with two functions, which is why the whole pipeline can be re-shaped without touching
  a single caller.

## 3. The three things the references have that BN does not

### 3a. Friendly fire — PARKED by founder ruling, 17 Aug 2026

> **FOUNDER RULING: no teams. Matchmaking targets FREE-FOR-ALL, and only free-for-all.**
> *"Right now we don't need to worry about teams… we're just gonna be focusing on free-for-all.
> Keep it simple for now. Have in mind that we're gonna be making it later, but right now I don't
> want you to focus on that."*
>
> **What this means for the pipeline: nothing is missing.** In free-for-all every player is a
> legal target for every other player, which is exactly what `BNDamage` does today. The gap
> below is not a gap under FFA — it is a feature the mode does not have.
>
> **When teams arrive**, the work is one gate inside `BNDamage::ApplyDamage` — never at the call
> sites — and self-damage stays exempt. All five damage sources are covered by that one edit.
> Nothing else in the pipeline moves. Recorded here so the next reader does not mistake FFA for
> an oversight, and does not start sprinkling team checks through the abilities.

The reference detail, kept for the day it matters:

Both team-based references gate damage on team attitude, and they do it **inside the damage
calculation**, not at the call sites:
- Lyra: `TeamSubsystem->CanCauseDamage(EffectCauser, HitActor)` → multiplier `1.0` or `0.0`.
- OnSight TDM: `GetAttitude(...) == Friendly → return` before any output modifier, with
  **self-damage deliberately excluded from the gate** so a player can still hurt themselves.

BN has no teams yet, so nothing is wrong today — but the moment 4v4 exists, every rifle in the
game shoots teammates. The correct home is **inside `BNDamage::ApplyDamage`**: one check, all five
damage sources covered at once, no caller edited. Self-damage must be exempt (our own
`BNKillSelf`/grenade-at-your-feet paths depend on it).

### 3b. Distance falloff — a curve per weapon, not a number

Lyra evaluates `DistanceDamageFalloff` (a rich curve) per shot. BN's rifle does full damage at
1 metre and at 100. Our grenade already does linear falloff, so the concept exists in the codebase;
the weapon side needs a row column and one multiply behind the door.

### 3c. Positional damage beyond the head

Zorans maps every bone to a section (head/torso/limb) with per-section multipliers; Lyra uses
tagged physical materials, which also lets ARMOUR carry a multiplier. BN checks one bone name
(`head`) against one row multiplier. That is honest and works, but it cannot express "legs take
less", and it breaks silently if the skeleton's head bone is ever renamed. Not urgent; worth
knowing the ceiling.

## 4. What BN should NOT copy

- **`ApplyRadialDamage` (NewMoons)** — one call that damages everything in a sphere with no LOS,
  no per-target control, no GAS involvement. Our grenade's own overlap + per-target GE is more
  code and strictly more correct.
- **Zorans' unreliable client trust** — it takes the client's `FHitResult` and applies damage from
  it. That is a wallhack's dream in a competitive game.
- **Lyra's execution calc, for now.** It earns its complexity when there are captured attributes to
  combine (armour, buffs, resistances). BN has one number from a row. When mitigation arrives, the
  exec calc is the right destination — and BN reaches it by rewriting the inside of `BNDamage`,
  which is exactly what that door was built for.

## 5. Verdict

**The pipeline is correct.** It matches every structural decision the GAS references make, and it
is stricter than all of them on the two things that matter most in a competitive shooter: one
damage entry point, and server validation of every client claim.

**Under the free-for-all ruling (17 Aug), the pipeline is FEATURE-COMPLETE.** Everyone may damage
everyone, which is what it already does. The remaining two items are tuning, not correctness:
distance falloff and per-bone sections, in that order, whenever the shooting stops feeling right.
Friendly fire is parked with teams (§3a).

None of the three changes the pipeline's shape — all are edits *inside* the door, which is the
whole reason the door exists.
