# Adding `User.Team_Color` to an FX system — the two-minute edit

The code side is **already done and shipped**. Every weapon cue resolves an ally/threat colour
and writes it to its Niagara system on every shot. The systems just don't declare a parameter
to receive it, and Niagara drops an undeclared USER parameter with no warning — which is why
this looked like a code bug for weeks and wasn't one.

So there is **no code change in this task**. Add the parameter, and the tint that is already
being written starts landing.

## Which systems, and what each one buys you

Six cues, but only **six systems**, and they are not equally worth your time:

| System | Lights up | Worth it? |
|---|---|---|
| `NS_WeaponFire_MuzzleFlash_Rifle` | MuzzleFlash + GrappleFire | **Start here** — every shot, most visible |
| `NS_WeaponFire_Tracer` | Tracer + GrappleRope | **Start here** — reads at range, the clearest team signal |
| `NS_ImpactConcrete` | Impact + GrappleHit + the concrete surface row | Yes — the default impact |
| `NS_ImpactSparksCharacter2` | hits on PLAYERS | Yes — arguably the most useful of all: it tells you whose shot connected |
| `NS_ImpactGlass` | glass surface row | Only for completeness |
| `NS_ImpactDecals` | the impact decal | Skip — a coloured bullet hole reads as a bug, not a team |

Doing just the first two gets most of the value.

## The edit

Do this once per system. **Open `NS_Grenade_Explosion` in a second tab first** — it already has
this exactly right, and copying a working reference beats following prose.

1. Open the system. In the **Parameters** panel find the **User Exposed** section.
2. `+` → **Color** → name it exactly `Team_Color`. Niagara shows it as `User.Team_Color`.
3. Set its default to **white, `(1,1,1,1)`**. This is not cosmetic: white is the identity value
   for a multiply, so a system whose colour nobody sets still looks precisely as it does today.
   A black or transparent default makes every effect vanish in FFA and in the frames before a
   team id replicates.
4. Wire it into the emitter's colour **as a multiply, not a replace**. In the emitter's
   **Particle Spawn** stack, add a **Scale Color** module, then bind its Scale input to
   `User.Team_Color` (the input's dropdown → *Link Inputs* → `User.Team_Color`).
   - **Multiply**, because these effects are not white — the muzzle flash is a hot yellow, the
     sparks are orange. Replacing their colour throws away the art; scaling it keeps the shape
     and shifts the hue.
   - If a system has several emitters (smoke + flash + sparks), scale the ones you want tinted.
     Smoke usually reads better tinted; a white-hot core usually reads better left alone.
5. Save.

## Then verify, without guessing

```bash
python Tools/bn/80_team_audit.py        # editor open
```

The row flips from `!!` to `OK` the moment the parameter exists. That is the whole check —
no rebuild, no code edit, no restart of anything but the editor's own asset save.

To see it live, run with `-LogCmds="LogBN Verbose"` and watch for `BNCue:` lines; each one
names the colour it resolved and whether it had a component to write to.

## If you name it something else

Fine — the name is a contract with the ASSET, not with the code, so tell the code instead.
In `Config/DefaultGame.ini`, under that cue's section:

```ini
[/Script/BreachpointNext.BNGameplayCue_MuzzleFlash]
TintParameter=User.MyColorName
```

## One thing to decide per system

Your own weapon fire is currently **not** tinted, by design — `bTintOwnEffects` defaults to
false, because tinting the thing firing from your own hands every frame is noise rather than
information. The grenade blast opts in (`bTintOwnEffects=True`) because you are nearly always
watching your own explosion.

If you want your own tracers coloured too once the parameter exists, add to the same section:

```ini
bTintOwnEffects=True
```
