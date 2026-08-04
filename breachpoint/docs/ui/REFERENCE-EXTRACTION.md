# Reference extraction — what the Halo Infinite UI sources actually contain

**Status:** v1, 2 Aug 2026. Companion to `docs/UI-DESIGN-SYSTEM.md` (the tokens) and
`.claude/skills/ui-presentation/SKILL.md` (the method). This file is the **inventory**: every
screen, component set and icon family that exists in the sources, and what Breachpoint takes
from each. Legal boundary is unchanged and non-negotiable: **geometry and behaviour matched,
art original** (`UI-REFERENCE.md` §"Art: original, always").

**Node-id provenance — read before using any id below.** Every node id in this document (§4
inventory, §5 component sets) is an id in the **reference community file
`Kn87U5sy2VD0lP8K7h4LcQ`** — §1's "only measured source" — and **not** in Breachpoint's working
file, which holds §7b's clones under *different* ids. Verified 2 Aug 2026: `1:2` resolves in
`Kn87U5s…` as the `Play` frame 1280×720, and returns *"the provided node ID was not found"*
against `yznvnVdOFDADaugZSeomfP`. **Only `1:2` and `124:1179` have been probed; every other id
here is unsampled** — a per-id sweep is owed and has not been run.

Working file (the clones live here, ids differ): **BREACHPOINT — UI/UX System** —
`figma.com/design/yznvnVdOFDADaugZSeomfP`

---

## 1. Sources and what each one gave

