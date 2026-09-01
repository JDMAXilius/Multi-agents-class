# BN FRONT-END — measured breakdown (the menu referee)

Source: file `yznvnVdOFDADaugZSeomfP`, pages `6:35` FE/Main Menu · `6:36` FE/Matchmaking ·
`6:38` FE/Custom Games. Read via the Figma MCP, 1 Sep 2026, node by node — same method and
same file as `00-HUD-MEASURED.md`, and the same law: **this file is the referee.** Every box
carries the node id it was read from; re-read the node before arguing with the number.
Canvas is **1280 × 720** (×1.5 → 1920×1080), identical to the HUD.

The designer's own margin notes, kept verbatim because they ARE the layout system:
- `22:4` — *"Left third is UI, centre is the 3D subject, right is status. Left rail is one
  Menu Combo at (69,138) 349x510."*
- `22:8` — *"Load Bar at (0,620) doubles as the search animation AND the vote countdown.
  Popups are the 451x682 chassis."*
- `22:16` — *"Two-column lobby plus a team-grouped scrolling roster. Browser is
  master/detail: 700 list + 420/460 preview."*

---

## 1. THE CHASSIS — components shared by every screen

These recur at the SAME box on every frame. Build once, reuse everywhere (they are Figma
components; they become one WBP each).

| Component | Node (exemplar) | Box | Notes |
|---|---|---|---|
| **Profile Bar** | `21:32862` | x0 y670 **1280 × 50** | full-bleed bottom band: local player identity right, prompts left |
| **Button Prompts** | `21:32863` | x60 y685 h20 | width varies with prompt count (62–227) |
| **Navigation Bar** (tabs) | `21:32864` | x33 y45 **666 × 30** | PLAY · CREATE · COMMUNITY · SHOP + LB/RB — main-menu pages only |
| **Page Title** (breadcrumb) | `21:32983` | x0 y0 **1280 × 75** | "PLAY ▸ MATCHMAKING" style — every non-main page |
| **Progression Button** | `21:32826` | x869 y55 **334 × 115** | career/battle-pass card, top-right |
| **Party List** | `21:32861` | x862 y397 **349 × 273** | header 317×31 at (16,16); rows **317 × 30, pitch 35**; border chassis = rounded rect + two 88×4 notch bars top/bottom |
| **Menu Combo** (left rail) | `21:32877` | x69 y138 **349 × 510** (main) · x69 y76 **349 × 520** (sub-pages) | = News/Preview 349×222 (photo variant 349×196.7) over **Menu in Border** |
| **Menu in Border** | `I…7:7383` | 349 × 186/226/242 | 3px border inset; **Menu List** 343 wide; contents inset **16,16**, width **311** |
| **Text Button** (menu row) | `I…21:32897` | **311 × 28, pitch 40** | the standard row everywhere (349×28 when railless) |
| **Map Button** (art row) | `21:33086` | **311 × 60, pitch 72** | map vote/select rows with art |
| **Description Frame** | `21:32837` | **349 × 37**, text inset x20 y10 h17 | the hint strip under the rail |
| **Popup Options chassis** | `21:33053` | x48 y38 **451 × 682**, border −4 → 459×688 | header 451×60 (title x50 y14, underline h2); main frame y62 451×620; list x50 y30 **351 wide, rows 28, pitch 40**; confirm row x50 y522 351×28 |
| **Load / Search Bar** | `21:33022` | x0 y620 **1280 × 50** | search anim AND vote countdown (`22:8`) |
| **Game Settings Breakdown** | `21:43050` | x466 y76 **349 × 332** | the centre "DETAILS / OVERRIDES" panel in the lobby |
| **Selection highlight** | `I…7:7398` | 3 × 65 at x−4 | the amber caret riding the list's left edge |

**The notch language:** every bordered panel = rounded rect + two **88 × 4** bars cut into
the top and bottom edges (`I21:32861;7:4097/4098` at x130.5 top / x218.5 bottom). That is
the whole Halo-panel look, and it is proceduralisable — no texture needed.

## 2. FE / MAIN MENU (`6:35`) — frame `21:32824` FE_Play

| Element | Node | Box |
|---|---|---|
| Background | `21:32825` | 0,0 1280×720 |
| Navigation Bar | `21:32864` | 33,45 666×30 |
| Progression Button | `21:32826` | 869,55 334×115 |
| Player (3D subject) | `21:32827` | 480,118 **320 × 602** — centre column is the character, NOT UI |
| **Menu Combo** | `21:32877` | **69,138 349 × 510** |
| — News Button | `I…7:7381` | 0,0 349×222 |
| — Menu in Border | `I…7:7383` | 0,232 349×186 (list 343×180, 4 rows 311×28 pitch 40, 5th hidden) |
| — Description Frame | `I…7:7384` | 0,473 349×37 |
| Party List | `21:32861` | 862,397 349×273 |
| Profile Bar | `21:32862` | 0,670 1280×50 |

