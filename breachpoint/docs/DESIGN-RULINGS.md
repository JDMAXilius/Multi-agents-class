# Design Rulings — the ledger the critic judges against

A REFUTER with an attack surface but no statement of intent attacks the design itself,
escalates every round, and never converges — the first data-crew run proved it (three rounds
of "findings" against intended Halo design before rulings were injected). This file is the
fix, made permanent: **rulings already made.** Every critic pass loads it. Attacking a ruling
is out of scope for review; a finding must show a hard-constraint violation or an internal
contradiction. Re-opening a ruling is a *founder/lead* decision, logged here with a date —
never a review outcome.

The lead appends; nobody else writes here. A doubt this ledger closes is closed.

## Combat sandbox (from the 29 Jul 2026 data-crew run)

- **R1. Precision weapons reward aim.** The Magnum's all-headshot TTK beating every other
  path is the intended fantasy, paid for by an 8-round mag and no forgiveness on a miss.
  "Skilled Magnum play is strong" is the design working.
- **R2. The AR is the shield-stripper, not a finisher.** Slower solo TTK is intended; its
  headshot multiplier stays 1.0 — headshot bonuses belong to precision weapons.
- **R3. The Magnum is the finisher, not a self-sufficient primary.** It is NOT required to
  solo a full-200-EHP target from one mag on body shots — the intended line is AR-strip →
  0.4 s swap → Magnum finish.
- **R4. The Rocket is balanced by scarcity, not by damage.** Map pickup, 90 s timer, 2 shots,
  ReserveMags 0 (reload intentionally unreachable). It is SUPPOSED to win the fight it is
  present for. HeadshotMult stays 1.0 (a 240-damage headshot one-shot is a defect — caught
  in the run).
- **R5. Shields-first, no health regen, 2.5 s recharge delay** are pillars, not tunables.
  Proposals touching them go to the founder, not through the pipeline.

## Arena (from the same run)

- **R6. The slice ships one compact map**, not a competitive-ranked layout. Imperfect spawn
  distribution is acceptable when the hard constraints hold (≥ 8 spawns, ≥ 8 m spacing,
  ≤ 35 m sightlines) and the imbalance is documented in the manifest's doubts.
- **R7. Geometry claims are editor-rung.** Mutual visibility, occlusion, and 5 m LOS-breakage
  cannot be settled from coordinates; a manifest that records them in `doubts[]` with that
  caveat has handled them correctly. Only coordinate-provable contradictions are findings.

## Bots & AI (BP08 domain)

- **R8. No LLM in the hot path — structural, not preferential.** Bot decisions in a live
  match are deterministic code reading data. LLMs shape bots offline (tuning rows, ambition
  weights via the curator pipeline) and decorate post-hoc (Spotter strings). There is no
  mid-match model call that a game outcome waits on.
- **R9. One brain: GOAP-style goal layer over a StateTree execution spine** (see
  `BREACHPOINT-AI-BOTS.md`). No second BT asset running in parallel — behavior-tree
  patterns live as selector-shaped tasks *inside* StateTree states. A dual-brain proposal
  is a finding against itself.
- **R10. Plans are short and disposable**: ≤ 3 steps, replanned on event, never per-tick.
  A bot that "thinks" every frame is a perf finding; a plan that survives a world
  contradiction is a correctness finding.
- **R11. The 200 ms reaction floor is a law, not a difficulty knob.** No tier, scalar, or
  ambition weight may produce sub-human reaction; `Breachpoint.Bots.*` pins it.
- **R12. Bots are legible before they are optimal** (the Halo lesson). A bot whose state
  change players can't read (break-off on shield-crack, rocket contest on timer, tier
  fantasy) fails review even if it wins more.

## Crew & pipeline

- **R13. Only `high` severity blocks a landing.** Medium/low land in the risk register with
  the artifact. A reviewer with no ship gate never ships — the run deadlocked to prove it.
