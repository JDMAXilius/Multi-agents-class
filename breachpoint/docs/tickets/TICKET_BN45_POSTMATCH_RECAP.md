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
