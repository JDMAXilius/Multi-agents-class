# Art pass — Stage 3: the nomenclature ledger, and the blocker under it

Figma file `yznvnVdOFDADaugZSeomfP`. **25,907 visible nodes swept** across 17 pages (the twelve
`FE / …` plus the five `HUD / …`), against a 130-term case-insensitive Halo lexicon, matching
**both layer names and text content**.

**Result: 1,561 Halo-owned occurrences — 1,141 in layer names, 420 in text.**

Stage 1 estimated this stage at "1–2 days, mechanical". It is neither, and the reason is §3.

---

## 1. Correcting stage 2

`ART-PASS-STAGE-2.md` §5 reported **31** Halo-owned strings. That number measured text content
only, on 12 pages, against a case-sensitive 50-term list. Three separate limits each hid work:

| Limit | Hidden |
|---|---|
| Layer names never scanned | **1,141 occurrences** — 73% of the total |
| Case-sensitive matching | 71 instances of `HALO` alone (matched `Halo`, missed `HALO`) |
| 12 pages, not 17 | the entire HUD, which contains `MA40 AR`, `Master Chief:`, `Spartan Cores`, `[ODST]`, `EAGLE` |

The 31 was not wrong about what it measured. It was wrong to be quoted as the size of stage 3.
Stage 2 §5 now carries this correction inline.

**Method note, so the next sweep does not repeat it:** substring matching on short terms produces
false positives. `reach` matches **B-reach-point** — our own wordmark, our own tagline
("Fight for control of the breach."), and every page header. Any term shorter than ~6 characters
needs a word-boundary test, not `indexOf`. The 1,561 figure has these excluded; a naive count
reads higher and is wrong.

---

## 2. The 1,561, by family

Families are how the rename executes — one decision per family, not one per node.

| Family | Layer | Text | Total | Leading terms |
|---|---:|---:|---:|---|
| **GAMERTAG** | 334 | 177 | **511** | `superintendent` ×151, `paddy thibau` ×110, `falcon` ×71, `project zeb` ×39 |
| **ARMOUR** | 195 | 40 | **235** | `anubis` ×78, `hammer time` ×37, `aureate midnight` ×36, `mark v` ×19 |
| **FACTION** | 131 | 30 | **161** | `banished` ×74, `odst` ×29, `spartan` ×28, `forerunner` ×14 |
| **MAP** | 102 | 45 | **147** | `salvation` ×43, `foundry` ×32, `liwatoni` ×23, `origin` ×15 |
| **BRAND** | 97 | 39 | **136** | `halo` ×95, `343` ×15, `infinite` ×10, `unsc` ×7 |
| **RANK** | 116 | 12 | **128** | `cadet` ×92, `hero` ×14, `bronze` ×11, `diamond` ×8 |
| **SEASON** | 61 | 18 | **79** | `heroes of reach` ×56, `echoes within` ×22 |
| **COMMEND** | 53 | 5 | **58** | `driving offensively` ×34, `eagle` ×24 |
| **MODE** | 21 | 32 | **53** | `slayer` ×18, `ctf` ×14, `team doubles` ×5 |
| **MAKER** | 14 | 9 | **23** | `lethbridge` ×14, `sevine` ×6, `tremonius` ×3 |
| **WEAPON** | 8 | 10 | **18** | `sidekick` ×4, `br75` ×3, `ma40` ×3 |
| **GEAR** | 9 | 3 | **12** | `repulsor` ×4, `spartan core` ×4 |
| | **1,141** | **420** | **1,561** | |

**~130 distinct terms produce 1,561 occurrences.** The unit of work is a mapping table with
~130 rows, applied by script. It is not 1,561 edits.

---

## 3. The blocker: we do not own the components the screens use

This is the finding that matters, and it is not about strings.

Of the 393 text nodes carrying Halo strings, **346 sit inside INSTANCES**. Tracing those
instances to their main components:

- They resolve to **22 distinct main components**.
- **227 of the 290 instance-borne strings resolve to components living on
  `Refences - Main Menu - Ideal`** — a *reference* page.

