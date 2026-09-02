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