- **R14. The orchestrator holds no opinions.** Managers are deterministic (script + lead
  session); intelligence lives inside the boxes. A proposal to add an LLM manager reargues
  a closed ruling.

## Tags & engine seams (31 Jul 2026, from the contract audit)

- **R17. Montage-raised gameplay events extend `Event.*`; there is no `GameplayEvent.*`.**
  The authoritative native-tag header (`ARCHITECTURE §3.1`, landed by BP01) already owns
  `Event.*` for sim-facing gameplay events (`Event.Death`, `Event.Kill`). The
  montage→gameplay notify seam joins it rather than opening a parallel namespace — a second
  top-level namespace meaning the same thing is how a header stops being authoritative.
  `GameplayEvent.Combat.*`, which `animation.md` previously used, is **rejected**: it existed
  in no header, no ticket, and no other document.

  **The four tags, landed by BP01 step 2:**

  | Tag | The moment it announces | Consumed by |
  |---|---|---|
  | `Event.Melee.WindowBegin` | melee trace window opens | `BRGA_Melee` (BP05) |
  | `Event.Melee.WindowEnd` | melee trace window closes | `BRGA_Melee` (BP05) |
  | `Event.Weapon.ReloadCommit` | the point ammo actually moves | `BRGA_WeaponUtility` (BP03) |
  | `Event.Weapon.SwapCommit` | the point the active slot flips | `BRGA_WeaponUtility` (BP03) |

  **Extension rule (so the next one needs no ruling):** montage-raised events are
  `Event.<Verb>.<Moment>` — the verb is the ability's noun (`Melee`, `Weapon`, `Grapple`),
  the moment is what just happened in the animation, never what should result from it.
  A new tag under this rule is a normal `Core/` change, not a re-opening of R17.

  **The boundary this preserves (`animation.md` law 4):** a notify announces a *moment*; the
  sim decides the *consequence*, on the authority. A notify never carries a number, a branch,
  or a damage call. `Event.Weapon.ReloadCommit` says "the hands reached the magazine" — it
  does not say how many rounds, which is a `DT_Weapons` row, and it does not move them, which
  is the ability's job on the server.

  *Why four and not two:* BP03 builds reload **and** swap in one ability (`BRGA_WeaponUtility`)
  and `animation.md` law 3 authors both timings to the sourced pack, so both need a commit
  moment. An unused native tag costs nothing; a missing one costs a `contract_gap` and a stop
  after `Core/` has closed — the asymmetry decides it.

## Online services & Phase 2 (from the GameLift plan, 29 Jul 2026)

- **R15. Identity is Steam-derived; there is no first-party account creation.** Phase-2 auth
  validates the client's Steam session ticket server-side and issues our own short-lived
  token. Any flow that asks a Steam player to sign up (Cognito login UI included) is a
  defect, not an option. Cognito may serve as token machinery only.
- **R16. Managed fleets (real money) are telemetry-triggered, never date-triggered.**
  GL-3 opens only when demo telemetry shows the listen-server pain: host-quit abandonment,
  NAT/join failure rate, host-advantage complaints. Low numbers = the fleet money stays
  unspent and listen + GL-2 keeps shipping. Billing alarms and fleet caps are acceptance
  criteria on any fleet ticket (denial-of-wallet is an exploit class).

## Authoring policy (29 Jul 2026)