**The shipping screens are instances of reference-page components.** That is the expected
consequence of having built them with `node.clone()` — cloning an instance preserves its pointer
to the original main — but the consequence is structural, not cosmetic:

1. **A layer inside an instance cannot be renamed.** Instance children mirror their main. So
   most of the 1,141 layer renames are simply not performable where they currently sit.
2. **Editing the text creates a per-instance override**, 346 of them, each one a copy that
   silently diverges from its main and has to be maintained forever.
3. **The only place the fix belongs is the main component — which is on a reference page** the
   founder asked to preserve as reference. Editing it corrupts the reference *and* still leaves
   the shipping library owning nothing.

We already have a Breachpoint component library — **122 components** across `Buttons & Rows`,
`Panels & Cards`, `Roster & Player`, `Navigation`, `Titles & Headers`, `Items & Rarity`,
`Tables & Lists`, `Prompts & Glyphs`, `Progress & Feedback`, `Modals & Overlays`, `Forge Editor`.
**The screens are not using it.** Checking the 22 needed against what we own:

| | Count | |
|---|---:|---|
| Ours already, name-matched | **14** | `Profile Bar`, `File Detail`, `Gear Detail`, `Tag Frame`, `File Card`, `Popup Options`, `Store Card`, `File Title`, `Item Title`, `Highlight Button`, `Team Label`, `Warning Message`, + 2 fuzzy |
| Missing from our library | **8** | `Party List`, `Menu Combo`, `Menu in Border`, `Progression Button`, `Table Buttons`, `Navigation Bar`, `Load / Search Bar`, `Shop Passes Card` |

So the real prerequisite is small and well-shaped:

> **Stage 3a — take ownership of the component layer.** Author the 8 missing components on our
> own pages, then repoint every instance on the 17 shipping pages from the reference mains to
> ours. 22 components, not 1,561 nodes.

Once the screens instance *our* components, the nomenclature pass is a mapping table applied to
122 mains, and every screen updates for free. **Doing stage 3 before stage 3a means doing it
twice**, and leaving 346 overrides behind as interest.

Our own pages are not clean either — **132 distinct Halo-named nodes live on the Breachpoint
pages already**, including in the HUD (`MA40 AR`, `SET Weapon / Sidekick`, `Master Chief:`,
`View Upgrades to spend Spartan Cores`, `[ODST]`, `EAGLE`, `TEAM SLAYER`). Those are ours to fix
today and need no prerequisite.

---

## 4. The mapping — decision-free half

Grounded in names **already committed to the repo**. No new decisions; these can be scripted the
moment 3a lands (and the HUD ones today).