| Source | Reachable? | What it contributes |
|---|---|---|
| Figma community file `Kn87U5sy2VD0lP8K7h4LcQ` "Halo Infinite UI Rework" | **Yes**, via Figma MCP | 7 pages · 78 screens @1280×720 · 20 component sets · 14 icon families · 3 type tokens. **The only measured source.** |
| `interfaceingame.com/games/halo-infinite/` | **Yes**, via curl (WebFetch is 403 — the site blocks the fetcher's UA) | 24 campaign screenshots: HUD, Map, Chapter, Database, FOB, Upgrades, Targets, Quest, Difficulty Select, Audio/Video/Controller settings, Loading, Confirmation, Interact, Control Panel |
| `mrdies.com/halo-infinite-presentation` (Eric Dies, UI + Realization Lead) | **Yes** | Method, not geometry: design-system-first, grid templates, tiered icon art direction, VISR two-channel colour, in-world branding, motion targets |
| `gameuidatabase.com/gameData.php?id=1262` | **No — Cloudflare challenge, 403 on every path incl. cookie-jar + browser UA** | Nothing. Would have supplied the *multiplayer* screens the community file and interfaceingame both lack. **Open gap** — needs a manual export if we want it. |

**The one thing no source covers: the in-match multiplayer HUD, scoreboard, carnage report and
death/respawn.** The community file is front-end only; interfaceingame is campaign only;
gameuidatabase is blocked. Those surfaces are **original design work** against
`UI-DESIGN-SYSTEM.md` §5, not a 1:1 recreation. Say so whenever they are reviewed.

---

## 2. Type tokens — measured, from the reference file's variables

| Token | Font | Size | Weight | Line height | Letter spacing | Used for |
|---|---|---|---|---|---|---|
| `Buttons/Tab Text` | Rajdhani SemiBold | 14 | 600 | 100% | **15** | Nav tabs |
| `Buttons/Button Text` | Rajdhani SemiBold | 16 | 600 | 100% | **10** | Menu rows, buttons |
| `Body/Flavor Text` | Roboto Condensed Medium Italic | 14 | 500 | 100% | **8** | Description strip |
| `Premium Yellow` (colour) | `#FFC11C` | — | — | — | — | Premium/battle-pass accent |

The extreme letter-spacing is the signature of the language — 15 units at 14px is roughly
**1.07em**. Reproduce it; it is most of why the chrome reads as "military UI".

Rajdhani ships **no italic**. Flavor text therefore stays Roboto Condensed Medium Italic — that
split is deliberate in the reference and we keep it.

Effect token in the reference: `Medal 3D Effect` — two inner shadows (`#FFFFFFCC` +0,1 r0.5 /
`#00000080` 0,-1 r0.5) plus two drop shadows (`#00000040` 0,2 r4 / `#00000080` 0,1 r1). This is
the skeuomorphic-medal treatment from 343's tier rule; it belongs on medals only, never on chrome.

---

## 3. Measured geometry (unchanged from `UI-DESIGN-SYSTEM.md` §3, restated for this file)

```
Canvas             1280 × 720   (×1.5 → 1920×1080)
Side margin        69           content width 1143
Columns            3 × 349  |  4 × 249.75      Gutter 48 in BOTH
Nav bar            y=45  h=30      tabs 666 wide from x=33
Feature card       349 × 222       (news button 330 × 222 as a component)
Menu row           h=28  pitch 40
Description strip  349 × 37
Menu Combo         349 × 510       (menu list + news button, the left rail as one unit)
Party List         349 × 273       header h=31 inset 16, rows h=30 pitch 35
Player row         390 × 30        (Player Buttons component)
Profile Bar        1280 × 50 at y=670
Load / Search Bar  1280 × 50
Grid overlays      "Grid - 3 Collumn" / "Grid - 4 Collumn", both 1143 × 570 at (69, 38)
Character subject  320 × 602 at x=480, y=118   ← the 3D scene's occupied box
Progression Button 334 × 115 at x=869, y=55    (career rank panel, top right)
```

`Grid - 3 Collumn` and `Grid - 4 Collumn` exist as real frames inside the reference screens —
the grid is a shipped artifact in that file, not an inference. Ours must be too.

---

## 4. Screen inventory — all 78, with the Breachpoint call

`KEEP` = build 1:1. `ADAPT` = same layout, Breachpoint content. `DROP` = not in our feature set.

### Main Menu section
| Reference screen | node | Call | Breachpoint name |
|---|---|---|---|
| Play | `1:2` | KEEP | `FE_Play` |
| Create | `1054:13588` | ADAPT | `FE_Create` |
| Community | `276:1764` | ADAPT | `FE_Community` |
| Menu Background | `98:763` | KEEP | `FE_Background` |
| Splash Screen | `266:1762` | ADAPT | `FE_Splash` |
| Echoes Within Splash Screen | `2409:37164` | ADAPT | `FE_Splash_Seasonal` |
| Loading | `572:10452` | KEEP | `FE_Loading` |

### Matchmaking section
| Reference screen | node | Call | Breachpoint name |
|---|---|---|---|
| Matchmaking | `2058:28286` | KEEP | `MM_Root` |
| Matchmaking Social | `33:2` | KEEP | `MM_Social` |
| Matchmaking In Progress | `933:8346` | KEEP | `MM_Searching` |
| Playlist Select | `936:8543` | KEEP | `MM_PlaylistSelect` |
| Match Composer | `1226:13396` | KEEP | `MM_Composer` |
| Matchmaking Game Voting | `2494:36113` | KEEP | `MM_Voting` |
| Matchmaking Game Chosen | `2506:36682` | KEEP | `MM_Chosen` |

### Roster section
| Reference screen | node | Call | Breachpoint name |
|---|---|---|---|
| Fireteam | `76:5392` | ADAPT → "Squad" | `RS_Squad` |
| Friends | `927:43283` | KEEP | `RS_Friends` |
| Recents | `927:43697` | KEEP | `RS_Recents` |
| Player Inspect | `2092:29214` | KEEP | `RS_PlayerInspect` |
| Text Chat | `1062:12996` | KEEP | `RS_TextChat` (396×322 panel) |

### Custom game + browsers
| Reference screen | node | Call |
|---|---|---|
| Custom Game | `1414:15140` | KEEP |
| Custom Game - Lobby Options | `1650:21830` | KEEP |
| Custom Game Browser - Cards | `1421:16805` | KEEP |
| Custom Game Browser - Table | `755:6805` | KEEP |
| Custom Game Loading | `1699:22861` | KEEP |

### File browser / Forge share (8 screens)
`File Browser - Cards` `581:4459` · `- Cards Filters` `831:7286` · `- Cards Selection Mode`
`1595:19125` · `- Table` `1428:18627` · `- Table Selection Mode` `1595:19068` ·
`- Table Selection Options` `1603:20121` — all KEEP.

### File details / bundles (6 screens)
`File Details - Overview` `1548:19329` · `- Edit` `1548:19700` · `- Credits` `1562:22399` ·
`File Bundle - Overview` `1613:23277` · `- Edit` `1613:23660` · `- Files` `1615:24051` — all KEEP.

### Settings
| Reference screen | node | Call |
|---|---|---|
| Settings | `1031:13111` | KEEP |
| Control Panel | `619:4854` | KEEP |
| Gear Filters | `891:8026` | ADAPT |

### Progression / career
| Reference screen | node | Call |
|---|---|---|
| Career | `934:9674` | KEEP |
| Career Unlocks | `1396:14822` | KEEP |
| Challenges | `82:930` | KEEP |
| Multiplayer Challenges | `934:9948` | KEEP |
| Battle-Pass | `82:840` | KEEP |
| Post Game XP | `1860:25253` | KEEP |
| Post Game XP Cards | `1862:25791` | KEEP (1320×740) |
| Rank Up Page | `3606:39204` | KEEP |

### Operator customization (Halo "Armor Hall" stack → Breachpoint operator + weapons)
`Customize` `19:2` · `Armor Hall` `208:1603` · `Core Select` `621:6717` · `Helmet` `317:2434` ·
`Armor Kits` `3598:37973` · `Weapons Bench` `572:4207` · `Primary Color` `984:12045` ·
`Finishes / Materials` `1004:13231` · `Patterns` `2095:28981` · `ODST Helmet Appearance`
`1029:12297` · `ODST Helmet Attachments` `944:10641` · `ODST Helmet Coating` `2415:29855` ·
`Gear Detail` `484:3279` · `Start Menu Background` `621:6661` — ADAPT all. The **layer taxonomy**
(core → piece → coating → finish → pattern → attachment) is the transferable part; Halo's armour
part names are not.

### Store
`Shop - Page 1` `276:2013` · `- Page 2` `2530:37007` · `- Armor` `306:2286` · `- Style`
`1486:18704` · `- The Exchange` `3594:37799` · `- The Exchange Redux` `3652:39683` ·
`Locus Armor Set` `861:7724` · `Ruckamuck Rangekick` `868:8415` — ADAPT.

### News (5 screens) — `News / Battle Pass` `103:815` · `Premium Pass` `2448:34490` ·
`Map or Playlist Feature` `2371:97304` · `Store Feature` `2377:28958` · `Boring News Page`
`2377:29265` — KEEP. These are the feature-card carousel's destination pages.

### Overlays
`Full Page Warning` `940:10676` · `Player Inspect` `2092:29214` · `Pop-Up Options` `2387:32475`
(451×682) · `Challenge Pane` `375:2694` (1298×546) · `Warning Message` `707:5770` (349×60) — KEEP.

### Campaign — DROP (5 screens)
`Campaign - Missons` `36:0` · `Mission Select` `793:9083` · `Difficulty Select` `817:6977` ·
`Skulls Select` `817:7221` · `Campaign` `1659:22234`. Breachpoint has no campaign. **Difficulty
Select's radial-icon layout is worth stealing for the bot-difficulty picker** — that is the only
carry-over.

---

## 5. Component inventory — every set in the reference, with its variant axes

| Component set | Variant axes | Count | Breachpoint UE class |
|---|---|---|---|
| `Main Button` | Status × Alignment × Type | **27** | `UBRMenuRow` + subtypes |
| — Status | Idle · Hover · Active · Active Hover · Idle Winning · Hover Winning | | |
| — Alignment | Left · Center | | |
| — Type | Default · Disabled · Drop Down · Dig Down · Icon Only · Slider · Checkbox · Radio · Map Voting · Image | | |
| `Highlight Buttons` | Status (Idle/Hover/On Click) × Type (Main/Event/Disabled/Premium/Boring/Photo Button) | 13 | `UBRHighlightButton` |
| `Items` | Type (Default/Empty/Non-Interactive) × State × Rarity (common/rare/epic/legendary) × Size (Default 114 / mini 30) | 19 | `UBRItemTile` |
| `Store Card` | State × Size (Default 280×245 / Wide 570×245) × Type | 6 | `UBRStoreCard` |
| `File Card` | Status × Type (Default/Add New) × Selection Mode × Selected — 157×132 | 8 | `UBRFileCard` |
| `Table Buttons` | State × Type (Servers/Files) × Selection Mode × Selected — 660×28 | 8 | `UBRTableRow` |
| `Player Buttons` | Status × Text Color (Black/White) — 390×30 | 4 | `UBRRosterRow` |
| `Button Border` | State × w/ Fade × Sticker — 100×100 | 6 | `UBRButtonBorder` |
| `Menu Slider Button` | Active × Status — 138×26 | 4 | `UBRSliderRow` |
| `Campaign Slider` | Active — 160×26 | 2 | DROP |
| `Microphone` | Property (Mic/Speaking/Muted) × Colour (White/Black) — 16×18 | 6 | `UBRMicIcon` |
| `Switcher` / `Switcher Icon` | Active · Inactive — 6×6 dots on a 96×10 rail | 2 | `UBRCarouselDots` |
| `Drop Down Button` | State — 107×27 | 2 | `UBRDropDown` |
| `Challenge Card` | State × Completed × Size — 364×68 / 364×54 | 6 | `UBRChallengeCard` |
| `Commendation Card` | State — 364×68 | 2 | `UBRCommendationCard` |
| `Load / Search Bar` | State (Loading/Searching/Searching 2) — 1280×50 | 3 | `UBRLoadBar` |
| `Profile Bar` | State — 1280×50 | 1 | `UBRProfileBar` |
| `Roster Group Header` | State (Inactive/Active) — 1180×31 | 2 | `UBRRosterHeader` |
| `Decorative Line` | Type (Header 1180×23 / Footer 1180×3) | 2 | `UBRRule` |
| `Page Title` | Type (Drill Down / Single Page) — 1280×75 | 2 | `UBRPageTitle` |
| `Item Title` | Rarity (Epic/Legendary/Rare) — 1280×105 | 3 | `UBRItemTitle` |
| `Store Bottom Widget` | Owned/Price × Active — 266×32 | 4 | `UBRPriceTag` |
| `Checkbox` / `Radio Button` | Enabled — 16×16 | 4 | `UBRCheckbox` / `UBRRadio` |
| `Text + Image Button` | State — 330×200 | 2 | `UBRFeatureCard` |
| `News Button` | — 330×222 | 1 | `UBRFeatureCard` |
| `Party List` | — 349×273 | 1 | `UBRRosterPanel` |
| `Menu List` | — 536×446 | 1 | `UBRMenuList` |
| `Menu Combo` | Property 1 — 349×510 | 1 | `UBRLeftRail` |
| `Menu in Border` | — 349×226 | 1 | `UBRMenuPanel` |
| `Filter & Sort Bar` | — 536×53 | 1 | `UBRFilterBar` |
| `Item Grid` | — 536×388 | 1 | `UBRItemGrid` |
| `Pop-Up Options` | — 451×682 | 1 | `UBRPopupOptions` |
| `Navigation Bar` | Menu= Template/Main/Start Menu/Roster Menu — 666×30 / 516×30 | 4 | `UBRNavBar` |
| `Button Prompts` | Input Method (Computer Key/Controller Button/Bumper L/Bumper R/Thumbstick/Thumbstick Scroll) × Size (Small 16 / Medium 20) | 8 | `UBRButtonPrompt` |
| `Progress Bar` | — 536×152 frame | 1 | `UBRProgressBar` |
| `Rating` | Rating 1.0 … 5.0 in halves — 85×17 | 9 | `UBRRating` |
| `Gear Detail` | — 626×600 | 1 | `UBRGearDetail` |
| `Game Settings Breakdown` | — 349×469 | 1 | `UBRGameSettings` |
| `Preview Photo` / `Three Photo Preview` | — 330×158 | 2 | `UBRPreviewPhoto` |
| `Countdown` | — 324×152 | 1 | `UBRCountdown` |
| `Currency Widget` | — 256×74 | 1 | `UBRCurrency` |
| `Tag Frame` | — 182×85 | 1 | `UBRTagFrame` |
| `Team Label` | — 1180×32 | 1 | `UBRTeamLabel` |
| `Small Header` | — 1180×27 | 1 | `UBRSmallHeader` |

**Variant total across the library: ~200 components.** Rule 9 of `figma-generate-library` caps a
single matrix at 30 — `Main Button` at 27 is inside it, but only just. Icons go through
INSTANCE_SWAP, never a variant per icon.

## 6. Icon families — all original art, names kept for taxonomy only

| Family | Count | Size | Breachpoint call |
|---|---|---|---|
| Button Prompts | 8 | 16 / 20 / 27×15 | KEEP — keyboard, gamepad face, bumpers, thumbstick |
| Misc. Icons | 23 | 40 | KEEP most: Time, Settings, Locked, Bookmark, Server, Ascending, Descending, Friends/Players, Playlist, Play, Recommended, Party Leader, Share, Favourite, Credits, Reset Date, Date Added, Event, Swap, Delete, Report |
| Button Icons | 10 | 16 | KEEP — Revert, Restart, Save & Quit, Download, Swap, Table View, Card View, Forward, Delete, Report |
| Item Icons | 15 | 40 | ADAPT — armour slots become Breachpoint operator slots |
| File Icons | 5 | 40 | KEEP — Map, Gametype, Pre Fab, Game, Bundle |
| Gametype Icons | 4 | 40 | ADAPT — Slayer, Tactical Slayer, Elimination, Infection |
| Large Mode Icons | 14 | 256 | ADAPT — Assault, CTF, Elimination, Extraction, Forge, Infection, KOTH, Land Grab, LSS, Oddball, Slayer, Stockpile, Strongholds, Total Control |
| Difficulty Icons | 4 | 120 | ADAPT → bot difficulty |
| Ranks | 16 | 76 | ADAPT — original insignia, same 16-step ladder |
| Grades | 3 | 42×32 | KEEP structure |
| Banners | 2 | 46×176 | KEEP structure |
| Team Icons | 3 | 240 | ADAPT — Breachpoint's two teams + observer |
| Core Icons | 5 | 40 | ADAPT |
| Manufacturer Icons | 6 | 40 | ADAPT — in-world brands, Dies' "in-world branding" note |
| Currency | 4 | 24×40 | ADAPT |
| Loading Icon | 4 frames | 40 | KEEP — a 4-frame sprite animation, so **motion is specified in the source** |
| Commendation Medals | 2 | 64 | ADAPT — original medal art (law) |
| Challenge Icons | 6 | 140 | ADAPT |
| Skulls | 2 | 50 | DROP (campaign) |
| Reach Ranks | 7 | 90 | DROP (legacy set) |
| Season Pass Icons | 4 | 100 | ADAPT |

---

## 7. Conflicts between the reference file and `UI-DESIGN-SYSTEM.md`, resolved

| # | Conflict | Resolution |
|---|---|---|
| 1 | Doc §4 lists **12 components**; the reference file has **~45 sets / ~200 variants** | Doc §4 was the HUD-era slice. **The reference file wins** — §5 above is the real inventory, and `UI-DESIGN-SYSTEM.md` §4 gets replaced by a pointer to it. |
| 2 | Doc §2 palette is a **VISR-derived 12-token** set with no `Premium Yellow`; the file ships `Premium Yellow #FFC11C` and per-rarity colours | **Both.** VISR tokens stay the semantic core (they carry meaning under stress); rarity + premium are a separate *decorative* ramp scoped to store/progression only. A decorative token may never appear on the HUD. |
| 3 | Doc §3 says "roster panel w=349 main menu / 310 lobby"; the file's `Party List` symbol is **349×273** with `Player Buttons` at **390×30** | The 390 is the standalone component's art-board width; in-screen instances are 349. Panel = **349**, row = **349** inside it. Doc §3's "310 lobby" is unconfirmed — flagged, not built, until a lobby node is measured. |
| 4 | Doc has no `Menu Combo`; the file composes the whole left rail as one **349×510** unit | Adopt `Menu Combo` → `UBRLeftRail`. It is why the rail stays aligned across Play/Create/Community without re-deriving spacing. |
| 5 | Doc §5 "no motion tracker" and "vitals top-left" | **Unchanged and unaffected** — the reference file has no HUD, so it cannot contradict these. |

---

## 7b. What changed once the reference was pasted into our own file (2 Aug, founder)

The founder duplicated the community file into **BREACHPOINT — UI/UX System** as eleven
`Refences - *` pages. Two consequences, both large:

1. **Reconstruction is dead.** Everything now lives in one file, so `node.clone()` reproduces a
   component *exactly* — variants, vectors, images, effects. No hand-rebuilding, no fidelity loss.
   68 components have been cloned into the Breachpoint component pages and renamed to their UE
   classes.
2. **Two pages existed that the MCP never listed from the community original** — and one of them
   is the most valuable page in the file:

| Page | What it holds |
|---|---|
| **`Refences - Style Guide`** | The author's own palette (7 named colour cards) and type ramp. **Authoritative.** |
| **`Refences - Forge`** | A complete Forge editor UI — radial menus (Top/Left/Right/Bottom + Exit), colour picker with hue rail and hex field, material picker with drill-down (UNSC → Concrete → options), Object Browser, Object Properties, Forge Button / Category Button / Search Button / Menu Tab component sets |
| `Refences - Campaign` | 3 frames |
| `Refences - Idea Explanations` | annotation page |
| `Refences - Images` | 18 source images |

### The Style Guide's stated type ramp — differs from live usage, and both matter

| Style Guide says | Live nodes actually use | Resolution |
|---|---|---|
| BUTTON = Rajdhani **Bold** 16 / 10% | Rajdhani **SemiBold** 16 / 10% (53 occurrences) | **SemiBold** — live usage wins for `Label/Button`; the Bold intent is preserved in `Heading/Panel` |
| MENU TABS = Rajdhani Bold 14 / **20%** | Rajdhani SemiBold 14 / **15%** | **15% SemiBold** for `Label/Tab`; 20% Bold kept as `Heading/Small` |
| PAGE TITLE 24 / 10%, Item Title Regular 48 / 10%, OPTIONS HEADER Bold 28 / 10%, Heading 2 Bold 32 | not present in the component library | **Added** — the extraction had missed the entire large end of the ramp |

### The author's palette, with their semantic names

| Role | Name | Hex |
|---|---|---|
| Highlight | Electric Blue | `#2EC3E5` |
| Secondary | Premium Yellow | `#FFC11C` |
| Limited Time | FOMO Orange | `#FF5C00` |
| Warning | Wrong Red | `#FF4B4B` |
| Rare | Rare Blue | `#6295DA` |
| Epic | Epic Purple | `#AB55FF` |
| Legendary | Legendary Gold | `#E8BA3D` |

The author also states plainly: **"Many of the fonts in this file are using substitute Google
Fonts"** — the real faces are **Industry (Adobe)** and **Segoe UI (Microsoft)**. Our use of
Rajdhani + Roboto Condensed is therefore the *same substitution the reference itself makes*, and
is OFL-licensed. This closes the typeface question.

### Art families deliberately NOT cloned — these need original Breachpoint art

`Ranks` (16) · `Reach Ranks` (7) · `Grades` · `Banners` · `Team Icons` · `Core Icons` ·
`Manufacturer Icons` · `Commendation Medals` · `Skulls` · `Challenge Icons` · `Item Icons` ·
`Gametype Icons` · `Large Mode Icons` · `Difficulty Icons` · the whole `Emblems` page — plus the
**`Map Voting` variants of `Menu Row`**, which embed emblem artwork inside an otherwise generic
component. Every one of these is 343/Microsoft art. The *taxonomy* transfers; the drawing does not.

---

## 8. Open gaps

1. **gameuidatabase blocked** — no multiplayer reference screens. Needs a manual export if wanted.
2. **No HUD/scoreboard/PGCR/death source anywhere.** P5 is original design, not extraction.
3. **`UI-DESIGN-SYSTEM.md` §6's four C++ gaps are unchanged** — per-player stat block, reticle
   target-state, respawn countdown, lobby ViewModel. No screen in this file can bind until they land.
4. **Lobby roster width (310 vs 349)** — unmeasured, see conflict #3.
