# Hit reactions — a modular GA, driven by where and how hard you were hit

**Cut:** 13 August 2026 by the cloud lead, at the founder's request: *"modular hit reaction…
gameplay ability… based on the point you're actually hitting… aware of the direction… advanced
programming, multiplayer native, GAS purity, best practices."*

---

## 1. What the assets actually support — read before designing, not after

The founder is right that everything needed ships already. The precise inventory decides the shape:

| Direction | Light | Medium | Heavy |
|---|---|---|---|
| Front | ×4 (`AM_MM_HitReact_Front_Lgt_01..04`) | ×2 | ×1 |
| Back | ×1 | ×1 | — |
| Left | ×1 | ×1 | — |
| Right | ×1 | ×1 | — |

Thirteen montages, all under `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/Actions/`
(UE4-mannequin twins excluded per ASSET-RULES §1b).

Two conclusions fall straight out of that table:

1. **The template's hit-react language is DIRECTION × SEVERITY, not body part.** There is no
   "left arm" montage — there is "hit from the left". The per-limb half of the founder's ask
   (left arm twitches when the left arm is shot) is not a montage problem at all; it is
   **per-bone physical animation** (an impulse on the hit bone with a physics blend), which is a
   different mechanism with real multiplayer cost. **Phase 2, with its trigger recorded below.**
   Phase 1 is the montage layer, and the montage layer is what the assets fund.
2. **Variety and fallback are data problems.** Front-Light has four variants (the most common hit
   in an FPS — you are usually facing your attacker); Back/Left/Right have no Heavy. So the data
   model is *a list of montages per (direction, severity) cell*, picked from at random, with
   severity stepping DOWN (Heavy→Medium→Light) when a cell is empty.

**The bone is already in the pipeline.** `BNDamage::ApplyWeaponDamage` reads `Hit.BoneName ==
"head"` for the headshot rule, and `FBNLastDamage.Hit` (last packet) carries the full hit —
BoneName included — refreshed on every landed hit. Phase 1 logs the bone on every reaction, which
is both the groundwork for phase 2 and a live check that traces actually return bones in PIE.

## 2. The trigger — why this is NOT a gameplay event, and not a cue either

**Where does "you were hit, react" come from?** Three candidates, judged:

| Route | Verdict |
|---|---|
| **Gameplay event from `PostGameplayEffectExecute`** (`Event.Hit` → triggered GA) | The textbook answer, and rejected for a reason this codebase already owns: ability trigger tags are registered in the **CDO constructor**, and BN's standing rule — proven three times over — is that native tags are NOT guaranteed registered while CDOs are built. Every trigger-shaped thing in BN already routes around this (`UBNGE_State` carries tags on the spec, cooldown tags build on first use, `UBNGA_Death` activates **by class**). A trigger tag would be the one exception, betting on load order. |
| **A GameplayCue** | Law 6 says cues carry cosmetics, and a flinch is cosmetic — but the founder asked for a GA, and the GA is the right call for a better reason: a hit reaction is about to grow gameplay consequences (stagger, aim punch, interrupt rules), and those belong in an ability that can hold tags and be blocked by `State.Dead`. A cue can never graduate. |
| **Activated BY CLASS from `UBNHealthComponent`** | **Chosen.** It is exactly death's shape, one delegate over: the health component already watches Health on every machine, already gates on authority, and already activates `UBNGA_Death` by class at zero. Hit react is the same pattern at *not-zero*: health went DOWN and stayed above zero → `TryActivateAbilityByClass(UBNGA_HitReact)`. No new mechanism, no tag-in-CDO bet, no second reaction point. |

The chain, whole:

