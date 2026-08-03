# UI structure — the single reference for where things live

**Status:** v1, 2 Aug 2026. Verified against disk on that date. Trees and tables only; if you
want the narrative, read `ASSET-PIPELINE.md` and `SCREEN-MANIFEST.md`.

Binding law: `CLAUDE.md` laws 3/5/7, `R18`/`R26`, `BREACHPOINT-AUTHORING-MATRIX.md`.

---

## 1. The two trees

✅ = on disk today · ⬜ = declared here, not yet built.

### `Source/Breachpoint/UI/` — one level of nesting, flat inside each

```
Source/Breachpoint/UI/
├── ✅ BRUIManagerSubsystem.h/.cpp     CommonUI layer stack (GameHUD/Menu/Modal)
├── ✅ BRActivatableWidget.h/.cpp      the one widget base
├── ✅ BRRootLayout.h/.cpp             UBRRootLayout
├── ✅ BRHUDLayout.h/.cpp              UBRHUDLayout + UBRKillfeedEntryWidget
├── ✅ BRViewModels.h/.cpp             UBRVM_Combat + UBRVM_Match (merged pair)
├── ✅ BRUISettings.h                  UBRUISettings (soft widget-class refs)
├── ✅ BRUITypes.h/.cpp                shared UI enums/structs
├── ⬜ Styles/                         UBRUIStyles — CommonUI styles as C++ classes (§6)
├── ⬜ Components/                     UBR<Name> component classes, flat
├── ⬜ Screens/                        UBRScreen_/UBRPanel_/UBRModal_<Name>, flat
└── ⬜ ViewModels/                     UBRVM_<Domain>, flat
```

`BREACHPOINT-ARCHITECTURE.md:266` declares `### 3.9 UI/ — 4`, listing only the first four pairs
flat. **The four subfolders are not yet declared in §3** — declaring them is owed before the
first class lands, or `Tools/architect/architect.py` `undeclared_files()` (`:232-253`) reports
every new file as architecture drift.

### `Content/UI/` — assets only

```
Content/UI/
├── ✅ Components/     WBP_<Name>, one per Source/Breachpoint/UI/Components/ class
├── ✅ Screens/        WBP_Screen_* · WBP_Panel_* · WBP_Modal_*
├── ✅ Icons/          ~98 glyphs · T_UI_Icon_<Name> · LOAD-BEARING NAME, see §7
├── ✅ Art/            ~30 rank+medal marks · ~12 plates · 3 weapon silhouettes
├── ✅ Materials/      M_UI_<Effect> — gradients, scanlines, radial fills (~4)
├── ✅ Fonts/          Rajdhani + Roboto Condensed (both OFL)
├── ⛔ Styles/         DELIBERATELY DOES NOT EXIST — §6
├── ✅ WBP_RootLayout.uasset       ← at the root; belongs in Components/
├── ✅ WBP_HUDLayout.uasset        ← at the root; belongs in Components/
└── ✅ WBP_KillfeedEntry.uasset    ← at the root; belongs in Components/
```

Folders carry a `.gitkeep` (git tracks no empty directory, and UE shows no Content folder until
an asset lands) and a `README.md` stating what belongs, what never belongs, and the naming
pattern.

**Three pre-existing assets sit at `Content/UI/` root, not in a subfolder.** Moving them is an
editor operation on binaries someone else owns — not done here. Note also that
`WBP_KillfeedEntry` fails the §2 derivation: its parent is `UBRKillfeedEntryWidget`
(`Source/Breachpoint/UI/BRHUDLayout.h:14`), which derives to `WBP_KillfeedEntryWidget`. Both are
recorded, neither is fixed here.

---

## 2. The naming law, stated mechanically

> **A WBP's name is its C++ class name with `UBR` stripped and `WBP_` prefixed.**

```
UBRNavBar              -> WBP_NavBar
UBRScreen_FrontEnd     -> WBP_Screen_FrontEnd
UBRPanel_TextChat      -> WBP_Panel_TextChat
UBRModal_PlayerInspect -> WBP_Modal_PlayerInspect
```

The derivation is not cosmetic: it is total and reversible, so **a script can assert every WBP's
parent class from its filename alone** — no asset load, no editor. That is the only mechanical
review a binary WBP admits (R18: "binary assets are invisible to the critic"). A WBP whose name
does not derive from its parent breaks the one check we have.

Screen / Panel / Modal spellings are `SCREEN-MANIFEST.md:122-124`.

