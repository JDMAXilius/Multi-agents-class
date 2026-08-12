# BreachpointNext — the reworked framework tree

**Structure and manifest only.** Directories and `.gitkeep` markers — no source files, no
`.Build.cs`, no entry in `Breachpoint.uproject`. UBT does not see it, and `Source/Breachpoint/`
builds exactly as before.

The complete file-by-file manifest, the reasoning behind every collapse, and the three open
decisions are in **[`docs/BREACHPOINT-NEXT-STRUCTURE.md`](../../docs/BREACHPOINT-NEXT-STRUCTURE.md)**.
Read it before adding the first file.

**62 units · 119 files · 15 top-level folders · 8 subfolders** — against 272 files in the
module it replaces.

```
Core  Data  Input  AbilitySystem  Characters  Actors  Weapons  Animation
AI  UI  Match  Online  Interfaces  Utilities  Tests
```

## Before you add a file

Five principles decide it, in this order:

1. **Fewer files, straight to the point.** A class that is one function is a function. A folder
   holding one file is a naming problem. Several `UCLASS` may share a header when they are small,
   related, and always change together.
2. **Modularity where reuse is real** — when the second caller exists, not in anticipation of one.
3. **One home per concept.** Plausible in two folders means the structure is wrong.
4. **Group by domain, never by base class.** No `Subsystems/`, no `Managers/`, no root
   `Components/`. A subsystem lives with the thing it serves.
5. **The shape encodes the laws.** One damage door. One row-struct header. No `Tick/`. No
   `Blueprints/`. One pawn for players and bots.

Check the *not created* and *four folders removed* tables in the structure doc before adding a
folder — several plausible-looking ones are absent on purpose.

**This will change.** The manifest is a target, not a contract. When code proves a better shape,
change it — and record why in the structure doc.
