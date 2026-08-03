# TICKET — BP69: The HUD art is scoped to a bigger game than the sim ships

> STATUS: open — cut 3 Aug 2026 from the Figma export pass. Found by enumerating
> `HUD / Elements` (`6:48`, 61 nodes) and diffing it against `Content/Data/DT_Weapons.csv`
> and `Source/`. **Nothing here is a defect.** Art is allowed to lead. What is a defect is
> the silence: nobody has recorded that it leads, so the next person to see a Sniper reticle
> will reasonably assume a Sniper exists.

## The three divergences, measured

### 1. Weapons — 6 in art, 3 in data

| Weapon | HUD icon `SET Weapon /` | Reticle `SET Reticle /` | `DT_Weapons.csv` row |
|---|:--:|:--:|:--:|
| Assault Rifle | ✅ `78:2` | ✅ `62:5` | ✅ `AR` |
| Magnum | ✅ `78:6` | ✅ `62:17` | ✅ `Magnum` |
| Rocket | ✅ `78:10` | ❌ **none** | ✅ `Rocket` |
| Battle Rifle | ✅ `78:4` | ✅ `62:12` | ❌ |
| Sniper | ✅ `78:8` | ✅ `62:20` | ❌ |
| Shotgun | ✅ `78:12` | ✅ `62:26` | ❌ |

Three weapons have full HUD art and no data row. **The Rocket — which DOES ship — is the one
with no reticle**, and it is the power weapon the whole rocket-countdown system exists for.
That asymmetry is the most likely to bite.

### 2. Abilities — 6 in art, 1 in the sim

`SET Ability /` ships Ready + Cooling pairs for **Grapple, Repulsor, Threat, Drop Wall,
Thruster, Overshield** (12 nodes, 50.7×30 each). A grep of `Source/Breachpoint/` for
`Repulsor|DropWall|Thruster|Overshield` returns **nothing**. Only the grapple exists
(`BRGA_*`, and `UBRVM_Combat` carries `GrappleCooldownDuration`/`bGrappleReady`).

### 3. Grenades — 4 in art, 0 named in the sim

`SET Grenade /` ships **Frag, Plasma, Spike, Dynamo**, selected and unselected (8 nodes).
A grep for `Dynamo|Spike` in `Source/` returns nothing. `UBRVM_Combat` has a bare
`GrenadeCount` with no type axis at all.

### 4. Motion tracker — art exists, feed does not

`HUD / Motion Tracker` (`24:2`, 133×133) and `SET Minimap /` Clear · Contacts · Disabled
(`62:54/65/79`, 140×138) are authored. `UI-DESIGN-SYSTEM.md` §5 records the founder putting
the tracker **in scope**, and **nothing in `Source/` produces its data** — already filed as
BP65. This ticket adds the measurement: the art is done, so BP65 is the only blocker.

## Ordering law

Step 1 is a founder call and gates everything else. Steps 2–3 are recording, and can run
immediately.

## Kickoff

- requires: `files-only`
- `Content/Data/DT_Weapons.csv` still has exactly 3 rows (verify — the data crew may have
  added some)
- Figma page `6:48` still holds the nodes above
- owner_path: `docs/`, `docs/tickets/`

## Steps

1. **Rule the scope, per family.** For weapons, abilities and grenades, each item is one of:
   **(a) in the vertical slice** — needs a data row / C++ producer and a ticket;
   **(b) post-slice, art-first** — legitimate, record it and do not build the feed;
   **(c) cut** — remove the art so it stops implying a feature.
   This is a design call. Do not let it be settled by whoever next opens the HUD.
2. **Record the ruling** in `docs/DESIGN-RULINGS.md` and reflect it in
   `docs/UI-DESIGN-SYSTEM.md` §5, which currently describes neither the six abilities nor the
   four grenade types.
3. **Fix the Rocket reticle gap** — either author `SET Reticle / Rocket`, or record that the
   Rocket deliberately reuses another reticle and say which. It is the only shipped weapon
   with no reticle of its own.
4. **Only after step 1:** file the C++ gaps for whatever landed in bucket (a). Do not file
   them speculatively — six abilities and four grenade types would be a large phantom backlog.

## Done when

- [ ] Every weapon, ability and grenade in `HUD / Elements` is assigned to (a), (b) or (c),
      recorded in the rulings ledger
- [ ] `UI-DESIGN-SYSTEM.md` §5 matches the ruling
- [ ] The Rocket reticle question is answered either way
- [ ] Any bucket-(a) item has a real ticket; bucket-(b) items have none, deliberately

## Notes

- **Art leading the sim is normal and often correct** — you cannot design a weapon tray
  around three weapons and then add three more without redrawing it. The cost is only paid
  when it goes unrecorded.
- The export pipeline does not care: these all export cleanly as vector. This ticket is about
  what the icons *mean*, not whether they extract.
- `UBRVM_Combat` has no grenade-type or ability-type axis. If step 1 puts either in the
  slice, that is a ViewModel change and therefore a C++ gap, not a widget change.

## Log

**3 Aug 2026 — cut.** Found while enumerating `HUD / Elements` for export. The page lists 61
nodes; the divergence surfaced from diffing its component names against `DT_Weapons.csv`
(3 rows: AR, Magnum, Rocket) and grepping `Source/Breachpoint/` for the ability and grenade
names. Nothing was changed — this is a recording ticket.