| Kind | Name | Where |
|---|---|---|
| Widget asset | `WBP_<CppClassWithoutUBR>` | `Content/UI/Components/`, `Content/UI/Screens/` |
| Texture | `T_UI_<Family>_<Name>` | `Icons/` (Icon) · `Art/` (Rank, Medal, Plate, Weapon) |
| Material | `M_UI_<Effect>` | `Content/UI/Materials/` |
| Font face | `FF_<Family>_<Weight>` | `Content/UI/Fonts/` |
| BP default container | `BP_<CppClassWithoutPrefix>` | R26 condition 5 — not a widget |

Texture/material spellings: `ASSET-PIPELINE.md:81-83`.

---

## 3. Why ONE level of nesting, and not the 18-folder split in `TICKETS.md` §2(a)

**The previously-published justification was wrong, and this section replaces it.**
`TICKETS.md` §2(a) proposes `Components/{Core,Chrome,Rail,Grid,Leaf,Bespoke,HUD,Forge}/` and
`Screens/{HUD,OV,FE,MM,OP,PR,SH,ST,FG,PGCR}/`, on the ground that law 5 enforces `owner_path` at
folder granularity so packets sharing a folder collide. The supporting claim — that a 2-deep unit
*cannot be declared* in `BREACHPOINT-ARCHITECTURE.md` §3 — is **false**, verified adversarially:

- `Tools/architect/architect.py:162` — `name = u.group(1).split("/")[-1]`, i.e. a §3 unit written
  `Components/BRNavBar.h/.cpp` parses to the unit name `BRNavBar`, folder prefix discarded.
- `Tools/architect/architect.py:214-215` — `classify()` resolves it with
  `root.rglob(f"{unit}.h")` from `SRC/<folder>`, which matches at **any** depth.

So a 2-deep unit is declarable today, bare, with no tooling change. The real reasons are these
three:

**(a) Precedent — nothing in the module is 2-deep.** `AbilitySystem/` is the largest folder in
the module at **30 files** and has exactly one level: `Abilities/`, `Cues/`, `Effects/`, with
seven pairs sitting flat at its root. If 30 files need one level, a UI layer does not need
eighteen folders.

**(b) Law 5 does not need folders.** `.claude/hooks/guard_laws.py:73` accepts **exact-file**
`owner_path` entries:

```python
if not any(rel.startswith(o.rstrip("/") + "/") or rel == o for o in owners):
```

`rel == o` is the whole point — a claim may name `Source/Breachpoint/UI/Components/BRNavBar.h`
and the hook confines the packet to that file. This is the same device R23 uses for
`BRGameplayTags.h/.cpp` and R25 uses for one spec file per packet in `Source/Breachpoint/Tests/`.
Concurrency is bought with file-granular claims, not with folder splits. `TICKETS.md` §2(a)'s
premise — "`guard_laws.py` enforces `owner_path` at *folder* granularity" — is contradicted by
the line above.

**(c) Legibility.** One flat `Components/` is one `ls` and one grep. Eighteen folders means every
agent guesses which of eight families a new component belongs to, and guesses differently.

---

## 4. Where does X go?

| I need to create… | Folder | Prefix | C++ or asset | Tier |
|---|---|---|---|---|
| A reusable widget's behaviour, bindings, state | `Source/Breachpoint/UI/Components/` | `UBR<Name>` | C++ | 1 |
| That widget's layout | `Content/UI/Components/` | `WBP_<Name>` | asset | 4 (UMG layout) |
| A screen / panel / modal's behaviour | `Source/Breachpoint/UI/Screens/` | `UBRScreen_`/`UBRPanel_`/`UBRModal_<Name>` | C++ | 1 |
| That screen's layout | `Content/UI/Screens/` | `WBP_Screen_<Name>` etc. | asset | 4 (UMG layout) |
| Data a widget reads (no polling) | `Source/Breachpoint/UI/ViewModels/` | `UBRVM_<Domain>` | C++ | 1 |
| Colours, type ramp, text/button styles | `Source/Breachpoint/UI/Styles/` | `UBRUIStyle_*` | C++ | 1 |
| A tuning number of any kind | `Content/Data/*.csv` + row struct in `Source/Breachpoint/Data/BRDataRows.h` | — | text data | 2 |
| A glyph UMG cannot draw | `Content/UI/Icons/` | `T_UI_Icon_<Name>` | asset | 4 (sourced art) |
| A rank/medal mark, plate, weapon render | `Content/UI/Art/` | `T_UI_Rank_`/`Medal_`/`Plate_`/`Weapon_<Name>` | asset | 4 (sourced art) |
| A gradient, scanline, glow, radial fill | `Content/UI/Materials/` | `M_UI_<Effect>` | asset | 4 (material graph) |
| A typeface | `Content/UI/Fonts/` | `F_`/`FF_<Family>` | asset | 4 (sourced art) |
| A panel, rule, border, solid fill, or text | **nowhere** — `Border`/`Image`/`TextBlock` in the WBP | — | **zero assets** | — |
| A soft asset reference for a widget class | `UBRUISettings` / `Config/DefaultGame.ini` | — | C++/ini | 1–2 |
| Anything not on this table | it does not get an asset | — | answer: which tier, and if Tier 4, why can't C++ express it? | — |

