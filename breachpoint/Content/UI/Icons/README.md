# `Content/UI/Icons/` — LOAD-BEARING FOLDER NAME

~98 glyphs (shapes UMG cannot draw). Naming: `T_UI_Icon_<Name>`
(`docs/ui/ue-frontend/ASSET-PIPELINE.md:81`).

## Why the folder name is law, not taste

`Tools/verify_notices.py:49-51` hard-codes this glob as a dependency probe:

```python
DEPENDENCY_PROBES: list[tuple[str, str, str]] = [
    ("Content/UI/Icons/**/*.uasset", "Lucide", "icons are in Content/ but no Lucide notice"),
]
```

`main()` (`:132-135`) fails the run when the glob matches **anything** and the token `Lucide` is
absent from `THIRD-PARTY-NOTICES.md`. The Lucide set is ISC; ISC requires the copyright and
permission notice in **all copies**, so the obligation is discharged by the notice *shipping*,
not by existing. `verify_notices.py` runs in rung 2 (`docs/contracts/testing.md`).

**Therefore: an icon imported anywhere other than `Content/UI/Icons/` silently escapes the
licence gate.** The check does not fail — it reports OK, because the glob matched nothing. That
is the exact failure mode this folder exists to prevent. Import icons here. Only here.

## Known conflict

`docs/ui/ue-frontend/ASSET-PIPELINE.md:90` (§4) tells importers to drop PNGs into
`Content/UI/Textures/<Family>/`. That path is **not** covered by the probe glob. Ticket **BP63**
resolves the contradiction; until it lands, `Content/UI/Icons/` wins for icons, because it is the
one the gate can see.

## Never here

- Rank/medal marks, scene plates, weapon renders → `Content/UI/Art/`.
- Anything UMG can draw (rules, dividers, panel fills, text) — `ASSET-PIPELINE.md:15`,
  *export nothing UMG can draw*.
- A new icon set without adding its notice to `THIRD-PARTY-NOTICES.md` **and** re-copying
  `Content/Legal/THIRD-PARTY-NOTICES.txt` (`verify_notices.py:98-116` fails on drift).