```
damage → PostGEE (authority): capture FBNLastDamage, drain            (already built)
       → Health delegate fires: NewValue < OldValue
            ├─ NewValue == 0 → UBNGA_Death (by class)                 (already built)
            └─ NewValue  > 0 → UBNGA_HitReact (by class)              ← NEW, same shape
                 ├─ read LastDamage off the attribute set (hit, amount, bone)
                 ├─ DIRECTION: incoming shot vector, unrotated into the victim's
                 │             frame → Front/Back/Left/Right
                 ├─ SEVERITY: damage amount vs the set's thresholds → Lgt/Med/Hvy
                 ├─ MONTAGE: UBNHitReactionSet row (random among variants,
                 │           severity steps down when a cell is empty)
                 └─ ASC->PlayMontage → replicates to the server's view and
                    every simulated proxy
```

**Direction math, written down so it is reviewable:** the incoming vector is
`ImpactPoint − TraceStart` for traced hits (fire, melee); the grenade's hand-built hit carries no
trace, and its `ImpactNormal` already IS the blast's travel direction, so it is used as-is.
Unrotate into the victim's frame; the dominant axis picks the montage: incoming travelling
*backward* relative to the victim (−X) means the shot came from the front → **Front**; +X →
**Back**; incoming travelling toward +Y (rightward) means the attacker stood to the left →
**Left**; −Y → **Right**.

## 3. Multiplayer — who sees what, stated honestly

- **Server + every watching client: yes.** The GA is ServerOnly; `ASC->PlayMontage` replicates the
  montage to simulated proxies. One activation, every observer flinches the same montage — the
  server picks the variant, so there is no per-machine disagreement.
- **The victim's own first-person screen: deliberately NO, phase 1.** Montage replication is
  `COND_SkipOwner` — the same fact that was a *bug* for the grenade throw is a **feature** here:
  BN is true first-person with the camera on the head bone, so playing a full-body flinch on your
  own body snaps your own camera — that is aim punch, a tuning decision, not a default. The
  victim's own hit feedback is the HUD's damage-direction indicator (UI roadmap; the seam —
  direction, amount — is already captured). If the founder wants camera punch, it is a small cue
  later, tuned separately from the third-person flinch.
- **No prediction, no new replication, no RPC.** The victim cannot predict being shot; nothing new
  replicates — the montage channel already existed.

## 4. Purity — checked against the laws

- Activation is an **ability** (law 2's entry point), granted in `GrantDefaults` beside melee and
  grenade (a body verb — survives weapon swaps), blocked when dead by the base class's
  `State.Dead` check like every other verb.
- It **mutates nothing**: no attribute, no GE, no state tag in phase 1. A tag nothing reads is the
  overengineering the damage packet already declined; the moment stagger/interrupt rules exist,
  `State.HitReacting` joins with its first consumer (recorded trigger).
- The hit data travels in the **GE context → attribute-set capture** — the channel built last
  packet, used as designed; the attribute set still records, the GA still reads.
- All assets are **soft references on a DataAsset**, set in the editor by the terminal (§7's
  three-part rule) — nothing hard-references a montage.

## 5. The modularity the founder asked for — where it actually lives

**`UBNHitReactionSet` (a `UPrimaryDataAsset`)** is the grouping file: rows of
(direction, severity, montage list) plus the two severity thresholds. Swapping a character's whole
reaction personality is swapping one asset; adding a 5th direction montage is adding a row; no
C++ changes when animation changes. The GA holds one Config soft ref to it (ini fallback now, BP
child later per the migration packet).

## 6. Not built, with reopening triggers

| Deferred | Trigger |
|---|---|
| **Per-bone physical animation** (the left-arm twitch) | Founder asks for limb fidelity beyond the directional flinch. Mechanism: physics blend below the hit bone + impulse, restored on a timer — real cost, real tuning, own packet. The bone name is already captured and logged from day one |
| `State.HitReacting` tag | The first gameplay rule that reads it (stagger, interrupt) |
| Aim punch / owner camera feedback | Founder's call after feeling phase 1; it is a cue, tuned separately |
| HUD damage-direction indicator | The HUD roadmap; the data is already captured |
| Severity from damage TYPE (blast vs bullet) | Damage-type tags, which the damage packet deferred until something reads them — this would be that first reader if the founder wants blast-specific reactions |