Tier definitions: `BREACHPOINT-AUTHORING-MATRIX.md` §2. Tier 4 is a **closed list** (R18); UMG
layout, materials and sourced art are the only three rows this table can reach.

---

## 5. The rule that keeps the asset count near zero

> **Export nothing UMG can draw** — `ASSET-PIPELINE.md:15`.

The design language is flat panels, radius 0, 1px/0.5px/2px strokes, solid fills and uppercase
text. UMG draws all of it natively: a panel is a `Border` with a solid brush, a rule is an
`Image` 1px tall, a tab is a `Border` + `TextBlock`. **Zero assets.**

Files exist only for these six, ~147 in total (`ASSET-PIPELINE.md:23-31`):

| Category | Count | Why it must be a file |
|---|---|---|
| Icons / glyphs | ~98 | Shapes UMG cannot draw |
| Rank + medal marks | ~30 | Art |
| Scene plates / backgrounds | ~12 | Photography or rendered art |
| Weapon silhouettes | 3 | Rendered from the meshes |
| Materials | ~4 | Gradients, scanlines, radial fills |
| Fonts | 1–2 | Type |

A texture for a divider or a panel background is the defect this rule exists to prevent: it is
unreviewable, it does not recolour with the palette, and it does not scale.

---

## 6. `Content/UI/Styles/` deliberately does not exist

A CommonUI style asset (`CommonButtonStyle`, `CommonTextStyle`, `CommonBorderStyle`) is a
**Blueprint class parented to an ENGINE class**. Test it against R26's five conditions
(`docs/DESIGN-RULINGS.md:283-307`):

- **Condition 1** — "a direct child of a `BR`-prefixed C++ class": FAILS. Its parent is
  `UCommonButtonStyle`, not a `BR` class.
- **Condition 5** — "named `BP_<CppClassWithoutPrefix>`": FAILS. There is no `BR` C++ class to
  name it after.

Failing R26 means it is not an exception to R18, so R18 applies in full: **zero Blueprint
classes.** Styles are C++ classes in `Source/Breachpoint/UI/Styles/` — subclass the CommonUI
style type in C++, set defaults in the constructor from `UBRUISettings`, and the palette stays
greppable, diffable and reviewable. That is the whole reason (R18: "binary assets are invisible
to the critic — no diff, no merge, no grep").

---

## 7. `Content/UI/Icons/` is a load-bearing folder name

`Tools/verify_notices.py:49-51` hard-codes the glob:

```python
DEPENDENCY_PROBES: list[tuple[str, str, str]] = [
    ("Content/UI/Icons/**/*.uasset", "Lucide", "icons are in Content/ but no Lucide notice"),
]
```

`main()` (`:132-135`) fails the run when the glob matches anything and `Lucide` is absent from
`THIRD-PARTY-NOTICES.md`. Lucide is ISC; ISC requires the notice in all copies, so the check also
demands the staged copy `Content/Legal/THIRD-PARTY-NOTICES.txt` match (`:98-116`) and
`Config/DefaultGame.ini` stage `Content/Legal` (`:118-129`). It runs in rung 2.

**An icon imported anywhere else does not fail this check — it passes it, silently, because the
glob matched nothing.** That is the failure mode, and it is why the folder name is not a
preference.

**Open conflict:** `ASSET-PIPELINE.md:90` (§4) directs importers to `Content/UI/Textures/<Family>/`,
which the probe cannot see. **Ticket BP63** resolves it. Until then, icons go in
`Content/UI/Icons/`.
