# TICKET — hit reactions: ONE DataAsset, thirteen rows, nothing else

> STATUS: done — mac terminal 14 Aug 2026. `DA_BNHitReactions` on `UBNHitReactionSet` at the
> exact ini path, nine rows read back; `DefaultGame.ini:411` names it.

**Cut:** 13 August 2026 by the cloud lead · **For:** the terminal session (editor + Unreal MCP)
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 and §7 first.**

## THE SCOPE, stated before the work because it has been a problem

This ticket creates **exactly one asset** and fills **exactly one property list on it**. It does
not touch `Source/`, does not touch any montage, does not touch `DT_BNWeapons`, does not touch any
other ticket's assets, does not "improve" anything it notices along the way. Extra work found is a
**Log entry, not a licence**. If any step cannot be done as written, STOP and hand it back — do not
substitute. The read-back is the deliverable.

## Step 1 — the one asset

Create a **DataAsset** of class **`UBNHitReactionSet`** (`/Script/BreachpointNext.BNHitReactionSet`)
at exactly:

```
/Game/BN/AbilitySystem/DA_BNHitReactions
```

The `/Game/BN/AbilitySystem/` folder does not exist yet — creating it is part of this step and is
the "grouping file" the founder asked for taking its place in the BN tree (it mirrors
`Source/BreachpointNext/AbilitySystem/`). `DefaultGame.ini` already points at this exact path; a
different name or folder lands nowhere and fails silently.

The module must be compiled past commit `ec48999`'s successor for the class to exist — if
`UBNHitReactionSet` is not in the class picker, the build is stale: **stop and report**, do not
pick a similar-looking class.

## Step 2 — thirteen rows

All montages live under `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/Actions/`.
Every name below is verified on disk. Use the UE5 set only — **no `UE4_` variants** (§1b).

`Rows` is an array of `FBNHitReactionRow {Direction, Severity, Montages[]}` — **nine rows**, the
Front cells carrying their variants as array entries:

| # | Direction | Severity | Montages (array, in this order) |
|---|---|---|---|
| 0 | Front | Light | `AM_MM_HitReact_Front_Lgt_01`, `_02`, `_03`, `_04` |
| 1 | Front | Medium | `AM_MM_HitReact_Front_Med_01`, `_02` |
| 2 | Front | Heavy | `AM_MM_HitReact_Front_Hvy_01` |
| 3 | Back | Light | `AM_MM_HitReact_Back_Lgt_01` |
| 4 | Back | Medium | `AM_MM_HitReact_Back_Med_01` |
| 5 | Left | Light | `AM_MM_HitReact_Left_Lgt_01` |
| 6 | Left | Medium | `AM_MM_HitReact_Left_Med_01` |
| 7 | Right | Light | `AM_MM_HitReact_Right_Lgt_01` |
| 8 | Right | Medium | `AM_MM_HitReact_Right_Med_01` |

There is deliberately no Back/Left/Right Heavy — the template ships none, and C++ steps severity
down on an empty cell. **Do not fill the gap with a Front montage**; the fallback is code's job.

`LightMaxDamage` (25) and `MediumMaxDamage` (50) keep their C++ defaults — touch neither.

**The soft-ref trap, again:** MCP property setters silently drop `{"refPath": …}` objects on
soft-pointer fields — they read back `None`. Plain soft-path strings land. This has now cost two
tickets; read every row back.

## Step 3 — read back

Reload the asset fresh. Print all nine rows — direction, severity, and each montage's resolved
path — plus the two thresholds. Intent vs actual, per cell.

## Done means

The asset exists at the exact path, nine rows read back correctly, thresholds untouched, and the
table pasted into the Log. **That is the whole ticket.**

## Log

_(terminal: the read-back table, and anything handed back)_

---

## For the founder — what this unlocks, and how to see it

With this asset and the K key (INPUT-WIRING ticket) in place, in a **two-window** PIE: shoot the
other player anywhere non-lethal and their body flinches **away from your shot** on your screen and
the host's — front hits rock backward, a shot from their left rocks them left. Sustained fire keeps
them rocking (retrigger). Your own first-person view deliberately does not flinch — that is the
HUD damage indicator's job later, and camera punch is a separate tuning decision, not a default.

Log line per reaction (Verbose): `BNGA_HitReact: <victim> hit from <dir> for <amount> (bone '<bone>')
-> <montage>`. **The bone name in that line matters**: if it ever reads `None`, traces are hitting
the capsule rather than the mesh — which would also mean headshots have never worked, and that is a
finding to report.

- 14 Aug 2026 (mac terminal) — **DONE.** `/Game/BN/AbilitySystem/` created;
  `DA_BNHitReactions` on `UBNHitReactionSet` at the exact ini path. Nine rows read back
  cell-perfect (Front L4/M2/H1, Back L1/M1, Left L1/M1, Right L1/M1 — the UE5 set, no UE4
  variants, no invented Heavy rows), thresholds untouched at 25/50. Whole read-back was
  string-path soft refs; nothing else touched.
