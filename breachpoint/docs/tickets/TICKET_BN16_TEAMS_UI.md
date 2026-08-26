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

**26 Aug — EDITOR-SIDE AUDIT (asset-builder). One HIGH found and scripted; the editor died
mid-session, so the fix is COMMITTED-AND-READY, NOT RUN. Nothing was saved.**

Method: committed script → execute → read-back audit, driven through `mcp-ui/gen_ui/mcp.py`
against the live editor's MCP server. Scripts: `Tools/bn/bn16_audit_team_ui.py` (read-only)
and `Tools/bn/bn16_scoreboard_team_header.py` (the fix). Two probe bugs of my own, fixed
before the run quoted below: `call_tool` wants the SHORT tool name (`GetWidgetDescription`,
not the fully-qualified one `describe_toolset` prints), and `list_properties` reads property
names back camelCase — mcp.py's own rule 3, which produced four false MISSING findings on
the first pass.

- **Rung 1 is further along than this Log said.** `list_properties` on
  `/Script/BreachpointNext.Default__BNVM_Match` returns all five BN16 FieldNotify fields on
  the loaded CDO — `bTeamsMode` (carrying its `---- TEAMS (BN16) ----` comment verbatim),
  `myTeamScore`, `enemyTeamScore`, `myTeamScoreFraction`, `enemyTeamScoreFraction`. The
  editor target compiled and loaded. "WRITTEN, NOT COMPILED" above is superseded FOR THE
  EDITOR TARGET ONLY — Server and Client are unprobed and remain a `run-ubt.ps1` question.

