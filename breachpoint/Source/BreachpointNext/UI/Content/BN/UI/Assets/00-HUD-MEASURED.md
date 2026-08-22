# BN HUD — measured breakdown

Source: file `yznvnVdOFDADaugZSeomfP`. Read via the Figma MCP, 22 Aug 2026.
Canvas is **1280 × 720** (`UE Handoff` 6:55 §3 — "×1.5 → 1920 × 1080"), which is also what
`UBRScrim::DesignCanvasWidth/Height` declares in C++.

Pages read: `6:47` HUD/Core · `6:48` HUD/Elements · `6:49` HUD/Scoreboard & PGCR ·
`6:50` HUD/Death & Respawn · `6:51` HUD/Pause · `6:20` Colour · `6:54` Motion · `6:55` UE Handoff.

**This file is the referee.** Every number below is a node id you can re-read. Anything not
carrying one is marked INFERRED.

---

## The four anchors (6:55 §"IN-MATCH HUD — FOUR ANCHORS")

`TOP-CENTRE` shield + health · `BOTTOM-LEFT` motion tracker + event feed ·
`BOTTOM-CENTRE` score + timer · `BOTTOM-RIGHT` weapon/ammo/grenade/equipment tray ·
`CENTRE` reticle. The WBP anchors match these, so the HUD is edge-relative, not 0,0-relative.

## Colour tokens (`6:20`) — C++ owns these, a WBP never types a hex

| token | hex | use |
|---|---|---|
| `hud/self` | `#35D0F2` | YOU: shields, your team, reticle at rest |
| `hud/shield-low` | `#0E7E9B` | shield bar gradient floor |
| `hud/health` | `#F5C542` | health beneath shields — hidden until damaged |
| `hud/clock` | `#FFA333` | a clock is running |
| `hud/threat` | `#FF4A3D` | enemy reticle, incoming damage, shield break |
| `hud/team-them` | `#FF7A45` | opposing team in lists/feeds/scoreboards |
| `hud/ground` | `#05080C` · `hud/panel` `#0A1018` · `hud/edge` `#1E2C3A` | grounds + hairline |
| `hud/ink-dim` | `#8397A9` · `hud/dead` `#4A5A6B` | secondary text · spent |
| `surface/panel-strong` | `#000000 @0.8` | panel over bright art |
| `text/primary` `#FFFFFF` · `text/dim` `#FFFFFF @0.6` · `text/faint` `#FFFFFF @0.3` | |
| `border/rule` `#FFFFFF` · `border/rule-dim` `#FFFFFF @0.3` · `border/panel` `#FFFFFF @0.2` | |

## HUD / Core — frame `30:2` (1280 × 720)

| element | node | box |
|---|---|---|
| Vitals | `42:2` | x503.33 y66 **273.33 × 34** — shield ARC `42:3` 273.33×20 (thickness 16, sag 2.7) · health `42:4` y20 273.33×5 · centre tick `42:5` x135.9 y20 1.33×10 |
| Match State | `42:6` | x474.67 y622 **302 × 22** — mode L `42:7` x0 y2 18×18 (ELLIPSE) · bar self `42:8` x24 y7 60×8 · score self `42:9` x90 y1 34×20 · sep `42:10` x128 y2 4×18 · timer `42:11` x138 y1 43×20 · sep `42:12` x190 y2 · score them `42:13` x200 y1 34×20 · bar them `42:14` x240 y7 44×8 · mode R `42:15` x284 y2 18×18 (ELLIPSE) |
| Event Feed | `30:21` | x60 y455 **340 × 76** — rows `30:22/26/30` **340 × 20 at pitch 24**; per row: Killer x8 y2 h14 · **Weapon Glyph x78 y6 22 × 8** · Victim x110 y2 h14 |
| Loadout Tray | `30:34` | x940 y580 **280 × 110** — FRAG `30:35` x0 y0 40×34 (count x14 y8) · PLASMA `30:37` x46 y0 40×34 · Equipment `30:39` x100 y0 40×34 · Weapon `30:42` x60 y44 87×14 · Mag `30:43` x74 y58 36×43 · Div `30:44` x126 y70 5×26 · Reserve `30:45` x138 y70 28×26 · **Silhouette `30:46` x190 y56 88×32** · Stowed rule `30:47` x190 y96 88×2 · Stowed label `30:48` x120 y92 93×12 |
| Reticle | `30:49` | x620 y340 **40 × 40**, centre lands exactly 640,360 |

The band's midpoint is 625.67 — **14.33 px left of centre, deliberately**. Do not re-centre it.

