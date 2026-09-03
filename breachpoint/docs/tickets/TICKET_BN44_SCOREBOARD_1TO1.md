# TICKET — BN44: scoreboard 1:1 with `43:2` (the measured frame), win/lose header line

> STATUS: claimed — Claude (session 014esNfHwPnkiAJkRKBMwR7b) 2026-09-02. Founder: "next, the
> HUD, the scoreboard, and the win/lose UI". Inventory + order: `docs/ui/ue-frontend/POSTMATCH-HUD-INVENTORY.md`.

The scoreboard is measured 1:1 in Figma (`00-HUD-MEASURED.md` §`43:2`) and built to ~30%. This
packet finishes it over data that ALREADY replicates — no new replicated state. Assists and
service tags are honest-unknown (dash / hidden) until a netcode packet adds them. Team names
stay RELATIVE ("YOUR TEAM" / "ENEMY TEAM") until the founder rules on literal COBRA/EAGLE.

**Ordering law:** C++ binds are all Optional, so the board keeps working with the old WBP; the
layout script lands the WBP when an editor is live. Nothing here touches `Source/Breachpoint/`.

## Kickoff

- requires: engine-installed for the C++; editor-live for `Tools/bn/bn51_scoreboard_1to1.py`
- `00-HUD-MEASURED.md` §`43:2` exists (it does; the numbers below are copied from it)
- owner_path: `Source/BreachpointNext/`, `Content/BN/UI/`, `Tools/bn/`, `docs/tickets/`, `docs/ui/`

## Steps

1. `FBNScoreRowView` gains Score / Assists (-1 = unknown) / ServiceTag; the director fills Score.
2. `UBNScoreRow`: optional ScoreText, AssistsText, TagText, HighlightFill, HighlightAccent
   (`43:39/57`: fill 694x22 + 4x22 accent at x460) — the local row is a highlighted fill, not a tint.
3. `UBNScreen_Scoreboard`: optional ModeText, MapText, ResultLineText, MyTeamNameText,
   EnemyTeamNameText; `RefreshHeader` prints mode (teams/FFA), map (level name), and
   "MATCH WON/LOST/DRAW · SCORE: a-b · DURATION: m:ss" (ElapsedTime from GameStateBase).
4. `Tools/bn/bn51_scoreboard_1to1.py`: header tick/rule, column headers SCORE/KILLS/ASSISTS/DEATHS
   at the measured x, column tints, team cards (accent, emblem, name, score), fills, divider,
   row leaves at the measured row-local x. `--selftest` asserts the geometry.
5. verifier: editor + game builds; PIE post-match (teams and FFA); the three-client claim is NOT
   made here (listen server not attempted).

## Done when

- [ ] Builds PASS (editor, game)
- [ ] `bn51` run against a live editor, WBP saved, tree read back matches PLAN
- [ ] PIE post-match capture: header line, four columns, highlighted self row, team cards

## Log

### 2026-09-02 — C++ + layout script landed; WBP pass pending an unlocked session
- Row view: Score / Assists(-1) / ServiceTag; director fills Score. Row: four columns, dash for
  unknown assists, hidden empty tag, self row = 18% Self fill + Shield accent (`43:39/57`).
