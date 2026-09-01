# BN FRONT END — the plan that ships today (M-phases, 1 Sep 2026)

> Founder directive, 1 Sep: main menu next, CommonUI + native UE, **1:1 to the Figma**
> (`yznvnVdOFDADaugZSeomfP` pages 6:35/6:36/6:38), single machine vs bots, Play → map →
> FFA/TDM → bot count → match. HUD is the quality reference. **Deadline: today.**
>
> This plan supersedes the 2-Aug `ROADMAP.md` for one reason it could not have known:
> that roadmap targeted the OLD `Breachpoint` module, whose UI layer was "complete as a
> mechanism and wired to nothing". **BreachpointNext's is complete AND wired AND
> match-proven** — BNUIManager, BNRootLayout with four CommonUI stacks, BNActivatableWidget,
> two mouse-capable screens (Pause, Death), the measured HUD, and the bn11 script pattern
> that builds WBPs through the in-editor MCP. The 2-Aug suite's design content
> (LAYOUT-DOCTRINE, ASSET-PIPELINE, SCREEN-MANIFEST) remains valid reference; its
> phase plan does not. Layout truth for the menu: **`01-MENU-MEASURED.md`** (the referee,
> read node-by-node from Figma this morning, same method as the HUD's).

## The scope knife

Today ships **M1**: boot → main menu → play setup (map / mode / bots) → match → leave →
menu. Two screens, one map asset, one row component, one panel component. Everything else
the Figma contains — popup chassis, matchmaking pages, browser, voting, loading screen,
roster column — is measured, filed under M2/M3, and **not today**.

## What already exists that M1 rides on (all match-proven)

| Piece | Where | State |
|---|---|---|
| Layer stacks + push door | `BNUIManager` / `BNRootLayout` | live |
| Activatable base w/ input configs | `BNActivatableWidget` | live |
| Mouse-screen pattern (cursor, Escape, focus) | `BNScreen_Pause` | live |
| Bot fill to `TargetPlayers`, teams, tiers | `BNGameMode` + AIBot | live |
| FFA and TDM in one codebase | BN15 (`NoTeam` = FFA) | live |
| WBP building through in-editor MCP | `Tools/bn/bn11_*.py` + `bn16` | proven |
| Colour tokens in C++ | `BNUITypes.h BNUIColors` | live |
| Leave-match travel | `BNPlayerController::LeaveMatch` | live |

## What the cloud landed this morning (WRITTEN, NOT COMPILED)

1. **`01-MENU-MEASURED.md`** — the referee. Chassis inventory (§1), per-screen boxes
   (§2–4), the four M1 WBPs (§5), and the **IP line** (§6): Figma's 343-owned art never
   enters the repo; backgrounds/previews are OUR map captures.
2. **`UBNScreen_FrontEnd`** — PLAY pushes play-setup on the same Menu stack (back = pop,
   free); QUIT quits. Two live rows on the four-slot chassis; dead slots ship disabled.
3. **`UBNScreen_PlaySetup`** — MAP/MODE/BOTS **cycling selectors** + START. Roster is
   **ini data** (`[/Script/BreachpointNext.BNScreen_PlaySetup]` — a new map is one line,
   law 3). Bots row prints "8 (YOU + 7 BOTS)" / "4 V 4"; totals include the human, per
   the founder's own counting.
4. **`ABNFrontEndGameMode`** — spectator + menu push at PostLogin. Deliberately NOT
   ABNGameMode: no fill, no clock, no HUD on a menu map.
5. **`ABNGameMode::InitGame` now parses the travel URL** — `?TargetPlayers=N?Teams=0|1`
   override the ini per launch; absent options are byte-identical to today, so PIE-into-map,
   the packaged boot and the gauntlet are untouched.
6. **Config**: the two screen classes under BNUIManager; the map roster; presets 4/8/12/16;
   `LeaveMatchMapPath=/Game/Maps/FE_MainMenu` (host-case warning kept as a tripwire —
   lawful today because no connected clients exist).

Why cycling selectors and not the 451×682 popup: one fewer widget class, one fewer input
mode, zero focus-restore hazards — on the day of the deadline, the popup is M2 polish with
its chassis already measured. The reference UI itself renders these rows as label-left /
value-right, which is exactly what a cycler looks like.

## The waves (agent crew, per the founder's phase/wave method)

**WAVE 1 — cloud, DONE this morning:** everything above, pushed to main.

**WAVE 2 — terminal, editor, ~2h (BN41+BN42+BN43):** compile rung 1 → build the four WBPs
by script at the referee's boxes → build FE_MainMenu.umap → capture map previews → walk
the loop in PIE. Gate: **the loop, not the look** — boot→menu→setup→match with the right
bot count and mode→leave→menu. Ship ugly-but-exact over pretty-but-broken.

**WAVE 3 — terminal, same session if time (BN43):** previews wired into the setup screen,
background capture, notch-language polish pass against the referee, screenshots to the
founder side-by-side with the Figma.

**WAVE 4 — after the deadline (M2, not today):** popup chassis (451×682), Party List
showing YOU + the configured bots pre-match, Game Settings Breakdown panel as its own
widget, Nav Bar tabs, gamepad polish (a real UCommonUIInputData so back stops being a
NativeOnKeyDown), loading screen, roster column. All measured already.

## Honesty ladder for today

Cloud writes and it is WRITTEN, NOT COMPILED. Terminal compiles (rung 1), builds assets,
and the ONLY claim that counts tonight is: **a stranger at the packaged build reaches a
match against seven bots in under two minutes without instructions** — which is also,
verbatim, BN40's grading gate. If the menu is not walking by BN40's cook time, the
packaged build keeps `GameDefaultMap=BR_Spillway` and the menu ships in the next build;
a broken menu in tonight's zip is worse than no menu (BN40's own "if it goes wrong" law).