## HUD / Scoreboard — frame `43:2` (MEASURED 1:1, full screen)

Not a centred plate. Full-bleed over a blurred scene that **does not pause a live match**.

| element | node | box |
|---|---|---|
| Header tick | `43:4` | x5 y18 3×52 |
| Mode icon | `43:5/6/7` | ellipses at x33 y23 **44** / x43 y33 **24** / x47.5 y37.5 **14** |
| Mode · Sep · Map | `43:8/9/10` | x100 y34 h33 · x358 · x385 |
| Header rule | `43:11` | x100 y67 **1059 × 2** |
| Win cond · sep · clock | `43:12/13/14` | x100 y73 h17 · x264 · x282 y73 |
| Team block EAGLE | `43:15..21` | accent x96 y268 4×44 · emblem x100 **44×44** (art inset 6) · nameplate x145 142×44 · score block x287 **67×44** |
| Team block COBRA | `43:22..28` | same, y335 |
| Column headers | `43:29..32` | y157 h15 w100 — SCORE x816.5 · KILLS x900 · ASSISTS x983 · DEATHS x1066.5 |
| Header rule strong | `43:33` | x465 y173 **694 × 2** (+ cap `43:34` 1×4) |
| List top rule | `43:35` | x465 y191 693×1 |
| Column tints | `43:36/37` | x825 and x992, w83, y191 h282 |
| Team fills | `43:38` / `43:102` | x465 y224 **694 × 88** · x465 y378 694×88 |
| Row | `43:40` etc | x465 **694 × 22, pitch 22** — status dot x8 y7.5 7×7 · rank x31 y4 · gamertag **x51** y5 · service tag ~x97-150 · SCORE x356.5 w90 · KILLS x440 · ASSISTS x523 · DEATHS x606.5 |
| Highlight row | `43:39/57` | fill x465 694×22 + accent x460 4×22 |
| Team divider | `43:101` | y367 693×1 |
| Table bottom rule | `43:139` | x465 y472 694×2 (+ cap `43:140`) |

## HUD / Death — frame `36:2`

| element | node | box |
|---|---|---|
| Scrim + vignette | `36:4` / `37:2` | full-bleed 1280×720 |
| Status line | `36:6` | y250, full width, h20, centred |
| Killer | `36:7` | y276, full width, **h59**, centred |
| Killing Weapon | `36:8` | x540 y340 **200 × 34** — silhouette `36:9` x0 y8 **84×20** · name `36:10` x96 y8 118×19 |
| Respawn ring | `36:12/11` | x588 y410 **104 × 104** |
| Countdown | `36:13` | x588 y432 104×71 |
| Respawn label | `36:14` | y528, full width, h17 |
| Prompt | `36:20` | y596 |
| Match state | `36:15` | x520 y660 240×44 — persists through death |

## HUD / Pause — frame `38:365`

The **451 × 682 popup chassis** (`UBRPopupOptions`, 6:55 S1: "451×682 at x=48").

| element | node | box |
|---|---|---|
| Scrim | `38:367` | full-bleed |
| Panel | `38:368` | **x48 y38 451 × 682** — header 60, content 351 @ x50, rows h28 pitch 40 |
| Border | `38:369` | inset −4 → 459 × 690 |
| Title | `38:370` | x50 y14 138×46 |
| Title underline | `38:371` | x50 y60 **160 × 2** |
| Row RESUME | `38:372..377` | fill x50 y92 **351 × 28** · top rule y92 h1 · bottom rule y119 h1 · L tick x50 y96 **1×20** · R tick x400 y96 1×20 · label x60 y97 |
| Row SETTINGS | `38:378..382` | y132 (pitch 40) |
| Row CONTROLS | `38:383..387` | y172 |
| Row FILE SHARE | `38:388..392` | y212 |
| Row LEAVE MATCH | `38:393..397` | y252 |
| Server-authority note | `38:398` | x50 y600 **351 × 46** — N1 x10 y7 · N2 x10 y25 |
| Prompts | `38:401` | x60 y686 |

**The row IS the Menu Row atom** (`UBRButton`, 250×28 shell → 351 wide here): top rule, dim
bottom rule, 20-tall side ticks, label at +10. Identical to
`Content/UI/Components/Buttons/Assets/02-MenuRow.md`.

## Motion (`6:54`) — recorded, not yet implemented

House curve `cubic-bezier(0.45, 0.15, 0.10, 1.00)`. Hover inversion **90 ms in / 140 ms out,
colour only — never scale or nudge**. Panel reveal 330 ms each way. Nothing in BN animates yet.
