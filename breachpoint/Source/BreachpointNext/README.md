# BreachpointNext — the reworked framework tree

**Structure only.** This folder contains directories and `.gitkeep` markers — no source files,
no `.Build.cs`, no entry in `Breachpoint.uproject`. UBT does not see it, and
`Source/Breachpoint/` builds exactly as it did before.

The full visual structure, the ownership table, the deliberate omissions, and the four open
decisions are in **[`docs/BREACHPOINT-NEXT-STRUCTURE.md`](../../docs/BREACHPOINT-NEXT-STRUCTURE.md)**.
Read it before adding the first file.

## The short version

19 top-level folders, one per concept, each a real `owner_path`:

```
Core  Data  Input  AbilitySystem  Characters  Actors  Weapons  Animation  AI  UI
Match  Subsystems  Online  Audio  Telemetry  Interfaces  Utilities  Tests  Python
```

Three rules govern where a file goes:

1. **One home per concept.** If it could live in two folders, the structure is wrong.
2. **A folder is an ownership boundary** — it maps to a discipline (D1–D8) so packets can
   name it and `guard_laws.py` can enforce it.
3. **The shape encodes the laws.** One damage folder (law 2). One row-struct folder (law 3).
   No `Tick/` anything (law 4). No `Blueprints/` (R18).

Before creating a folder that is not here, check the *deliberate omissions* table in the
structure doc — several plausible-looking folders are excluded on purpose.
