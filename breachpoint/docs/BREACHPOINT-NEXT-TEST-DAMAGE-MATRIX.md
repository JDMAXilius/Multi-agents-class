# TEST — every damage source, proven in one session

**Cut:** 17 August 2026 by the cloud lead · founder's goal: *"make sure we're able to do damage
with every firing weapon, and with every GA as well — the melee, the grenade."*

Filter the log to **`LogBN`**. Every line named below is automatic; nothing here needs a console
command (there are none any more — see DIAGNOSTICS §2).

## 0. Before you shoot anything — read the three startup lines

These decide whether the test is even meaningful, and two of them are new:

| Line | Must say | If it doesn't |
|---|---|---|
| `BNHit: <pawn> is hittable — mesh blocks WeaponTrace and MeleeTrace` | present, per pawn | **`BNHit: … CANNOT BE SHOT`** = the Blueprint pawn out-serialised the C++ collision defaults. Every bullet and swing passes through every player. Fix the Blueprint's mesh collision before testing anything else — nothing else can pass while this fails |
| `BNLoadout: 'Rifle' — fire: 20 x1 = 20/shot (head x2.0) \| melee: 40 dmg at 120uu \| mag 30` | one per weapon, four total | a weapon missing = its row didn't load; `fire: NONE` = that row has no AbilitySet and cannot shoot |
| `BNLink: … [template original]` | — | (aim/pose only, not damage) |

Expected loadout table, from the rows as landed:

| Weapon | Fire | Melee |
|---|---|---|
| Rifle | 20/shot, head ×2 | 40 |
| Pistol | per row | 40 |
| Shotgun | **12 × 6 pellets = 72/shot** | 40 |
| Knife | **NONE — by design**, melee-only row | **100** at 150uu |

## 1. The five sources, and the one line each must produce

Every source ends at the same door, so the proof is always the same line:

```
BNDamage: <instigator> -> <victim>, <amount> | shield 0 -> 0 | health 100 -> <n>
```

| # | Source | How | Also expect |
|---|---|---|---|
| 1 | **Rifle fire** | shoot a player | `BNGA_Fire: validated hit — … at <point>, <n> uu` then `BNDamage: … 20` (40 on a headshot) |
| 2 | **Pistol fire** | swap (`X`/wheel), shoot | same pair, pistol's row damage |
| 3 | **Shotgun fire** | swap, shoot once, up close | **up to six** `validated hit` + `BNDamage` pairs from ONE trigger pull — that is correct, one per pellet. At range, fewer pellets connect and the damage falls off naturally |
| 4 | **Melee (GA)** | `V`, any weapon | `BNGA_Melee: swing — montage …` then `BNGA_Melee: validated connect — … for <n>` then `BNDamage`. With the **knife** equipped it must read **100** |
| 5 | **Grenade (GA)** | `G`, land it at their feet | `BNProjectile: blast — <target> for <n> at <d>uu` **per target**, then `BNDamage` per target. 90 at the centre, falling off linearly from 150uu to 500uu |

## 2. The three failure shapes, and what each one means

- **`BNInput: Input.X -> REFUSED`** — the ability never ran. Reason follows on the ability's own
  line (dead, cost, cooldown, sprint, `bCanADS`). Nothing downstream is at fault.
- **A `validated hit` with no `BNDamage`** — the door refused. Only two ways: the target has no
  ASC (shooting a prop), or the row's damage is zero (the loadout audit would have said so).
- **No `validated hit` at all, but you clearly hit them** — the server's re-trace disagreed with
  the client's claim. Either the collision check in §0 is failing, or the shot genuinely missed on
  the server (lag). This is the one that used to be invisible and now has §0 in front of it.

## 3. Multiplayer, which is the rung that counts (honesty ladder §6)

Damage is the server's alone, so every line above is a **server-log** line. In a two-window PIE:

- **Client shoots host** — the `BNDamage` line appears in the HOST's log only; the client sees
  health drop through replication. A client-side `BNDamage: REFUSED … on a non-authority machine`
  is a real bug and means something bypassed the door.
- **Host shoots client** — same, from the other side.
- **Grenade from a client** — the projectile is the server's; the blast lines are the server's.
- **Melee across a weapon swap** — must still work; melee is granted by the PlayerState, not the
  weapon, so swapping cannot revoke it.

## 3b. The two damage rules, and how to turn them on

Both landed 17 Aug **inert** — every weapon behaves exactly as it did before them until a row
says otherwise. They live inside `BNDamage::ApplyWeaponDamage`, so no ability was touched.

**Body sections** (Zorans' shape). Every bone resolves to head / torso / arm / leg by substring,
so `neck_01`, `spine_03` and `upperarm_l` all land correctly without being listed. **Neck counts
as head** — the convention everywhere that has the distinction. An unrecognised bone resolves to
torso, never to a free hit. Row columns: `HeadshotMultiplier` (already 2.0 and unchanged),
`TorsoMultiplier`, `ArmMultiplier`, `LegMultiplier` — the last three default to 1.0.
*To tune:* set e.g. `LegMultiplier = 0.75` on the Rifle row. Nothing else.

**Distance falloff** (Lyra's concept, three numbers instead of a curve asset):
`FalloffStartDistance` (full damage out to here), `FalloffEndDistance`, `FalloffMinMultiplier`.
Disabled while `End <= Start`, which is the default 0/0. Distance is measured from the shot's own
origin, not the shooter's feet.
*To tune:* Shotgun `Start = 500`, `End = 2000`, `Min = 0.3` gives it a real close-range identity.

**To see either working:** raise the log to Verbose for `LogBN` and every landed bullet prints
its own arithmetic —

```
BNDamage: 20.0 base x2.00 head (bone 'head') x1.00 falloff @ 412uu = 40.0
```

If a number ever looks wrong, that line names which of the three factors produced it. And a
`bone 'None'` in it means the trace resolved on something with no skeleton — headshots could
never fire in that state.

## 4. Known, stated rather than hidden

- **Shotgun magazine is 30** — the weapon CDO carries no magazine property, so the C++ default
  stands. A 6-pellet pump with 30 rounds is placeholder data, not a bug; one row edit fixes it
  when you want it fixed.
- **Shields are off** (`MaxShield = 0`), by your ruling — every `BNDamage` line reads
  `shield 0 -> 0` and all damage lands on health. Turning shields back on is one number.
- **The grenade does not damage through cover** (`bRequireLineOfSight=True`) and its LOS trace
  runs on Visibility, which players deliberately do not block — so a teammate's body never shields
  you from a blast, but a wall does. That is the intended shape.