- **R18. Zero Blueprint classes; an engine asset exists only where UE 5.8 has no C++ path,
  and every such asset is named in `BREACHPOINT-AUTHORING-MATRIX.md`.** The complete
  Tier-4 list is: AnimBlueprint graphs, materials/instances, Niagara, MetaSounds, UMG
  layout (WBP), the `ST_Bot` StateTree asset, EQS query assets, and sourced art. Anything
  not on that list does not get an asset — it is C++ (Tier 1), text data (Tier 2), or
  generated by a committed script (Tier 3).
  Corollaries that are findings, not preferences: GameplayEffects are **C++ classes**, not
  assets (SetByCaller + dynamic tags make six of them cover the game); GameplayCue handlers
  are **C++ classes** registered by tag (the asset is only the VFX/SFX they play); an
  AnimGraph carrying a gameplay number or decision is a contract violation; a WBP carrying
  anything but layout is a contract violation.
  The reason is reviewability, not taste: **binary assets are invisible to the critic** —
  no diff, no merge, no grep. Adding a Tier-4 asset means adding a part of the game only a
  human staring at the editor can review, so the standing question for any new asset is
  *"which tier, and if Tier 4, why can't C++ express it?"* No crisp answer ⇒ no asset.

## Verification policy (31 Jul 2026 — paid for by a false PASS, not theorized)

- **R19. A build claim requires a timestamp proof. A file existing is not evidence that you
  built it.** Every compile-rung report — verifier, builder observation, CI — carries five
  items per target: (1) wall-clock time printed BEFORE the command, (2) verbatim output tail
  including `Result:` and `Total execution time:`, (3) exit code captured from `$?`,
  (4) the produced binary's mtime, and (5) **an explicit assertion that mtime > start time.**
  Item 5 is the load-bearing one — it is the only check a pre-existing artifact cannot satisfy.
  A report missing it is not a rung result; it is an opinion about a directory listing.

  *Origin:* BP01's first V1 pass reported PASS on three targets having rebuilt one, presenting
  the size and mtime of binaries a **builder** had produced an hour earlier as its evidence for
  the other two — one of which had since been deleted. See BP01's Log.

  *The structural lesson, which generalizes past builds:* the crew's separation of powers
  capability-limits the verifier (no write tools) so that "quietly fixed the test" is
  impossible. That defends against a reviewer **changing** an artifact. It does nothing against
  a reviewer **misreading** one — `ls`, `git diff` and `grep` are all read-only, and a stale
  artifact is indistinguishable from a fresh one unless something forces the time question.
  Read-only is not the same as honest. Where a rung's evidence could be satisfied by state that
  already existed before the check ran, the check must be redesigned, not trusted harder.

