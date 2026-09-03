# TICKET — BN45: PLAYER RECAP as the win/lose screen, 1:1 with the founder's capture

> STATUS: claimed — Claude (session 014esNfHwPnkiAJkRKBMwR7b) 2026-09-02. Founder: "after the
> scoreboard, do the win/lose screen widget, one to one with this [recap capture] but add the
> win lose somewhere".

Reference: the shipped post-game PLAYER RECAP (1920x1080 → x1.5 → 1280x720). Measured (design
px): LB pill 38,45 · tabs from 66.7,38 h28.7 · RB pill 525,45 · header right edge 1214.7, lines
at y38.7 / 56.7 / 73 (font ~11, values bold, "|" dividers) · SCORE label 66.7,105.3, rule y122
296.7 wide, value at y135 (font ~46) · RANK label 660,105.3, rule 640,122 290 wide, crest 640,130
73x67, "DIAMOND 3" 763.3,140 (18, cyan), bar 730,176.7 183.3x6.7 · stats strip 66.7,213.3
846.7x170, six 141.1 columns, labels y235, values y305 (30), the 4th column lifted with a caret
and a white rule · medals 66.7,394.7 846.7x92, eight 105.8 cells, first framed, chips 46.7, counts
y466.7 · medal name 66.7,528 (18) + italic description y556 · nameplate 960,523.3 250x43.3 ·
bottom band from y656.7 with B Close / X Start Matchmaking / R Profile at y683 and the profile
card. The VICTORY / DEFEAT / DRAW word (the founder's addition) sits top-right at y14, right-aligned
to the header, in Shield / Threat.

**Honesty:** the six columns are KILLS · DEATHS · ASSISTS · KDA · SCORE · MEDALS; assists and
medals print dashes (not tracked); the rank block prints the ini career line the front end shows.
Team names stay relative (YOUR TEAM / ENEMY TEAM) pending the founder's ruling.

## Kickoff
- requires: engine-installed (C++), editor-live (`Tools/bn/bn52_recap_1to1.py`)
- owner_path: `Source/BreachpointNext/`, `Content/BN/UI/`, `Tools/bn/`, `Config/`, `docs/tickets/`

## Steps
1. `UBNScreen_PostMatch` (Config=Game, Menu input): outcome word, three header lines, score, rank,
   six stat columns, medal row, nameplate, prompts (Close = LeaveMatch), tabs (SCOREBOARD pushes
   the scoreboard; its PLAYER RECAP tab pops it).
2. `UBNUIManager::PostMatchScreenClass` (ini) + the director pushes it on WaitingPostMatch; the
   scoreboard pins only when no recap class is set (pre-BN45 behaviour preserved).
3. `bn52_recap_1to1.py` places the 62 leaves + 3 tabs; all colours are C++'s.
4. verifier: builds; PIE post-match capture (the board lives 10 s — use the 2 s watcher).

## Done when
- [x] Builds PASS (editor, game)
- [x] bn52 run, WBP saved
- [x] PIE post-match capture shows the recap with the outcome word, then SCOREBOARD tab → board

## Log
- 2026-09-02 (late): landed and VERIFIED in PIE post-match (watcher capture): the recap is the
  post-match screen — DRAW / VICTORY / DEFEAT top-right, three header lines, SCORE + rule, RANK
  crest + career line + bar, six columns with the KDA column lifted (caret + rule), eight medal
  chips with the first framed and dashes, MEDALS title + body, nameplate (placement digit,
  gamertag, YOUR TEAM), bottom band with Close / Start Matchmaking / Profile and the profile card.
  SCOREBOARD tab pushes the board over it (captured); the board's PLAYER RECAP tab pops it.
  Tabs are fixed canvas boxes at the measured widths (an HBox makes the shared button report its
  311 design width). Header line 3 prints "WINNER: <relative>" only until literal team names are
  ruled on. Rung: PIE, single machine. Boxes 1-3 ticked.
- 2026-09-02 (later): "put the assets" — Figma has NO medal icon set (the PGCR page's medals are
  bare ellipses; `SET Feedback / Medal Chip` 62:106 is the only medal art, a backing). Its PNG
  export is opaque, so `bn53_medal_chip.py` draws the two hexagons from the SVG path data, CENTRED
  (the HUD audit's imported chip had both at (0,0)), and imports `T_BN_MedalChip`; the recap's eight
  medals use it. Team emblems, rank crest, nameplate and mode glyph were already the Figma exports.
  Medal ICONS remain a Family-D art task (ART-PROMPT-LIBRARY), not a Figma export.
- "make text on button dynamic": `BNTabBar::Layout` measures each tab label with the font service
  and sizes/places the tabs from it (text + 2*pad, gap 6.5), on both the scoreboard and the recap.
- 2026-09-02 (verified): recap + board captured post-match with the label-measured tabs (no
  overlap on either screen, LB/RB pills hugging the bar) and the centred medal chip in all eight
  cells; team-card scores now read (white on the tinted block). VICTORY case captured: winner line
  "WINNER: YOUR TEAM". Rung: PIE, single machine.

### 2026-09-02 — AUDIT: what is still fixed-size / not yet dynamic (founder: "audit what is left")

DYNAMIC TODAY: scoreboard rows/divider/bottom rule/track/dots follow the player count; gamertags
ellipsise + clip at the cells; team-card names ellipsise; tabs size to their labels on both
screens; recap header lines are right-aligned auto-fit; prompts auto-size; settings panel rows
and roster rows are built from data; profile bar collapses an empty gamertag / zero friends.

STILL FIXED — recap (WBP_BNScreen_PostMatch, bn52):
1. `NameplateName` 180 wide, no ellipsis — a 20-char gamertag (the PIE machine name did) runs
   past the plate art. Fix: ellipsis + clip, one line each in Refresh.
2. `RankLineText` 240 wide, 18pt, no ellipsis — a longer career line would clip hard.
3. `ScoreValueText` 300 wide at 46pt — fits "99,999"; a 7-digit score would not. Fix: ellipsis or
   a font-size step at 6+ digits.
4. `MedalNameText` 500 / `MedalDescText` 500 — no wrap; a two-line medal description (the
   reference's are one line) would clip. Fix: AutoWrap on the description only.
5. Stats strip: six columns of 141 at 30pt — a 6-digit value would overflow its column. Fix:
   step the font down above 5 digits (BNTabBar-style measure), or accept (kills never reach it).
6. Medal row: fixed eight cells; more medals than eight need paging (the capture's dots row is
   exactly that page control) — blocked on medals existing at all.
7. Outcome word: fixed 20pt, right-aligned — fine for VICTORY / DEFEAT / DRAW; a localised long
   word would need the same measure step.
STILL FIXED — scoreboard (bn51):
8. Table height is the authored 294 (17 rows); LayoutRowBlock grows past it but never shrinks
   below it, so a 4v4 board carries empty plate under the rows. Fix: let the list shrink to the
   row count (AuthoredListHeight floor removed) and move the bottom rule / dots with it — they
   already follow the computed height.
9. Team cards: fixed 212.5 wide; names ellipsise, scores are 52-wide blocks (3 digits max).
10. Header line 1 right-aligned auto ✓; the mode glyph is a fixed 14 at x930.5 — it does not move
    if line 1 grows taller (single line today).
STILL FIXED — front end / lobby (from the earlier passes):
11. Lobby DescriptionText (349x37 band): no wrap policy set — a long map description clips.
12. Front-end news title / hint: single line, no ellipsis.
13. Rail rows: 311 wide label + value; a long value ("TEAM DEATHMATCH") already nearly meets the
    label — no ellipsis on the value.
DATA, NOT LAYOUT (needs the netcode packet or a ruling): assists, medals, service tags, literal
team names/emblems, per-player emblems, the "1/6" header counts.

### 2026-09-02 — match end = recap, then everybody to the front end (founder's flow)

`ABNGameMode::TravelToFrontEnd` replaces RestartMatch when `PostMatchMapPath` is set (ini:
/Game/Maps/FE_MainMenu). Armed by HandleMatchHasEnded after `PostMatchDuration` (ini: 5 s, the
recap's time on screen). Scores/winner/killfeed are wiped first because seamless travel carries
PlayerStates. Non-PIE: `World->ServerTravel(path, absolute)` — one call, every connection
follows. PIE: `OpenLevel` (a ServerTravel ends the PIE session, measured 25 Aug); the log names
which path ran. Empty path = the old restart-in-place, so nothing existing changes by accident.
Verified PIE: recap up at match end, front end (rail + IN MENUS) present 4.4 s later. Rung: PIE,
single machine — the ServerTravel branch itself is the packaged/listen path and is NOT exercised
in PIE by design; it needs the listen-server rung (host + client) before a multiplayer claim.
