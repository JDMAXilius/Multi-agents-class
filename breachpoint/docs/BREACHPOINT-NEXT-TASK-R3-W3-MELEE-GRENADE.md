# HANDOFF — melee + grenade: the six editor edits

**Cut:** 13 August 2026 by the cloud lead · **For:** the founder (or the terminal, if asked)
**Read [`BREACHPOINT-NEXT-ASSET-RULES.md`](BREACHPOINT-NEXT-ASSET-RULES.md) first** — §1 (reuse,
never author) and §5 (do only what is listed) govern this.

The C++ is landed and complete. **Nothing below creates an asset** — every asset named already
exists. Six edits to three existing assets, then melee and grenade are live.

## 1. Input — four rows, two assets, zero new files

Both InputActions already exist and are reused in place (§2: existing assets are referenced, never
copied into `/Game/BN/`).

| Asset | Add |
|---|---|
| `DA_BNInput` | row: tag `Input.Melee` → `/Game/Input/Actions/IA_Melee` |
| `DA_BNInput` | row: tag `Input.Grenade` → `/Game/Input/Actions/IA_Grenade` |
| `IMC_BNNext` | mapping: `IA_Melee` → **V** (the reference used F; V avoids the crouch row) |
| `IMC_BNNext` | mapping: `IA_Grenade` → **G** (the reference's key, `MyCharacter.cpp:856`) |

Until these exist, both bindings log *"that control is dead"* every run. That is the announcement
being loud on purpose — the abilities are granted and reachable the moment the rows land.

## 2. `DT_BNWeapons` — two columns, two rows

Melee reads its animation from the current weapon's row. Both montages already exist under
`/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/Actions/`.

| Row | `MeleeMontage` |
|---|---|
| `Rifle` | `AM_MM_Rifle_Melee` |
| `Pistol` | `AM_MM_Pistol_Melee` |

**Watch the soft-ref trap the DT ticket already paid for:** through the MCP, `set_rows` silently
drops `{"refPath":…}` objects on soft-pointer columns — plain soft-path strings land, objects read
back `None`. In the editor by hand this does not apply.

`MeleeDamage` (40) and `MeleeRange` (120, the reference's measured `MeleeTraceDistance`) carry C++
defaults and need no edit unless you want to tune them.

**Optional, same pass:** `FireSound` — `MSS_Weapons_Rifle2_Fire` and `MSS_Weapons_Pistol_Fire`.
Not required; the muzzle cue already falls back to the rifle shot from ini, so the weapon is
audible either way. Filling these is what makes the pistol sound like a pistol.

## What is C++ and needs nothing here

Per ASSET-RULES §3: `UBNGA_Melee` and `UBNGA_Grenade` are C++ classes, granted in
`ABNPlayerState::GrantDefaults`. They are **not** in either `DA_BNAbilitySet` and must not be added
— two grants means two specs.

**The grenade actor is `ABNProjectile`, a C++ class — NOT `BP_FPST_Grenade`.** That changed after
the first pass, on purity law 3: *"the engine damage API is BANNED … Radial = our own overlap query
→ per-target GE application."* A template Blueprint grenade explodes through `ApplyRadialDamage`,
which would have been a second damage pipeline bypassing attributes, shields and death — damage
`BNDamage` never logged and GAS never saw. `ABNProjectile` does the contract's own prescription
instead, and still wears the template's art (`SM_grenade`, `NS_Grenade_Trail`,
`NS_Grenade_Explosion`, `MSS_Explosions_Grenade`), all soft-referenced from `DefaultGame.ini`.

Nothing in the editor is needed for the grenade at all — no Blueprint, no asset. Its tuning
(fuse 3s, 90 damage, 150/500 radii, line-of-sight on) is ini.

## How to test

**Stage A, standalone PIE:**
- **V** — the weapon swings. The connect lands in the montage's `AN_FPST_Melee` notify window, not
  on the press, so damage arrives a beat after the key. Impact FX and a decal appear on what you hit.
- **G** — a grenade leaves the hand ~0.2s after the press, flies where you were **looking**,
  bounces, and detonates on a 3s fuse with FX and a bang.
- Log lines to expect: `BNGA_Melee: validated connect — … for 40`, `BNGA_Grenade: … threw …`,
  and one `BNProjectile: blast — <target> for <damage> at <distance>uu` **per target hit**.
- **Stand behind cover next to a grenade** — the blast should not reach you. Line-of-sight is on.

**Stage B, listen server + client — this is the one that counts:**
- Melee from the CLIENT: the host must see the swing animation and the victim must lose health on
  the **server's** judgement. A connect the server's own trace does not confirm is rejected
  silently (by design — that is the wallhack close).
- Grenade from the CLIENT: the projectile is the server's alone, so it appears on both windows
  from one spawn and bounces the same way on both. If it appears on only one, that is a finding.
- Grenade damage on the CLIENT: every point of blast damage is the server's. Throw one at the host
  from a client window and watch the host's health — and the `BNProjectile: blast` line, which
  should appear on the **server's** log only.
- Melee across a **weapon swap**: it must still work, because it is granted by the PlayerState,
  not by the weapon.

## Known gaps, stated rather than hidden

- **No throw animation.** `ThrowMontage` is unset — the template's throw montage has not been
  identified. The grenade throws correctly with no arm animation. Name the asset and it goes in
  `DefaultGame.ini`; no code change.
- **Grenade is `ServerOnly`, so its throw has ~½RTT of input latency on a client.** Deliberate: a
  client-predicted projectile diverges from the server's on the first bounce with nothing to
  reconcile the two.
- **Unarmed melee is a no-op**, because the row supplies the numbers and there is no unarmed row.
- **Melee has no whoosh sound.** `sfx_MeleeWhoosh_nl_meta_Preset` exists and is the obvious
  candidate, but the cue has no field for it — that is C++, and it belongs to the cue-asset
  migration rather than to this handoff.
