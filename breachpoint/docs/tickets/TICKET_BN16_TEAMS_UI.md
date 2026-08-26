# TICKET — BN16: the team UI (T5 — relative presentation over the BN15 feeds)

> STATUS: in-progress — cloud lead + crew wave, 26 Aug 2026. Design authority:
> docs/BN-TEAMS-PACKET.md (§UI: relative friendly/enemy off `OnTeamChanged`, deferred
> subscription for the replication race, two-block scoreboard, killfeed tints, match
> band); order: docs/BN-TEAMS-ROADMAP.md T5. Contracts: the UI doctrine already in
> force in `UI/` (director is the ONE gameplay-aware producer; widgets render and
> decide nothing; honest-unknown; C++ palette).

## Reconciliations against the roadmap's first cut

- "nameplate tints" DROPPED: BN has no nameplate widget (no over-head names exist to
  tint). The roadmap line was written from the BR module's surface; transcription over
  invention says we do not build a nameplate system to have something to tint.
- Composition changed from "scoreboard+band ∥ killfeed+nameplates" to
  **producer ∥ widgets**: `BNHUDDirector.cpp` is the single gameplay-aware file and
  BOTH team feeds (ledger, relations) route through it — it cannot be split between
  two widget writers without a shared-file collision.

## Wave plan (AIBOT-WAVES doctrine)

SERIAL FOUNDATION (lead, landed with this ticket):
- `UI/BNUITypes.h` — `EBNUITeamRelation` (None/Self/Ally/Enemy; None renders today's
  FFA palette BY CONSTRUCTION), relation fields on `FBNKillfeedViewEntry`
  (Killer/VictimRelation) and `FBNScoreRowView` (Relation), `BNUIColors::Ally`
  (#4A9BFF; the enemy reuses Threat — one hue, one meaning, held).
- `UI/BNViewModels.h` — `UBNVM_Match::SetTeamScores(bTeamsMode, My, Enemy)` +
  FieldNotify `bTeamsMode/MyTeamScore/EnemyTeamScore/My|EnemyTeamScoreFraction`.
- `UI/BNHUDDirector.h` — `HandleTeamScoreChanged(uint8)`,
  `HandleAnyTeamChanged(ABNPlayerState*)`, `RelationTo(const ABNPlayerState*)`,
  `EnsureTeamSubscriptions()`, `TeamScoreHandle`, `TeamChangedHandles` map.

W-BUILD ×2, disjoint file lists:

| Writer | Files (exact, nothing else) |
|---|---|
| ui-builder (producer) | `UI/BNHUDDirector.cpp` · `UI/BNViewModels.cpp` |
| ui-builder (widgets) | `UI/BNScreen_Scoreboard.h/.cpp` · `UI/BNScoreRow.h/.cpp` · `UI/BNMatchBand.h/.cpp` · `UI/BNKillfeed.cpp` · `UI/BNKillfeedEntry.h/.cpp` |

Barrier: lead merges, greps (no widget file names a gameplay type; no absolute team
id/color reaches a widget), single commit. Then W-REVIEW: bn-critic on the
replication-race dimension — the deferred `OnTeamChanged` subscription is THE attack,
plus the register note from BN15: `OnRep_TeamId`'s body is now load-bearing on the
authority (the director subscribes `OnTeamChanged`, which the authority fires by hand
from the setter) — law 3's deletion test must be re-run against this wave.

## Done when (terminal proof, after the review barrier)

- [ ] Rung 1 all targets
- [ ] OFF-case: today's FFA HUD renders untouched (relations all None; no team strip)
- [ ] ON (eyes-on protocol): scoreboard two blocks, my side listed first and tinted
      Ally/Self, enemies Threat; match band shows the two team scores relative
      (mine left); killfeed killer/victim parts tinted by relation; a mid-match
      joiner's rows re-tint when late TeamIds land (the deferred subscription firing)
- [ ] Winner banner reads Victory/Defeat from WinningTeamId vs own team, threes later
      under BN18

## Log

_(outputs verbatim)_
