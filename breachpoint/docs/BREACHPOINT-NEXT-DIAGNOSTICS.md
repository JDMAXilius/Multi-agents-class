# THE DIAGNOSTIC SHEET — what BN tells you, and how to read it

**Cut:** 14 August 2026 by the cloud lead, on the founder's standing order: *"I want to see logs
on the stuff that we are experiencing that is happening wrong."*

Everything here is `LogBN`. Filter the output window to that category and the whole game reports
itself. **Nothing here fires per frame** — announcements are on edges and failures — except the
one live trace, which is opt-in and off by default.

## 1. What announces itself automatically, with no command typed

| Prefix | When | What it settles |
|---|---|---|
| `BNLink:` | every time the character links a weapon's anim layer | WHICH layer class, by full path, and **`[template original]` vs `[BN DUPLICATE]`**. A duplicate also prints a WARNING naming it as the frozen-aim root cause |
| `BNLayers:` | when the linked-layer set changes, and once at startup even if nothing linked | Whether layers are connected at all, which ones, and **which aim properties each one ACCEPTS vs is MISSING**, by name |
| `BNInput:` | every input-driven ability press | `ADS/melee/grenade/... -> ACTIVATED` or `REFUSED`. Covers every ability at once |
| `BNGA_ADS:` | ADS press, activate, end | The refusal REASON (sprinting / no row / `bCanADS` false), whether the tag and speed GE applied, and whether it ended by release or was CANCELLED (descoped by damage or sprint) |
| `BNPose:` | ADS tag flips, both directions | Whether `ChangePose` reached the PoseOffsets component — the gun-rises-to-the-eye half of ADS, separate from the FOV half |
| `BNGA_Melee:` | every swing | No weapon row / montage unset / montage refused to play (no ABP slot) / swing OK with montage name and length |
| `BNDamage:` | every point of damage | instigator → victim, amount, shield and health before/after |
| `BNCues:` | startup | which cue class won each tag |
| `BNEquipmentComponent:` | startup, per weapon | a startup row that does not exist in the table is named and skipped |

**The three lines that mean "you found the bug":**
- `[BN DUPLICATE]` — a stale layer copy. §5b of ASSET-RULES.
- `MISSING: PitchRotator ...` — the aim value is being written into a layer that has no such
  property; the pose cannot move through it.
- `NO linked anim layers` — nothing is linked at all; aim, ADS and weapon poses are all dead.

## 2. Commands, when you want to ask rather than wait

| Command | Does |
|---|---|
| `BNAimDebug` | One-shot: the whole aim chain on one line — owner (`NATIVE`/`COMPONENTS`), BaseAimPitch → Pitch → PitchRotator, axes, `bFPSMode`, lean — **plus one line per linked layer showing what that layer actually holds** |
| `BNAimLog 1` / `0` | Streams that same state twice a second so the chain can be WATCHED while moving the view. Off by default |
| `BNLayerCheck` | Re-prints the full link report on demand |
| `BNAimNative 1` / `0` | Flips who owns the aim surface, live, to A/B both paths in one session |
| `BNAimAxis 0\|1\|2` · `BNLeanAxis 0\|1\|2` | Bone-space axis (Roll/Pitch/Yaw) for aim and lean — measured, not derivable |
| `BNMelee` | Activates melee BY CLASS, skipping the input assets. Swings here but not on V = the input row/mapping; dead both ways = the ability or the row |
| `BNDamageSelf [n]` · `BNKillSelf` · `BNRefill` | Drive the damage/death/respawn chain from the console |

## 3. How to read a frozen-aim session in under a minute

1. Look at the startup lines. `BNLink` + `BNLayers` tell you what is linked and whether it can
   receive aim at all. **Most aim bugs die right here.**
2. Move the view, run `BNAimDebug`.
   - Main line's `PitchRotator` at zero → the value is not being produced (upstream).
   - Main line live, layer lines zero/missing → the value is produced but not consumed (the
     layer — duplicate, or a layer that does not model aim).
   - Both live and the body still still → the layer's ModifyBone chain or the axis. That is the
     only case that needs an editor investigation, and by then everything else is excluded.
3. Hold aim: `BNGA_ADS: ACTIVE` plus `BNPose: ChangePose SENT` = both halves of ADS fired. FOV
   without `BNPose` = the pose component is missing.
4. Press V: `BNGA_Melee` names the dead link, or `BNInput: Input.Melee -> REFUSED`, or nothing
   at all (which means the key never reached the ASC — an input-asset problem).

## 4. The rule these came from

ASSET-RULES §5c: anything that hands a value across a boundary it cannot verify must announce
the outcome, name the counterpart, and fail loudly. Every entry above exists because a real bug
once hid behind that boundary in silence.
