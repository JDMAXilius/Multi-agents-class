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

**26 Aug — W-BUILD ×2 complete, barrier run, one wave commit per writer (stop-hook
split: writer 1 landed alone mid-wave). WRITTEN, NOT COMPILED.**

- Producer (director+VM): team ledger subscribed both-sides; deferred OnTeamChanged
  sweep runs from the roster walk (one handle per PlayerState, stale keys swept,
  H9 removal); RelationTo ladder (identity → NoTeam guard → AreFriendly — the ladder
  needs THREE answers, which is why the NoTeam test is explicit above the guard);
  teams mode = own TeamId != NoTeam (the one datum a client honestly has); relative
  ledger + relative team banner (WinningTeamId outranks the individual winner in a
  team match); killfeed pushes carry both relations via a RATIFIED defaulted-tail
  extension of PushKillfeedEntry (the ticket's escape hatch — FFA call sites
  unchanged). Writer's addition accepted: SetRoster's silence gate also compares
  Relation, or a late TeamId that moves no number never repaints.
- Widgets: relation→tint through each file's existing color channel; two-block
  scoreboard as ORDER (stable partition, non-Enemy first — identity permutation in
  FFA), team header on BindWidgetOptional members (the file's own pattern), collapsed
  when not teams; match band readouts/bars ternary on teams mode with tint writes only
  on the mode FLIP (FFA refresh is today's instruction stream); killfeed leaf tints
  with bInvolvesSelf keeping FULL priority (a line involving you is wholly white —
  ratified as designed); BNKillfeed.cpp untouched on purpose (R7.6's one-argument
  SetEntry already carries the new fields — zero-diff is the right diff).
- Decisions dated at the barrier: feed lines are immutable once pushed — one pushed
  during the reader's unknown frame keeps None relations for its ~6s linger (roster
  re-tints, feed lines do not; accepted, the roadmap's re-tint claim names rows);
  ScoreLimit is ONE number for both ladders (BNGameMode.cpp:474 confirms) so the
  band's limit text is correct in teams mode; scoreboard block seam is order+color
  only (no divider member exists — a WBP decision for a later pass if wanted).
- WATCH-LIST for the terminal (transcription over invention — first compile tells):
  weak-keyed TMap surface (Contains/Add via implicit TWeakObjectPtr ctor,
  CreateIterator().RemoveCurrent()); FieldNotify descriptor name for a bool property
  (bTeamsMode verbatim); UTextBlock::GetColorAndOpacity() getter (setter proven,
  getter not); UTextBlock default ColorAndOpacity == White assumption in the killfeed
  leaf reset.
- Barrier gates: no gameplay includes in widget files; no absolute team id reaches a
  widget (one comment hit only); GetAttitude grep law still one caller (BNTeams.h);
  LOCTEXT namespace present for the new banner strings.

**26 Aug — W-REVIEW (bn-critic, replication-race dimension): BLOCK — 1 HIGH, 1 MEDIUM,
both fixed at the barrier. Everything else attacked held.**

- **F1 HIGH — no edge re-pushed the snapshot when the OWN PlayerState bound.** The
  teams signal derives from the own PS; every push before its bind read FFA, the
  deferred subscription structurally cannot hear the own PS's initial TeamId (its
  OnRep fires inside the same bunch that adds it to PlayerArray — the broadcast is
  spent before any subscribe), and post-match supplies NO healing edge at all: a
  post-match joiner to a decided team match rendered "DRAW" for the entire
  post-match (Winner is deliberately null on a team win). FIXED: (a)
  EnsurePlayerBindings pushes the snapshot when BoundPlayerState changes hands;
  (b) ArmPlayerAcquisitionRetry — a bounded one-shot 0.5s retry that re-runs
  EnsurePlayerBindings while a match world lacks a controller/PS, re-arming only
  while the vacuum persists (law-4 note in code: the respawn clock's precedent;
  the critic flagged the acquisition-edge vacuum as pre-existing — it could also
  strand the HUD-show — so the retry closes both).
- **F2 MEDIUM — the restart rewrote the decided team banner to "DRAW" during the
  travel window**: RestartMatch clears Winner/WinningTeamId silently, then
  ResetTeamScores broadcasts → the new subscription recomposed the banner over the
  decided result (the OnRep_Winner null-guard's bug class, reopened). FIXED:
  HandleTeamScoreChanged early-outs when HasMatchEnded() — post-match ledger is
  frozen; the final kill still renders everywhere (server broadcasts the increment
  before the state flips; a client's same-bunch OnRep_MatchState push carries the
  final ledger).
- Passes worth keeping: authority-side deletion test holds (every rendered team datum
  rides an authority-fired delegate; WinningTeamId written before EndMatch); deferred
  subscription idempotent/H9-clean across rebinds, PIE and seamless travel; the
  no-PlayerArray-hook gap NARROWED for other players' rows; confident-0:0 unreachable;
  WinningTeamId same-bunch ruling holds at every read edge; all five VM fields go
  through UE_MVVM_SET_PROPERTY_VALUE; pooled rows rewrite every tint on claim; a Self
  part-relation with bInvolvesSelf false is unreachable. FieldNotify bool descriptor
  stays a watch-list compile question (bIsDead precedent noted by the critic).