- Board: `RefreshHeader` — mode (teams/FFA, the lobby's names), map from the level name, result
  line "MATCH WON/LOST/DRAWN · SCORE: a-b · DURATION: m:ss" (AGameState::ElapsedTime, no clock
  of ours), relative team cards, chrome tints (tick, column tints, divider, accents) — colours
  stay C++'s. The winner sentence yields to the header line once the header leaves exist.
- `Tools/bn/bn51_scoreboard_1to1.py`: 19 board leaves + 5 row leaves at the measured `43:2`
  numbers, selftest green. Team fills (`43:38/102`) deferred: C++-sized per player count.
- Rung: editor + game builds PASS. NOT run against the editor: the Mac session is locked and
  the editor's game thread stalls under lock (every MCP call times out). Next unlocked session:
  run bn51, PIE post-match (teams + FFA), capture, then tick the boxes.
- Open rulings: literal team names/emblems vs relative; assists/service tag = netcode packet.
- 2026-09-02 (later): `bn51` run against the live editor — 17 new board leaves + 5 row leaves
  placed, both WBPs compiled and saved. Lesson folded into the script: leaves that a C++
  BindWidget owns must NOT stay Blueprint variables, or the UMG compiler tries to create the
  property twice ("Tried to create a property HeaderTick ... already exists"). A PIE match was
  run to reach the post-match board; at 2-2 with 2:55 left the founder asked for PIE to stop, so
  the post-match capture is still owed. Box 2 ticked; box 3 open.

### 2026-09-02 — founder's 1:1 target changes from the Figma frame to the shipped scoreboard capture

Founder (2 Sep): "Make our scoreboard look one to one like this" — a 2560x1440 capture of the
shipped post-game SCOREBOARD tab (scale 2.0 → 1280x720 design). Where it differs from `43:2`,
THIS wins. Measured in design px:
- Tabs top-left: PLAYER RECAP 52.5,31 111.5x20 · TEAM LINEUP from 171 · SCOREBOARD 288.5..397.5
  (selected = bright box, others dim box). No LB/RB glyphs on this screen.
- Header, RIGHT-aligned to x923: line 1 y60..74 "SQUAD:KING OF THE HILL · RAT'S NEST" (map name
  in cyan) + mode glyph at 937.5,66.5; line 2 y79..89 "MATCH LOST · SCORE: 2-1 · DURATION: 9:52".
- Team cards left: card 1 48.5,246 212.5x35 (rank digit at x36, emblem box 48.5..88 darker,
  name from x92, score block 209..261 lighter), card 2 at y291 (pitch 45), ▸ caret at x22 on the
  local team.
- Table 332.5..947.5 (615 wide), rows from y160, pitch ~17.2, row h ~17; red block then a 1px
  divider at y307 then blue block; four value cells right-aligned to the table edge, 72.5 wide
  each (KILLS · DEATHS · ASSISTS · KDA), headers at y145.5 centred on x697.5/770/842.5/916.
  Row: emblem x340 (16), gamertag x355.5 with dim [tag]; italic numbers. Self row 350..367.5:
  lighter fill, white 2px bar at x330, ▸ caret at x320. Top row framed (hovered).
- Scrollbar x956.5 y160..454 with arrows; page dots y471 centred on 640 with ‹ ›.
- Bottom band from y521.5: text prompts Close · View · Start Matchmaking · Report Player ·
  Cycle Pages at y542; profile card 774..1000 (the existing `UBNProfileBar` band).
Columns: the capture shows KDA, the Figma frame showed SCORE. KDA = (K + A/3) − D; assists are
unknown here, so KDA prints (K − D) until assists replicate — stated on the row, not hidden.
Deltas vs the pass just landed: whole-board re-layout (table/cards/header move), KDA column,
tab bar, carets, rank digits, scrollbar, dots, prompt bar. bn51's PLAN is rewritten to these
numbers; the Figma-only leaves (ColScore, ModeIcon at x33, HeaderTick) are retired.
CRASH (PIE, 2 Sep 19:29): SIGSEGV in `RefreshHeader` → `SImage::SetColorAndOpacity` on a chrome
image bind. Fixed by resolving those images by name with `Cast<UImage>`; typed binds removed.
- 2026-09-02 (evening): re-laid out to the founder's capture and VERIFIED in PIE post-match
  (two matches, watcher-captured — the board lives only for PostMatchDuration = 10 s, so a poll
  slower than that misses it): right-aligned header (mode · cyan map + glyph; MATCH DRAWN ·
  SCORE · DURATION), KILLS/DEATHS/ASSISTS/KDA cells with team-tinted plates, emblem + gamertag
  rows, the self row lifted with bar + caret, the 9px gap + divider, team cards with rank digit /
  emblem box / name / score block, page dots with arrows, bottom band with the five prompts and
  the profile card. Corrections after the first capture: tabs widened to 130 (our tab type
  overflows the capture's 111.5 — deviation), the shared scroll bar replaced by a 1px track (it
  ignored its height and painted a full white column), Report/Cycle prompts +16. Second
  capture caught the board mid-fade; geometry unchanged. Rung: PIE, single machine.
- Crash fixed and re-verified: the SIGSEGV was `RefreshHeader` tinting through a mis-bound
  `UImage` bind; every chrome tint is now a by-name `Cast<UImage>`.