| Halo term | → Breachpoint | Authority |
|---|---|---|
| `Cadet` (rank 1) | `RECRUIT` | `ART-PROMPT-LIBRARY.md:355` — 16-rank ladder |
| `Bronze` `Silver` `Gold` `Platinum` `Diamond` `Onyx` `Hero` | the 16-rank ladder: RECRUIT · OPERATIVE · SPECIALIST · LEAD SPECIALIST · SERGEANT · STAFF SERGEANT · MASTER SERGEANT · SERGEANT MAJOR · WARRANT · LIEUTENANT · CAPTAIN · COMMANDER · BRIGADIER · VANGUARD · SPEARHEAD · **BREACHPOINT** | same |
| `Lethbridge Industrail` | `KESTREL DYNAMICS` | `ART-PROMPT-LIBRARY.md:1023` |
| `Sevine Arms` | `HALVORSEN ORDNANCE` | same |
| `Tremonius` | `MERIDIAN ARMOURWORKS` | same |
| *(unused makers)* | `SUTRO OPTICS`, `DRAYCOTT FIELD SYSTEMS`, `ORRIS POWERCELL` | same |
| `MA40` / `MA40 AR` / `BR75` / `Commando` | **Assault Rifle** (row `AR`) | `DT_Weapons.csv:2` |
| `Sidekick` | **Magnum** (row `Magnum`) | `DT_Weapons.csv:3` |
| *(rocket)* | **Rocket Launcher** (row `Rocket`) | `DT_Weapons.csv:4` |
| `Spartan Points` / `Spartan Cores` | `CREDITS` (soft) / `MARKS` (premium) | `ART-PROMPT-LIBRARY.md:1034` |
| `Oddball` | `RELIC` | `ART-PROMPT-LIBRARY.md:485` mode lexicon |
| `Strongholds` | `HOLDPOINTS` | same |
| `Last Spartan Standing` | `LAST STAND` | same |
| `Tactical Slayer` | `TACTICAL` | `ART-PROMPT-LIBRARY.md:544` gametype |
| `Rumble Pit` / `Team Doubles` | `SKIRMISH` | `ART-PROMPT-LIBRARY.md:485` |
| `Firefight` | `LAST STAND` | same |
| `Stockpile` | `STOCKPILE` *(already in our lexicon — keep)* | same |
| `Halo` / `Halo Infinite` / `Infinite` / `HALO 2` | `Breachpoint` | wordmark, `ASSET-METHODS.md:196` |
| `Xbox Profile` | `Steam Profile` | we ship on Steam — `CLAUDE.md` |
| `UNSC LOGO` | the Breachpoint faction mark, `T_Logo_Mark` | `ART-PROMPT-LIBRARY.md:1391` |
| `343` · `Master Chief` · `Cortana` · `Mjolnir` | **delete** — no equivalent, none wanted | law 7 |
| `Repulsor` `Drop Wall` `Overshield` `Active Camo` `Threat Sensor` | **delete** — Breachpoint has Grapple, Grenade, Melee only | `BRGameplayTags.h:7-20` |
| `Grappleshot` | **keep** — already a shipped Breachpoint string | `DT_Medals.csv:4` |

**Gamertags (511 occurrences, the largest family) are placeholder sample data**, not design. Eight
handles cover the file. Proposed, and free to change:

`Paddy Thibau` → **Rowan Mercer** · `Tyrelli47` → **Kestrel47** · `Strayff` → **Driftline** ·
`Snipy117` → **Longshot117** · `McSticko` → **Talonpike** · `Project ZEB` → **Project ECHO** ·
`Superintendent` → **Quartermaster** · `Falcon` → **Harrier**

That single table clears **GAMERTAG (511) + BRAND (136) + RANK (128) + MAKER (23) + WEAPON (18)
+ GEAR (12) = 828 of 1,561, or 53%**, with no decision required from anyone.

---

## 5. The mapping — decisions genuinely owed

These have **no Breachpoint equivalent anywhere in the repo**. The nomenclature agent confirmed
maps (beyond `BR_Arena01`), factions/teams and operators are empty categories. Each is one
decision covering many nodes.

| # | Decision | Clears | Seed available |
|---|---|---:|---|
| N1 | **Map roster + display names.** Only `BR_Arena01` exists and it has no human-readable name. | 147 | **Strong.** `Content/Data/arena_manifest.json` ships landmarks already spoken in VO: **The Core**, **The Gantry**, **Mezzanine Catwalks**, **North/South Barricade**, **East/West Stack**. "The Core" already reaches players via `DT_SpotterLines`. |
| N2 | **Armour-set and coating names.** | 235 | None. Six manufacturers exist to hang them off. |
| N3 | **Faction / team names.** Repo has deliberately avoided naming sides — tokens are literally `TeamThem`/`SelfWhite`. | 161 | None. |
| N4 | **Season names.** | 79 | None. |
| N5 | **Commendation names.** | 58 | Medal names in `DT_Medals.csv` are the tonal reference. |
| N6 | **Mode roster — ratify or cut.** The 14 mode names exist *only* as an art-prompt brief; no mode of any of those names exists in code. Meanwhile `Team Slayer` **is** shipped, in VO. | 53 | Conflict, see below. |

**N6 carries a live conflict.** `DT_SpotterLines.csv:49` ships the line *"Team Slayer. Live."* and
the vertical-slice GDD names Team Slayer as the only shipped mode — so **Breachpoint has already
committed to "Slayer" in player-facing audio**, while this stage is elsewhere treating `slayer`
as a term to remove. Slayer is Halo-originated but broadly generic across shooters. **Decide it
once**: either keep it (and drop it from the removal lexicon) or rename it (and re-record the VO
line). It cannot be both.

