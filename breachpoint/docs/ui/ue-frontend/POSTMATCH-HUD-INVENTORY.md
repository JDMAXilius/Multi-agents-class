# HUD · Scoreboard · Post-match — inventory and plan (2 Sep 2026)

Founder brief: next block is the in-match HUD, the scoreboard, and the win/lose UI, 1:1 with three
Halo Infinite captures (Player Recap, Scoreboard, in-match HUD). Read-only crew inventory, condensed.

## What exists (Source/BreachpointNext/UI, Content/BN/UI)
- HUD: `UBNHUDDirector` (subsystem; binds GameState/PlayerState/ASC/equipment), `UBNHUDLayout`
  (banner + reticle), `UBNMatchBand` (clock, my/top kills, team scores + bars), `UBNAmmoBlock`
  (mag | reserve, weapon silhouette, stowed, grenade count), `UBNVitalsWidget`, `UBNKillfeed`.
  ViewModels `UBNVM_Combat` / `UBNVM_Match` (FieldNotify, no Tick).
- Scoreboard: `UBNScreen_Scoreboard` — pooled `UBNScoreRow` (name, kills, deaths ONLY), two-block
  team partition, winner name cyan, local row = tint, outcome band (VICTORY/DEFEAT/DRAW) + two
  team scores. Hold-to-view + pinned post-match by the director.
- Win/lose: GameMode `FinishMatch/FinishTeamMatch` set Winner/WinningTeamId BEFORE `EndMatch`
  (same bunch), `HandleMatchHasEnded` freezes players and arms `PostMatchDuration` -> RestartMatch.
  GameState replicates Winner, WinningTeamId, TeamScores, MatchEndServerTime, ScoreLimit, Killfeed.
  Director composes `EBNMatchOutcome` from those. THERE IS NO POST-MATCH SCREEN: the pinned
  scoreboard's outcome band is the whole win/lose surface.
- Measured truth: `00-HUD-MEASURED.md` — Scoreboard `43:2` fully measured (header rule, mode/map,
  win-cond/clock, team blocks `43:15..28`, columns SCORE/KILLS/ASSISTS/DEATHS `43:29..32`, row
  `43:40` 694x22, highlight row `43:39/57` fill + 4px accent at x460, divider `43:101`). Death
  `36:*`, Pause `38:*`, Match band `42:6-15`, Reticle `30:49`, Loadout tray `30:34-48`.
  Motion tracker `24:2` (art `BN_Tracker_Face`, no class). Post-game XP `1860:25253` /
  `1862:25791` in the screen manifest, unbuilt.

## Gaps (reference -> repo)
- Player Recap screen: MISSING entirely (tabs, header line, SCORE, RANK, six-column strip,
  medals row + description). Data gaps: mode/map names, duration, assists, captures/returns/steals,
  score, medal awards, rank/progression. `DT_Medals.csv` (11 rows) exists, unread; no medal icons.
- Scoreboard: ~30%. Missing tab bar, header line, ASSISTS + KDA, team cards with emblems, per-row
  emblem + service tag, highlighted local row (measured, unbuilt), scrollbar, page dots, prompt
  bar, profile card.
- HUD: missing motion tracker widget, location label (`23:13`, nothing authored), objective
  marker (`23:24`), team emblems on the band, equipment slot.

## Rules that shape the work
- New replicated state (assists, medals, duration stamp, mode/map) = server-written, RepNotify,
  netcode packet + critic REFUTER (law 1). K/D/A is already in the GAS meta ledger; MEDALS ARE
  NOT — joining the ledger is a contract change in its own packet.
- No Tick: duration = `AGameState::ElapsedTime` (replicated by the engine) or a stamp diff.
- Relative presentation (`EBNUITeamRelation`): widgets never see absolute team ids/colours.
  "COBRA / EAGLE" literals in a header need a ruling (team names + emblems are not data today).
- Honesty ladder: a win/lose claim needs server + winning client + losing client.

## Proposed order (each its own ticket / packet)
1. Scoreboard 1:1 against `43:2` — pure UI over data that already replicates: header line
   (mode/map from config, outcome, score, duration from ElapsedTime), team cards, highlighted
   local row + accent, emblem + service tag (ini placeholders), columns with ASSISTS/KDA shown
   as honest-unknown dashes until assists replicate, scrollbar, prompt bar, profile card.
2. Post-match screen `BNScreen_PostMatch`: tab bar (PLAYER RECAP / TEAM LINEUP / SCOREBOARD)
   hosting the scoreboard page first; director pushes it on WaitingPostMatch instead of pinning
   the scoreboard. Win/lose = header line + outcome, from Winner/WinningTeamId. Recap page ships
   with the stats that exist (kills, deaths) and dashes for the rest.
3. Netcode packet: assists + match duration stamp + medal awards on PlayerState/GameState
   (server-only mutation), then the recap fills in. Medals need a ledger ruling first.
4. HUD: motion tracker (art exists), band team emblems, equipment slot; location label and
   objective marker need world data (volumes / objective actors) before any widget.