- **R20. `-Rebuild` on a monolithic UE target is not safely retryable, and "from scratch" must
  be witnessed, not assumed.** UBT deletes a target's binaries before recompiling, so an
  interrupted `-Rebuild` leaves the tree strictly worse than before it started — BP01 lost its
  game executable this way while the report read PASS. Prefer an incremental build that
  demonstrably completes (under R19's timestamp proof) over a `-Rebuild` that may not. When a
  Done-when clause says "from scratch" and only an incremental build completed, the honest
  verdict is **INCONCLUSIVE**, never PASS — and a build reporting "up to date" with zero
  actions proves nothing at all about compilation.

- **R21. Stopping an agent does not stop the processes it spawned, and UE's build lock is
  global. One build agent at a time — always.** `TaskStop` kills the agent, not its children:
  a `Build.bat` it launched keeps compiling, keeps holding `Build.bat`'s mutex, and keeps
  writing binaries long after the agent is gone. A second verification agent dispatched into
  that state does not queue politely — it gets
  `"Build.bat is already running, waiting for existing script to terminate..."` and, if it is
  honest, reports **BLOCKED**.

  *Origin (31 Jul 2026, BP01):* the lead retired a verifier mid-build and dispatched a
  replacement. The orphaned Game build ran to completion at 22:35:55 and its Server build held
  the lock; the replacement hit the mutex at 22:39:10 and correctly reported BLOCKED, and its
  Game check came back INCONCLUSIVE because the orphan had already made the target up to date.
  Two agents, three reports, zero usable rung results — none of it the replacement's fault.

  *Rules that follow:*
  1. **Never dispatch a second build-running agent while one is live.** Check for live
     `UnrealBuildTool`/`cl.exe`/`link.exe` before dispatching, not after.
  2. **Before stopping a build agent, decide what happens to its build.** If the build should
     die with it, kill the process tree explicitly. If it should finish, wait for it — do not
     dispatch over it.
  3. **Parallel pods (`CREW_PLAYBOOK` §12) do not make builds parallel.** Separate worktrees
     give disjoint *files*; they do not give disjoint *build locks* or disjoint
     `Engine/Intermediate`. Two builders may write code simultaneously; only one may compile.
  4. A wait for a build to end must exit on **failure as well as success** — poll for the
     build processes disappearing, not for the success artifact appearing. Waiting on the
     happy-path marker alone makes a crash indistinguishable from slow progress.

## Gameplay tag structure (31 Jul 2026 — resolved at BP01 step 2, before Core/ closed)

- **R22. `Damage.*` is FLAT: types and modifiers are siblings that compose. The tag is
  `Damage.Rear`, never `Damage.Melee.Rear`.** ARCHITECTURE §3.1 — the one authoritative tag
  list — enumerates `Damage.*` as `Kinetic, Explosive, Melee, Headshot, Rear`. §3.3 and
  `TICKET_BP05_TRIANGLE.md` both write `Damage.Melee[.Rear]`; **that notation is shorthand for
  the pair `{Damage.Melee, Damage.Rear}`, not a nested tag,** and the tickets are corrected to
  say so.

  *Why flat wins on the merits, not just on authority:* `Kinetic / Explosive / Melee` are damage
  **types** and `Headshot / Rear` are **modifiers**. A hit carries one type plus zero or more
  modifiers, so the ExecCalc queries each axis independently. Nesting `Rear` under `Melee`
  permanently forbids a rear-arc bonus on any non-melee source, and by symmetry would force
  `Damage.Kinetic.Headshot` — which no source proposes. Flat costs nothing: GAS tag containers
  hold both tags at once, and a query for "was this a rear hit" stays one comparison.

  *Consequence for BP05:* its ExecCalc asserts BOTH `Damage.Melee` and `Damage.Rear` for the
  lethal rear-melee case. A builder that greps for `Damage.Melee.Rear` will not find it — that
  is correct, and this ruling is why.

- **R23. `Ability.*` and `GameplayCue.*` are OPEN families. `Core/` closes for the other five
  and stays open for these two, and the packet that introduces an ability or cue declares its
  tag — under an exact-file owner_path grant.** §3.1 names both families and enumerates no
  leaves, unlike the five closed families. That is not an omission to be "fixed" by guessing:
  the leaves cannot exist at BP01 time because the abilities and cues do not exist yet. One tag
  per ability, one per cue, authored by the packet that authors the thing.

  *The mechanical half, which is the part that actually bites:* BP02/BP03/BP05 own
  `AbilitySystem/`, `Weapons/`, `Abilities/` — **not `Core/`** — so the hook blocks them from
  adding their own tag to the one authoritative header. At claim time each such packet's
  `owner_path` gains the two exact files `Source/Breachpoint/Core/BRGameplayTags.h` and
  `.cpp` — the same exact-file device BP01 used for its three `.Target.cs` entries, and for the
  same reason: grant the file, never widen to the folder.

  *Law 7 tension, recorded rather than waved away:* this means several packets append to one
  file, which one-owner-per-artifact otherwise forbids. Accepted because the file is append-only
  text in per-family blocks, conflicts are line-level and reviewable, and the board serializes
  claims anyway. If two packets ever do collide here, the answer is to split the header by
  family — not to let a packet declare its tags somewhere else, which would end §3.1's "one
  authoritative header" guarantee and with it the grep that proves nothing is missing.

- **R24. Log channels track the §3 discipline folders, one per folder. `LogBRInput` is added
  now; the extension rule is mechanical, not case-by-case.** ARCHITECTURE §3.1 enumerates
  `LogBRCombat/Net/AI/Online/UI` — five channels for twelve discipline folders. That is an
  oversight, not a design position: `Input/` is a §3.2 folder with no channel, so the step-3a
  builder had to fall back to development-only `ensureMsgf` for every diagnostic and could log
  nothing at all.

  This is not cosmetic. **BP01's Done-when box 4 requires "Native input flows IMC →
  BRInputComponent → tags → controller stubs (**log-proven**)"** — a clause no one can satisfy
  with no channel to log on. A missing log channel does not fail a compile; it fails an
  acceptance check months later, which is the same shape as the missing-gameplay-tag hazard R17
  and §3.1's grep exist to prevent.

  *Rule:* a discipline folder that needs to speak gets `LogBR<Folder>`, added to `BRCore.h/.cpp`
  by the packet that first needs it, under the same exact-file `owner_path` grant R23 defines
  for `BRGameplayTags.h/.cpp`. No new ruling per channel — this one covers all of them.

- **R25. `Source/Breachpoint/Tests/` holds ONE spec file per feature packet, named
  `BR<Feature>Spec.cpp`, each with exactly one owner. §3.12's three files are the initial set,
  not the complete set.** The folder appears in no ticket's `owner_path` while BP02, BP03, BP05
  and BP06 all must write specs there — surfaced at BP01 and deferred twice because the obvious
  per-ticket fix collides with law 7 (one owner per artifact) over a shared `BRCombatSpec.cpp`.

  *The resolution turns on a fact about UE automation that removes the conflict:* a suite name
  is a property of each **test**, not of the **file**. `Breachpoint.Sim.Combat` can be declared
  across many `.cpp` files, so "one pinned suite" and "one file per packet" are not in tension.
  BP03's fire-path tests live in `BRWeaponFireSpec.cpp` and still register under
  `Breachpoint.Sim.Combat`; BP05's radial-falloff cases live in `BRTriangleSpec.cpp` and do the
  same. Law 7 holds unbroken — every file has one owner — and no packet waits on another to
  release a folder.

  *Grant shape at claim time:* the packet takes its **own spec file by exact path**, never the
  `Tests/` folder. Same device as R23's `BRGameplayTags.h/.cpp` and BP01's three `.Target.cs`
  entries. A packet that needs to *read* a sibling spec may read it; the hook only gates writes.

  *ARCHITECTURE §3.12 is amended by this ruling* — it names three files and says "3 (cpp only)",
  which was a snapshot of the first three suites, not a cap. Anyone reading §3.12 as a closed
  list should read this ruling instead.

  *What this does NOT license:* a packet quietly adding a spec file that asserts nothing so a
  rung goes green. `testing.md`'s grep gates and BP00 step 5's critic pass ("can a spec pass
  asserting nothing?") still apply, and a spec file with no meaningful assertion is a finding.

- **R26. Blueprint children of BR C++ classes are permitted as DEFAULT-VALUE CONTAINERS ONLY.
  Zero graph nodes. This narrows R18; it does not repeal it.** Founder decision, 1 Aug 2026,
  after `BP_BRGameMode` / `BP_BRCharacter` / `BP_BRPlayerController` / `BP_BRPlayerState` /
  `BP_BRGameState` were created in the editor to assign assets.

  *What made this reasonable:* assigning a mesh, an `InputConfig` or a mapping context through
  a Blueprint child is the ordinary UE workflow, and a designer opening the editor to point a
  soft pointer at an asset is not the hazard R18 exists for.

  **A conforming BP child, all five conditions:**
  1. It is a direct child of a `BR`-prefixed C++ class and adds **no** new parent in between.
  2. **Its EventGraph and ConstructionScript are empty.** Zero nodes. Not "only cosmetic
     nodes" — zero, because "only cosmetic" is not a state anyone can verify at a glance.
  3. It declares **no** new variables, functions, macros, interfaces, or components. It sets
     values on properties the C++ class already declares `EditDefaultsOnly`/`EditAnywhere`.
  4. **No gameplay NUMBER is set here.** Law 3 is untouched: damage, cooldowns, speeds, score
     limits and timers come from `Content/Data/*.csv`. A BP default that mirrors a table value
     is the silent-drift class R19/R20 exist to kill, wearing a different hat.
  5. Named `BP_<CppClassWithoutPrefix>` — `BP_BRGameMode`, not `BP_GameModeFinal2`.

  **What it still may NOT be:** a place to branch, to hold state, to react to an event, to
  override a virtual, or to carry a value the sim reads at runtime. The moment a BP child
  contains a decision, it is a Tier-4 asset holding gameplay the critic cannot diff — which is
  exactly R18's original target, and that instance is a `high`-severity finding, not a style note.

  **Enforcement is owed, not assumed.** R18 was reviewable because there were no assets to
  review; this exception spends that. A `Tools/audit_blueprints.py` must assert conditions 1–3
  mechanically (node count, added-member count, parent chain) over every `BP_BR*` asset and run
  in rung 2, or condition 2 erodes to "only a little logic" within a month. **Until that script
  exists this ruling is enforced by goodwill, and that is stated here rather than assumed.**

  *Corollary — the config path stays available and is preferred where it works:* every
  `EditDefaultsOnly` soft pointer can also be set from `Config/DefaultGame.ini` under
  `[/Script/Breachpoint.<Class>]`, which is diffable, greppable, and survives a clone with
  nobody opening the editor. Prefer ini for anything a script can set; use a BP child where the
  editor is genuinely the better authoring surface.

- **R27. The middle bot tier is named `Marine`. Not `Regular`.** Two sources disagreed and the
  disagreement was invisible: GDD §2.8's tier table, §5.3's cut order (which names Marine as the
  surviving profile), and `BREACHPOINT-GDD-FULL-CONCEPT.md:178` all say **Marine**; comments in
  `BRDataRows.h:617` and `BRBotBrain.h:250` say Regular. Design documents win over code comments.

  *Why this was worth a ruling rather than a shrug:* `LoadBotTables(..., TierPerSlot)` resolves a
  tier **by row name**, and tier names — unlike ambition names — are matched against no C++ enum.
  So either spelling imports perfectly cleanly and the failure appears at runtime as
  `LoadBotTables` returning false, which spawns **zero bots** while reporting nothing about why.
  A one-cell typo would have looked exactly like the bug BP08 already has.

  Both comment sites are to be corrected in the packet that lands `DT_BotTuning.csv`, or this
  recurs the next time someone reads the header instead of the GDD.

- **R28. A bot tier may differ ONLY on levers a player can read.** `sight_radius_m` (35.0) and
  `sight_fov_deg` (90.0) are identical across all three tiers and stay that way. Tier difference
  lives in reaction time, aim error, commitment duration and re-aim interval — things a human can
  observe and learn to beat.

  Three reasons, in increasing order of how much they cost to violate: "no privileged state" is a
  GDD §2.8 promise and perception radius is its closest analogue; 35.0 m is the arena's own
  `sightlines.max_length_m`, so a bot sees exactly as far as the map ever offers and never
  further; and — the mechanical one — **`sight_radius_m` is also the normaliser for
  `dist_to_target_norm` and `dist_to_rocket_norm`**, so varying it per tier would make identical
  world geometry produce different facts per tier, and every consideration weight would silently
  mean something different for each. That is precisely the illegibility R12 exists to forbid.

  *Corollary:* `sight_fov_deg = 90` currently rests on UE's **default** camera FOV, because
  `BRCharacter` sets `FirstPersonFieldOfView = 70` for the arms only and never sets the world FOV.
  If anyone sets the player's FOV explicitly, this column silently becomes a privilege or a
  handicap. Pin the player FOV, or this ruling is resting on an engine default nobody chose.
