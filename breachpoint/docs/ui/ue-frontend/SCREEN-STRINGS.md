# Screen strings — the Halo→Breachpoint mapping for the two rendered screens

**Status:** v1, 3 Aug 2026. Cut from a founder render of the main menu that shows **Halo
Infinite's content in Breachpoint's layout** — the structure is ours, every string on it is not.

`ART-PASS-STAGE-3.md` §4 holds the general mapping (1,561 occurrences, 53% decision-free). **This
file is the per-screen application of it**, so the art pass has an exact target on the two
screens that exist rather than a lexicon to apply by judgement.

**Every replacement below is already authorised** by `ART-PROMPT-LIBRARY.md`, `DT_Weapons.csv`
or `BRGameplayTags.h`. Nothing here is a new decision except where marked **DECIDE**.

---

## 1. Main menu — every visible string

### 1.1 Navigation tabs

| On screen | → | Authority |
|---|---|---|
| `PLAY` | **`PLAY`** | keep — generic, not Halo-owned |
| `CREATE` | **`FORGE`** *(DECIDE)* | Forge Editor exists in our component library; if the slice ships no editor, **cut the tab entirely** rather than ship a dead one |
| `COMMUNITY` | **`CAREER`** | our progression surface. There is no community feature in the slice |
| `SHOP` | **cut** | no storefront in the vertical slice. A tab that opens nothing is worse than a missing tab |

> **Recommendation: ship two tabs — `PLAY` and `CAREER`.** The nav bar is built for a variable
> tab count, and four tabs where two work is three screens nobody built. `SCREEN-MANIFEST.md`
> already sorts Store/Forge into waves 5 and 7.

### 1.2 The menu list

| On screen | → | Authority |
|---|---|---|
| `CAMPAIGN` | **`HOST MATCH`** | `online-services.md` — Steam listen server, invite-first. There is no campaign |
| `MATCHMAKING` | **`JOIN BY INVITE`** | same. Quickmatch is explicitly cut |
| `FIREFIGHT` | **`TRAINING`** | bots-vs-you against `DT_BotTuning`; `ART-PASS-STAGE-3.md` maps Firefight → LAST STAND, but that is a *mode* name and this is a menu verb |
| `ACADEMY` | **`SETTINGS`** | the fourth row a player expects |

### 1.3 Feature card, tagline, rank

| On screen | → | Authority |
|---|---|---|
| `2 NEW ARENA MAPS!` | **`BREACH ARENA`** | one map ships. `arena_manifest.json` |
| *"Unravel the mysteries of Zeta Halo"* | **"Fight for control of the breach."** | our tagline, `ASSET-METHODS.md` |
| `CAREER RANK` | **`SERVICE RECORD`** | avoids Halo's exact panel name; same function |
| `SERGEANT GRADE 1` | **`SERGEANT`** | rank 5 of our 16-rank ladder, `ART-PROMPT-LIBRARY.md:355`. **Drop "Grade N"** — our ladder has no grades |
| **UNSC emblem** in the rank panel | **`T_Logo_Mark`** | `ART-PROMPT-LIBRARY.md:1391`. This is **art, not a string**, and it is the single most Halo-owned pixel on the screen |

### 1.4 Roster gamertags

Straight from `ART-PASS-STAGE-3.md` §4. `Paddy Thibau → Rowan Mercer` **has already landed** in
the profile bar; the rest have not.

| On screen | → |
|---|---|
| `Tyrelli47` | **`Kestrel47`** |
| `Project ZEB` | **`Project ECHO`** |
| `Snipy117` | **`Longshot117`** |
| `Strayff` | **`Driftline`** |
| `McSticko` | **`Talonpike`** |
| `The Traya` | **`Harrier`** |
| `Rowan Mercer` | ✅ **already correct** |

### 1.5 Chrome

| On screen | → |
|---|---|
| `IN MENUS` · `Invite Only` | keep both — generic session states |
| `Menu` (bottom-left prompt) | keep |
| `LB` / `RB` / `X` bumper glyphs | keep — platform glyphs, not Halo's |

---

## 2. HUD — every visible string

The HUD is **much cleaner** than the menu; most of it is numerals and our own callouts.

| On screen | Verdict |
|---|---|
| `ASSAULT RIFLE` | ✅ **correct** — `DT_Weapons.csv` row `AR`, `DisplayName` |
| `36 | 108` | ✅ mag / reserve |
| `38 | 12:00 | 31` | ✅ score · clock · score |
| `20 m` · `CANAL` | ✅ **ours** — tracker range and an `arena_manifest.json` callout |
| `42 m` | ✅ waypoint distance |
| `E  ACTIVATE TERMINAL` | ⚠️ **DECIDE** — no terminal exists in the slice. Either it is placeholder text (fine, mark it) or it names a feature we are not building. `ACTIVATE` alone is safer |
| `Kilner · Vance · Ortiz · Ash · Rook` | ✅ **ours** — not in the Halo gamertag set. Keep |
| `YOU` in white | ✅ **correct and deliberate** — Infinite's convention, do not "fix" it |

**Nothing on the HUD is Halo-owned.** The art pass does not need to touch this screen; it needs
to touch the menu.

---

## 3. Where these strings live, and why that decides who applies them

| String class | Lives in | Applied by |
|---|---|---|
| Menu rows, tabs, panel titles | The WBP's `CommonTextBlock` defaults | `wbp_plan.py` — text is a plan node key |
| Weapon names | `DT_Weapons.csv` `DisplayName` | already correct |
| Callouts | `arena_manifest.json` | already correct |
| Rank names | The 16-rank ladder table | BP-progression, not a UI packet |
| Gamertags | Sample data in the Figma file **and** whatever the roster ViewModel returns | Figma via the art pass; runtime via `UBRVM_Lobby` when it exists |

**The gamertags are the trap.** They appear twice — as design-time sample data in Figma, and as
runtime values once a lobby ViewModel exists. Renaming them in Figma alone leaves the *rendered
game* showing whatever the ViewModel returns. **Both, or neither is done.**

---

## 4. What this does not cover

The **art**. Strings are the easy half. The ODST Spartan, the UNSC insignia, the Zeta Halo
backdrop and the rank chevrons are Halo's, and no mapping table replaces them — that is
`ART-PROMPT-LIBRARY.md`'s families A–E and it is the long pole. This file exists so the *text*
half is not also a judgement call.