Rows (screenshot): CAMPAIGN · MATCHMAKING · FIREFIGHT · ACADEMY. **BN maps: PLAY ·
Row2 · Row3 · QUIT** (strings are ours, boxes are theirs).
FE_Create `21:32902` and FE_Community `21:32941` share the identical chassis — tabs swap
the rail's data, not the layout (which is also the old inventory doc's §2 ruling).

## 3. FE / MATCHMAKING (`6:36`) — what BN actually builds today

Frames: MM_Root `21:32975` · MM_Social `21:32993` · MM_Searching `21:33015` ·
**MM_PlaylistSelect `21:33049`** · **MM_Composer `21:33056`** · MM_Voting `21:33061` ·
MM_Chosen `21:33089`.

- Sub-page **Menu Combo moves up to (69,76) and grows to 349 × 520**: Preview Photo
  349×196.7 replaces the news card, Menu in Border below it at y206.7.
- **MM_PlaylistSelect** — the Popup Options chassis at (48,38) 451×682 with the list under
  a preview photo (rows 351×28); the SELECTED entry's name+blurb renders on the SHADE
  outside the popup at x548 y129 (name h36) / y173 (blurb 317×28). This is the founder's
  screenshot 3.
- **MM_Composer** — same chassis, no photo: rows from y30, four visible; confirm ("APPLY")
  at y522. Founder's screenshot 2.
- **MM_Voting** — Menu in Border grows to 242 with three **Map Button 311×60 pitch 72**
  rows. This is the MAP SELECT row spec.
- MM_Searching — Load/Search Bar (0,620 1280×50) + description "Estimated wait" 173×37.

## 4. FE / CUSTOM GAMES (`6:38`) — the lobby BN ships

**CG_Lobby `21:43019`** — founder's screenshot 1, three columns:

| Element | Node | Box |
|---|---|---|
| Menu Combo (left) | `21:43047` | 69,76 349×520 — Preview Photo + 4 rows: MAP · MODE · LOBBY OPTIONS · **START GAME** (Action Button) |
| **Game Settings Breakdown** (centre) | `21:43050` | **466,76 349 × 332** — DETAILS/OVERRIDES read-only panel |
| **Menu in Border (roster, right)** | `21:43056` | **863,38 349 × 599** — scroll rail 13 wide at x351; contents 311: Roster header 311×31 · per team: Team Label **311×32**, player rows **311×30 pitch 35**, team pitch 113; Spectators last |
| Page Title | `21:43048` | 0,0 1280×75 |
| Profile Bar / Prompts | `21:43023/24` | standard |

**CG_LobbyOptions `21:43087`** — Popup chassis; list = Small Header 351×28 · 4 rows ·
Small Header · 2 rows (grouped settings; value sits right-aligned in the row).
**CG_Loading `21:43217`** — loading screen: hint tab 0,609 1280×111 (progress bar 1280×6,
hint title h31 + body h16 at x125), map title block at 30,427 373×163 (name h61; mode icon
pane 60×100; mode name h31 + blurb 251×36), roster at 863,75.

## 5. What BN builds from this — the four WBPs of Phase M1

| WBP | Chassis parts | Figma truth |
|---|---|---|
| `WBP_BNScreen_FrontEnd` | Background · Menu Combo(69,138) · Party List · Profile Bar · Nav Bar (static) | §2 |
| `WBP_BNScreen_PlaySetup` | Page Title · Menu Combo(69,76, photo variant) with rows MAP/MODE/BOTS/START · Game Settings Breakdown(466,76) · Party List · Profile Bar | §4 CG_Lobby |
| `WBP_BNMenuListButton` | 311×28 row, pitch 40, caret 3×65 | §1 Text Button |
| `WBP_BNPanelBorder` | rounded rect + 88×4 notches + 3px inset | §1 notch language |

Popup chassis (451×682), Map Buttons (311×60), voting, browser, loading screen: **Phase
M2+** — measured above, deliberately not in the first build.

## 6. IP line for assets (BN35 policy, applied to the menu)

The Figma mocks embed **343-owned art**: Halo screenshots as backgrounds, Spartan renders,
playlist key-art. *Numbers may enter the repo; files may not.* Export from Figma ONLY what
we authored: panel geometry (all proceduralisable — the notch language needs no texture),
glyphs we drew, layout. **Backgrounds and preview photos come from OUR maps**: the terminal
captures BR_Spillway / BR_Aquarius / BR_RallyPoint_Blockout viewports (the BN33 screenshot
loop already does this) and imports those as the preview/background textures.
