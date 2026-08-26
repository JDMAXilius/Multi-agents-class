# AIBOT Phase 6 — the mode seam, audited (W-AUDIT ×2 barrier record, 26 Aug 2026)

Two read-only auditors, one question each (game side / module side), merged here by the
lead per the wave doctrine. This document IS the Phase-6 build packet's foundation:
decisions in chat are lost, so the merged findings, the work list, and the founder
rulings it needs live here.

## The headline asymmetry

Phase 6's BRAIN half already exists and is spec'd (objective-fact tag join, urgency
selectors, distinct capture/defend inputs pinned). The GAME half is absent entirely —
pure FFA slayer, kills-as-score, no objective actor, no teams, and the HUD's own
comment admits nothing models a mode. Phase 6 is ~90% game-side and adapter work, with
a short, sharp module list.

## The chosen proof shape (game side): "HILL"

One placed volume; SOLE occupant scores one point per second; first to the limit wins;
contested = nobody scores (the legibility rule). Cost ≈ 150–200 lines in Match/:
- `ABNHillPoint` mirroring the point-of-interest actor shape (radius, no tick).
- One looping GameMode timer + a sphere overlap (the projectile's proven query shape).
- **The honest third int**: `ObjectivePoints` on the PlayerState beside Kills/Deaths —
  NOT scoring hill-seconds through AddKill, which would silently poison the killfeed,
  the leaders query, and the scoreboard's meaning.
- Optional ~30-line HUD line, which closes the match band's recorded "mode pips absent"
  note honestly.
CTF was rejected: needs teams (none exist), a carriable actor, and carry/GAS
interaction; the hill needs a volume, a timer, and an int — and its bot behaviour
("walk there, stand there") is a task the module already ships.

## Plug-in sites (each mirrors an existing, compiled precedent)

- `IAIBAmbitionProvider` → implemented ON `ABNGameMode` (it owns score limit, timer,
  fill — it IS the mode). Urgency inputs all HUD-grade: who holds the hill, score gap.
- `IAIBWorldQuery` → a `UWorldSubsystem` in `AIBotAdapter/` (the cue registrar's
  pattern; NOT the replicated GameState — these are authority-only answers).
- Handoff: one guarded line in `ABNGameMode::SpawnBot` between player-init and restart
  — the same shape as the health/blast seams. The controller receives via a
  provider-door pair mirroring the avatar door (raw interface + GC-tracked UObject,
  IsValid-guarded getter).
- Phase-6 fairness scope for the world query: implement `QueryPointsOfInterest` and
  `AreEnemies` ONLY. `QueryVisibleEnemies` as specified is a second, UNMATURED
  awareness channel that bypasses the reaction clock — it stays an honest empty until
  it can be bounded properly, and `CountNearbyAllies` returns 0 until teams exist,
  flagged unknown (see module list item 5).

## The module work list (ordered by dependency; 3 and 5–6 could wave AFTER 3 lands serial)

1. **Founder ruling** (below) on the manager contradiction. Blocks 2.
2. `Core/AIBBotManager` — authority-only `UWorldSubsystem`; the HOST PUSHES providers
   in (`RegisterProviders`) so the module never searches the world; manager pushes to
   controllers. Controller gains the provider door pair + setter.
3. `Brain/AIBAmbitionEngine` — `BuildModeAmbitionSpec()`: a mode ambition is NEVER
   registered raw (raw = a constant that beats Roam whenever combat is quiet and then
   hysteresis-holds — the bot camps an objective with zero urgency). The translation
   attaches the ObjectiveUrgency consideration with ValueWhenUnknown=0 and a commit
   window. Plus `HasAmbition(Tag)` — today "no mode leftovers after a swap" is
   UNASSERTABLE (the registry's only accessor is a count).
4. Controller possession: `ClearAmbitions()` + re-register core + translated mode
   ambitions in OnPossess BEFORE the seeding Think (the recorded CTF-in-Slayer
   obligation — today registration happens once per controller LIFETIME, which cannot
   satisfy it). Plus `RefreshAmbitions()` for mid-life mode swaps (clear + core + mode
   + immediate Think, so the empty-tag window never reaches a selection and never
   sprays the Fallback Warning the verifier counts).
5. `Core/AIBFactsBuilder` — the audit's harshest finding: THREE contracts state the
   urgency clamp in the present tense and NO code implements it. Populate Objectives
   from the provider; clamp 0..1 AND scrub non-finite at the one site (a NaN urgency
   otherwise poisons Rescore's comparisons: first-registered spec wins the whole match,
   silently); fill ObjectiveDistanceUU from the resolved POI (today it is an authorable
   curve input with no possible producer — the inert-band defect class); fill
   NearbyAllies/Enemies with a new `bCrowdKnown` flag (today they are CONFIDENT ZEROS —
   the shape F-6.10 bans — and the confidence model's outnumbered term is dead code).
6. `Execution/` — gate matching becomes `virtual bool Matches(Current)` (default stays
   exact ==) + `FAIBGateModeCondition` using `MatchesTag(Ambition_Mode)` — today a
   `Mode.Hold` child tag wins arbitration and lands in Fallback: hysteresis holds the
   want, the sentinel never fires, the bot is a STATUE beside the objective with one
   Warning in the log. Plus `FAIBMoveToObjectiveTask` whose kind defers to the
   controller's current-ambition objective kind (data, not a serialized node param) and
   which actually calls `QueryPointsOfInterest` — the fact struct carries NO location,
   so without this the bot wants the hill, moves plausibly, and never approaches it.
   Mode branch authored after Roam, BEFORE Fallback (Fallback-last is load-bearing).
7. Editor/tooling, serial (terminal): probe list +2 structs, tree rebuild via the
   trigger, read-back audit. THE PROOF IS TERMINAL-GATED — cloud cannot run PIE.
8. Specs: a real `Mode.*` CHILD tag reaching a branch (the current suite pins the join
   only with stand-in tags — green while the feature cannot work); no `Mode.*` left
   after a swap (needs HasAmbition); the clamp; the NaN scrub.

## Founder rulings needed (Phase 6 blocks on the first)

1. **The manager contradiction**: ARCHITECTURE law 3 says "AIBBotManager is the only
   sanctioned spawner"; the roadmap blesses the BNGameMode BotSystem switch; the game
   mode does the spawning; the manager is eleven lines of comment stamped "Phase 3".
   Options: (a) manager becomes the spawn API BNGameMode calls (rewrites the fill and
   convergence logic), or (b) — RECOMMENDED by both audits — law 3 is amended: "the
   manager owns provider resolution; spawning stays the mode's."
2. **The Hill mode + the honest third score int** as described above (touches
   PlayerState/GameState/VM scoreboard meaning — a design call, not a builder's).
3. (Standing, unrelated) the blast-fuse noise ruling in FAIRPLAY remains OPEN.

## Contradictions the barrier names (per doctrine — two auditors disagreeing is a finding)

- The self-containment "3 files" claim was already false (health/projectile/game-mode
  seams) — RESOLVED this commit: the roadmap footprint is now a NAMED SEAM LEDGER.
- Three contracts certify a clamp site with zero sites (work item 5).
- ARCHITECTURE's possession obligation vs the controller's own "re-registering would be
  waste" comment — the code as written cannot satisfy the recorded obligation (item 4).
- Law 3 vs roadmap vs BNGameMode.cpp reality (ruling 1).
- `Ambition_Mode` parent-or-leaf: the tags header calls it a namespace, the cpp defines
  a leaf, the specs register it directly. The MatchesTag gate serves both readings,
  which is the argument for it; the header should still say which it is.
- The manager is stamped "Phase 3", Phase 3 shipped without it, and no phase in the
  roadmap table owns it — overdue and unassigned until ruling 1 assigns it.

## Explicitly NOT in Phase 6

Teams (CountNearbyAllies stays honest-unknown), a bounded QueryVisibleEnemies, the
SeekWeapon→Seek struct renames (still owed, still serial), the Phase-4 integration step
(separate packet, gated on AIB3), and any second game mode.