**A second conflict, inherited:** the repo holds **two irreconcilable medal lists** — 11 shipped
rows in `DT_Medals.csv` versus 16 doc-only names in `ART-PROMPT-LIBRARY.md`, overlapping only
partially. Nine shipped medals have no icon; eight icons have no medal. Not this stage's to fix,
but it lands in the same UI and should be settled with N5.

---

## 6. Execution order

1. **Today, no prerequisite:** the 132 Halo-named nodes on our *own* pages, using §4. The HUD
   ones are the most visible thing in the file.
2. **Stage 3a:** author the 8 missing components, repoint instances off the reference mains.
3. **Then §4 by script** across the 122 owned mains — 828 occurrences, one pass.
4. **Then §5**, family by family, as each decision lands.

Steps 1 and 2 need no decisions. Step 3 needs none either. **Only step 4 waits on the founder**,
and it waits on six answers, not on 1,561 nodes.

---

## 7. Execution record — §6 step 1, done

**57 layer names renamed, 22 text nodes rewritten**, across the 27 Breachpoint-owned pages.
Nothing on a `Refences - …` page was touched. Zero errors.

Three dry runs preceded the write, and each one caught a defect that would have shipped:

| Dry run | Defect caught |
|---|---|
| 1 | `Zeta Halo` → *"Zeta Breachpoint"*. `UNSC Alumnium` → *"BREACHPOINT Alumnium"* (it is a Forge material, not a faction). `Cadet Red` → *"Recruit Red"* — a **coating** name colliding with the rank. `Team Slayer` → *"Team Skirmish"*, silently deciding open question **N6**. |
| 2 | **Meaning inversion in our own engineering prose.** `"no Halo iconography reproduced"` → *"no Breachpoint iconography reproduced"*; `"built 1:1 with Halo Infinite"` → *"built 1:1 with Breachpoint"*. These sentences name Halo deliberately, as the **source**. Rewriting them destroys the note and reverses what it says. |
| 3 | Clean — executed. |

**Two guards now in the script, and both are load-bearing:**

1. **A `KEEP` list** — `Cadet Red`, `Team Slayer`, `Master Chief` are matched *first* and replaced
   with themselves, which shields them from every later rule. `Team Slayer` is on it because it
   is a **live conflict (N6), not a rename** — it ships in VO at `DT_SpotterLines.csv:49`.
2. **A 60-character prose limit.** Any string longer than that is engineering prose, never a
   content label, and is reported rather than edited. This is what stops defect 2 recurring, and
   it is the difference between a find-and-replace and a corrupted spec.

### Deferred, by design — 8 strings

Five are **our own notes that must keep the word "Halo"**, because they cite it as the reference
being matched: the rank-ladder provenance note on `Art / Insignia`, two HUD anchor notes on
`UE Handoff`, and the measured motion-tracker note on `HUD / Elements`. **Renaming these would be
a defect, not progress.**

Three are reference *copy* rather than notes — the `The Void … Halo 3's The Pit` map description
and the difficulty blurb on `Wireframes`. They belong to the **MAP family, decision N1**, and are
correctly left for that decision.

### Skipped — 2 nodes

`14:1008` and `14:992` (`UNSC Alumnium`, `Forge Editor`) carry the font **`Industry Medium
Italic`, which does not exist in this file**. Their layer names were renamed; their text was not.
Changing a text node's font is a design change, not a nomenclature fix, so the script refused
rather than substituting one. Resolve by installing Industry or by re-styling those two nodes to
Rajdhani — a design call.

### What remains

§6 steps 2–4. Step 1 cleared the Breachpoint-owned pages; **the 1,561 count is dominated by the
`FE / …` screens, which remain blocked on stage 3a (§3)** — layers inside instances cannot be
renamed, and their mains are on a reference page. That blocker is unchanged by this pass.