- **F3 HIGH — `MyTeamScoreText` and `EnemyTeamScoreText` were never placed in
  `WBP_BNScreen_Scoreboard`.** The C++ members were declared at the barrier; the widgets do
  not exist in the tree. Live read, BoardCanvas children: `BannerText`, `BoardScrim`,
  `ColDeaths`, `ColKills`, `HeaderRule`, `HeaderRuleStrong`, `HeaderTick`, `ListTopRule`,
  `OutcomeAccent`, `OutcomeText`, `RowContainer`, `Row_0`..`Row_7`, `TableBottomRule` —
  and neither team readout. Because both binds are `BindWidgetOptional` the WBP compiles
  clean, no warning fires, and `RefreshTeamScores` takes its opening
  `if (!MyTeamScoreText && !EnemyTeamScoreText) { return; }` forever: BN16's team-score
  header cannot render in any match, silently, and an eyes-on founder would report it as
  "the team header is broken" with nothing in any log to explain why. This was the ONLY new
  asset obligation in the whole packet — everything else BN16 needs was already placed
  (below). FIX SCRIPTED, NOT RUN: `Tools/bn/bn16_scoreboard_team_header.py`, add-only via
  `bn11_lib.ensure`, two CommonTextBlocks on BoardCanvas at x465/x553 y141 80x32, font 26
  (BannerText's size), **no stored colour** — the C++ owns both tints — and both stored
  Collapsed so the FFA board's "no team strip" is structural rather than refresh-timed.

- **Every OTHER BN16 asset dependency is satisfied — audited, not assumed.**
  `WBP_BNScoreRow` has all three required binds and stores no leaf colour, which is what
  makes `BNScoreRow.cpp:38`'s whole-row `SetColorAndOpacity` the only tint authority.
  `WBP_BNKillfeedEntry` places BOTH `KillerText` and `VictimText` — the `bParts` gate at
  `BNKillfeedEntry.cpp:29` needs both or the row falls back to the composed `LineText` and
  no relation tint could ever render — plus `WeaponIcon`, none of them storing a colour.
  `WBP_BNMatchBand` has all four optional binds (`TopKillsText`, `ScoreLimitText`,
  `SelfScoreBar`, `TopScoreBar`); `TopKillsText` carries no stored `ColorAndOpacity`, so the
  watch-list assumption that `DefaultTopKillsTint` captures White HOLDS for this asset.

- **MEDIUM (recorded, NOT fixed — not this ticket's to take) — the enemy score BAR stays
  blue while its number goes Threat red.** Live read of WBP_BNMatchBand: both bars are
  `FillColorAndOpacity:(R=0.000000,G=0.500000,B=1.000000,A=1.000000)`. `BNMatchBand::Refresh`
  tints only the two TEXTS on a mode flip; `grep -rn "FillColorAndOpacity\|SetFillColor"
  Source/BreachpointNext/UI/` returns hits ONLY in `BNVitalsWidget.cpp` — nothing anywhere
  writes these two bar fills. So in teams mode the band reads as two blue bars with one red
  and one blue number over them, and MY bar is not Ally `#4A9BFF` either: `(0, 0.5, 1)` is
  the engine default matching no palette entry. This is the SAME gap TICKET_BN11 already
  recorded ("the ENGINE default blue, not `--shield #35D0F2` … a C++ gap in the same family
  as gap 7, visibly wrong the first time a human sees a score bar") — BN16 does not create
  it, it raises its stakes, because relative team colour is now the thing the bar is failing
  to carry. Not a Done-when blocker: that box says "shows the two team scores relative",
  which the NUMBERS satisfy. Wants a C++ owner.

- **LOW (recorded) — `RowContainer` holds exactly `Row_0`..`Row_7`.** The header comment
  says "8 covers FFA with headroom"; in 4v4 that is 8 players into 8 rows with zero headroom,
  so a 9th connection trips `bWarnedRowShortage` and drops the tail. Pre-existing, but BN16
  is what makes 8 the exact number rather than a comfortable one.

- **OFF-case verdict: NOT verifiable through MCP. It needs the founder's eyes.** MCP's reach
  ends at the asset — `GetWidgetDescription` takes a WidgetBlueprint and `list_properties`
  takes a CDO or archetype. Every value BN16 could disturb in the FFA HUD is written at
  runtime by `SetText`/`SetColorAndOpacity`/`SetVisibility` onto a `UUserWidget` INSTANCE
  that exists only inside a running world; there is no MCP handle on that instance, and
  `CaptureViewport` returns the EDITOR viewport (gizmo and PlayerStart icons, no HUD), so it
  cannot be cited as HUD evidence. What CAN be shown, and must be labelled for what it is:
  a static "no instruction differs" argument — `BNScoreRow.cpp:28-38`'s switch has no `None`
  case so `RowTint` falls through to today's `bIsSelf ? Self : InkDim`; `Refresh`'s partition
  puts every non-Enemy index in pass one and adds nothing in pass two, an identity
  permutation; `BNMatchBand::Refresh` writes colour only when `bTeams != bTeamTintApplied`
  and both start false, so the first FFA refresh performs ZERO colour writes;
  `BNKillfeedEntry`'s `bRelationTinted` requires a non-None relation so the row takes the old
  white/dim branch. **That is not frame proof and must not be written up as if it were.** It
  proves no instruction stream changed, not that the frame is unchanged. Separately: "no team
  strip" passes today only BY ABSENCE (the two widgets do not exist) — once F3's fix lands,
  the Collapsed path becomes load-bearing and this box needs re-checking.

- **Done-when mapping — ZERO boxes close from the editor.** What the editor closed is a
  prerequisite none of them names.
  - *Rung 1 all targets* — CANNOT CLOSE, partial only. CDO probe proves the Editor target;
    Server and Client unprobed.
  - *OFF-case* — FOUNDER'S EYES. Frame claim, outside MCP entirely (above). Cheapest path is
    one glance at the top of the same eyes-on session, before teams are enabled.
  - *ON (eyes-on protocol)* — FOUNDER'S EYES, and BLOCKED until F3's script runs. Row tints,
    killfeed part tints, two-block order and the band's relative numbers all have their asset
    dependencies satisfied; the scoreboard team header cannot render. Running the protocol
    before the fix fails that clause for a reason a founder will misread as a code bug.
  - *Winner banner Victory/Defeat* — FOUNDER'S EYES. `BannerText` and `OutcomeText` are both
    present, so the asset side is clear; the words are composed at runtime by the director.

- **contract_gap FILED AND GRANTED mid-session:** `Tools/` and `Content/BN/UI/` were outside
  the packet claim, so the audit ran from scratchpad rather than the claim being widened
  unilaterally (law 5). Coordinator granted; `.claude/active-packet.json` is now
  `BN15+BN16` with both paths. Both scripts are in `Tools/bn/` as a result.

- **Collateral fix — `Tools/bn/bn11_lib.py` was UNRUNNABLE as committed.** `import mcp`
  raised `ModuleNotFoundError`: the `mcp` module those drivers were written against lived in
  the BN11 session scratchpad and did not survive the `git mv` into `Tools/bn` — exactly the
  consequence of BN11's own contract_gap (`Tools/` outside that packet's owner_path, so the
  files were moved without it). bn11_lib, bn11_killfeed, bn11_death and bn11_matchband were
  ALL dead. Bound to the project's ONE committed transport (`mcp-ui/gen_ui/mcp.py`) rather
  than restoring a second copy; lazy init so importing the module does not require an editor;
  raises on a None returnValue, because the transport reports a refusal as data and a driver
  that trusts it writes nonsense into a live asset. Four callers fixed by one diff.

- **BLOCKED, and this is the honest rung: the fix was NOT run and the after-tree does NOT
  exist.** Between the audit run and the fix run, the editor hosting MCP exited — port 8000
  went from serving to zero listeners (`lsof -nP -iTCP:8000 -sTCP:LISTEN` empty, `curl` exit
  7 on three attempts 2s apart). What was briefly alive in its place was a `-game -unattended`
  headless match on `/Game/Maps/BR_Arena01` (another agent's teams-ON run), which hosts no
  MCP server, and it has since exited too. No replacement editor was launched: R29 is one
  editor, one driver, and another agent owns the headless lane. `WBP_BNScreen_Scoreboard` is
  UNMODIFIED on disk — nothing was saved this session.
  **RUN ORDER when an editor is next up:**
  1. `python3 Tools/bn/bn16_scoreboard_team_header.py` — asserts its geometry before it
     touches the asset, then adds/compiles/saves. Idempotent; re-running is a no-op.
  2. `python3 Tools/bn/bn16_audit_team_ui.py` — must print `- none` under Findings. Exit 0
     is the gate. Anything else means step 1 did not land and the ON protocol is not ready.
  3. Rung 1, all three targets.
  4. Then the eyes-on protocol, OFF-case glance FIRST.
