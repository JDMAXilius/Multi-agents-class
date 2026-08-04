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
  A new tag under this rule needs no new ruling — the *name* is already decided here.

  **AMENDED 1 Aug 2026 by R23 — the naming is free, the write is not.** This paragraph
  originally ended *"a normal `Core/` change, not a re-opening of R17,"* which read as
  permission for any packet to add the tag itself. **R23 closed `Core/` for the five families
  other than `Ability.*` and `GameplayCue.*`, and `Event.*` is one of the five.** So a packet
  needing a new `Event.<Verb>.<Moment>` tag **files a `contract_gap` and stops**, exactly as
  `contracts/animation.md` §4 says. Two rules pointed opposite ways here for a day: an agent
  reading R17 would have written to `Core/` and been blocked by `guard_laws.py` mid-packet,
  because `Core/` is in no packet's `owner_path` but BP01's. R23 is later, is what the contract
  says, and is what the hook enforces — it wins.

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
  review; this exception spends that. An audit must assert conditions 1–3 mechanically (node
  count, added-member count, parent chain) over every `BP_BR*` asset and run in rung 2, or
  condition 2 erodes to "only a little logic" within a month.

  *Status, 1 Aug 2026 — the script exists and changes nothing yet.*
  `Tools/audit_blueprints/audit_r26.py` was written, but its packet was **stopped mid-flight:
  it is unreviewed, has never been run, and is not wired into rung 2** (`contracts/testing.md`
  does not mention it). Two of those three are the same problem — an audit nobody runs is an
  audit that does not exist. **So this ruling is still enforced by goodwill**, and the
  existence of a file must not be mistaken for enforcement. Wiring it into rung 2, and one
  green run against the five landed assets, is what closes this.

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

- **R29. One editor, one driver — and an open editor and a build must not overlap.** Three
  documents already cited this rule as "R21." **R21 does not say it.** R21 is about `TaskStop`
  orphaning spawned processes and UE's global *build* mutex; nothing in it mentions the editor.
  The rule those citations reached for is real, is load-bearing for every `editor-live` packet,
  and had no ruling — so it is cut here rather than left as three references to a rule that does
  not exist. (Found 1 Aug 2026 by a doctrine-consistency audit. A miscitation is worse than a
  missing rule: it reads as settled, so nobody checks.)

  **The rule, in three parts:**
  1. **One editor instance per project, ever.** UE takes an exclusive lock on the project; a
     second instance either refuses to start or opens read-only. This is the engine's behaviour,
     not our policy.
  2. **One driver per editor.** A running editor is a single mutable world. Two agents issuing
     MCP calls into it interleave with no transaction boundary — asset A half-created while
     asset B saves the level. The MCP exposes no locking, so "don't" is the whole mechanism.
     A session claiming an `editor-live` ticket owns that editor until it releases it.
  3. **An editor session and a build must not overlap.** The open editor holds `Binaries/Win64/`
     DLLs; UBT cannot replace a loaded module, so the build fails late and confusingly, or
     produces binaries the running editor will never load. **A session using the MCP must not
     dispatch a builder that compiles, and vice versa.** This is where R29 and R21 touch: R21
     says one build at a time, R29 says the editor counts as a party to that.

  *Cost of learning it the expensive way:* this burned real cycles on 1 Aug 2026 before anyone
  wrote it down (`TICKET_BP16_UE_MCP_BRIDGE.md` Notes).

  *Enforcement is honesty, not mechanism.* `guard_laws.py` gates `Edit`/`Write` by `file_path`;
  an MCP tool call has neither, and nothing in the repo can see whether an editor is open. This
  ruling is a rule the driver follows, and it is stated that way rather than assumed.

- **R30. Rung 4 has TWO topologies. 4a dedicated is the default; 4b listen is REQUIRED for any
  claim whose code path differs between host and remote client — and "the host is a client" is
  not one of the paths it differs on.** Founder ruling, 1 Aug 2026, resolving a contradiction
  the doctrine audit surfaced: `testing.md` and the `gauntlet-testing` skill specify rung 4 as
  *dedicated server, not listen* (the skill's §8 self-check **fails** a packet that adds a
  listen role), while `online-services.md` law 4 and `netcode.md` both make host-vs-remote
  testing a standing requirement on every netcode packet. **The requirement was unsatisfiable
  at the only rung meant to satisfy it**, and the slice's shipping topology — Steam listen
  server, invite-first — was the one configuration rung 4 could never run.

  **Neither side was wrong, which is why the answer is two configurations and not a swap.**

  *Why 4a (dedicated) stays the default:* it proves authority does not secretly depend on a
  local player. That is the exact bug class that turns the Phase-2 GameLift move (GL-3) into a
  rewrite instead of a config change, and it is why the skill's comment argued for it.

  *Why 4b (listen) is not optional:* **on a dedicated server no player is ever the authority.**
  Every client is remote, so predicted-ability and CMC code paths only ever execute on the
  client side of the predict/confirm split. On a listen server the host runs prediction **and**
  authority in one call stack. A `BRGA_*` whose prediction path silently never runs for the
  host, or an `FSavedMove_BR` flag that is only correct when a correction round-trips, is
  **invisible to 4a by construction** — not unlikely to be caught, structurally uncatchable.
  Host-quit has no dedicated analogue at all: there is nothing to quit.

  **When 4b is required (the test, not a vibe):** the claim touches a path that differs on a
  listen server. Those paths, enumerated so nobody has to judge:
  1. **Ability prediction / CMC prediction** — anything with a predicted client path (BP02's
     `BRGA_Sprint`, BP03's fire path, BP05, BP06's grapple).
  2. **Session lifecycle** — create, invite, join, leave, **host-quit**, backfill, travel.
     `online-services.md` law 4 already demanded this; 4b is where it becomes runnable.
  3. **Any claim phrased with the word "host"**, including host advantage.
  4. **Anything asserting a local player exists or does not** — `NM_ListenServer` branches.

  Everything else — pure server sim, damage arithmetic, match phase, scoring, bot decisions —
  is **4a only**. This is a targeted second run, not a doubling of the ladder.

  **What changes in the harness:** 4b is `RequireRole(Server)` with a local player (listen) +
  **one** remote client, not two. Assert-in-threes becomes assert-in-threes across **two
  processes**: server-authority view, host-local view, remote-client view — the first two share
  a process and must still be asserted separately, because collapsing them is precisely the
  mistake this rung exists to catch. A 4b scenario that only compares two viewpoints has not
  run rung 4b.

  **BLOCKED still applies, per axis.** A packet needing 4b reports **4b BLOCKED with the reason**
  rather than passing on 4a alone. 4a green is not 4b evidence, and a ticket that names both and
  reports one is an incomplete rung, not a pass. (Both are BLOCKED today anyway — Gauntlet does
  not compile on the workstation, `BP00` Log 31 Jul.)

  *Ordering note:* this ruling lands **before** BP00 step 3 wires Gauntlet, deliberately. Wiring
  a harness against a topology we do not ship and retrofitting the other one later costs more
  than building both role configurations once, while the test node is still being written.

## Crew coordination (1 Aug 2026 — paid for by three blocked packets in ninety minutes)

- **R31. A claim may name a SET of tickets sharing one window. `owner_path` is their union, and
  amendments are ADDITIVE ONLY.** Founder ruling, 1 Aug 2026, resolving the decision
  `WORK-ROUTING.md` §7 filed as owed. §7 offered two candidates; this takes **(a) window claims**.

  **The problem it solves, stated from what happened rather than from theory.**
  `.claude/active-packet.json` is ONE file in ONE shared working tree. `WORK-ROUTING.md` §5.6
  says *"a claim is per session."* It is not — it is per tree, last-writer-wins, with no
  ownership and no arbitration. In one session on 1 Aug:
  1. A BP15 claim silently confined an unrelated BP16 session to BP15's paths.
  2. A BP16 claim, written while free and verified live with a rejecting case, was **silently
     overwritten by a BP03 claim ~90 seconds later** while the BP16 packet was mid-edit.

  Instance 2 is the one that makes this a correctness ruling and not a scheduling preference:
  **two sessions can each believe they hold the claim**, and the hook faithfully enforces
  whichever was written last against whichever session calls a tool next. Neither session can
  see the other. The failure is silent on both sides.

  **The mechanics are already in place — verified against `guard_laws.py`, not assumed:**
  - `owner_path` is read as a **list** and matched with `any(...)` (lines 71–73), so a union of
    several tickets' paths works with **no code change**.
  - `ticket` is used **only** in the block message's f-string (line 76), so a list value renders
    as `['BP03', 'BP16']` and is harmless.
  - §7 predicted exactly this ("only the `ticket` field's meaning changes"). It was right.

  **The obligations, which are the actual content of this ruling:**
  1. **Additive only.** A session joining a window **adds** its ticket and its paths. Removing
     or replacing another packet's paths is forbidden — that is the instance-2 harm pointed the
     other way, and it would block a live packet mid-write for a reason it cannot see.
  2. **Read before you write.** Never write the claim file blind. Read it, union, write.
  3. **Leave additively too.** A session finishing its packet removes **its own** ticket and
     paths; if it was the last, it deletes the file. It never truncates the file to "release".
  4. **A window is one lock mode.** `WORK-ROUTING.md` §1's OPEN/CLOSED modes are exclusive; a
     window claim may not span both, because the resource — not the claim — is what serializes.

  **What this ruling does NOT do, said out loud because the ledger's job is naming the gap:**
  a union **weakens confinement**. Under a BP03+BP16 window, the BP03 lane is mechanically free
  to write `Tools/ue_mcp/`. Nothing stops it. The union exists so two lanes can **coexist**, not
  so either may write the other's files — that remains law 5, enforced by the agent's own
  discipline and by review, exactly as it was before there was a hook at all. **Precision was
  traded for coexistence, deliberately.** If that trade later proves wrong, §7 option (b) — a
  separate lock-holder file with an explicit owner — is the fallback, and it was rejected today
  for moving parts, not for being unsound.

  **A correction this ruling carries, because it was asserted in this session and was false:**
  a blocked packet is **never** unable to file its `contract_gap`. `guard_laws.py`'s
  `ALWAYS_ALLOWED` (line 31) exempts `docs/tickets/`, `docs/DESIGN-RULINGS.md`, and the claim
  file itself from confinement **regardless of claim** — the docstring says so explicitly. A
  session that reports "I could not even file the gap" has read the block message instead of the
  hook. *That mistake is the same shape as the four this project has already catalogued: a
  mechanism's behaviour inferred rather than fired at.*

## Delegated batch, 1 Aug 2026 — R32–R36

**Authority note, recorded because law 8 makes it matter.** The founder delegated these
standing: *"decide always on your recommendation and continue working."* Each was filed as a
recommendation in `docs/DECISIONS-OWED.md` first, with its trade-off written down; each is
recorded here as the ruling taken on that recommendation, not as a lead widening its own remit.
**Rulings are closed once made** — if one is wrong it is amended by a later R-number, never
re-litigated inside a packet.

- **R32. The architect's `depth` term is SUBTRACTED, not added.** BP15 step 2 spec'd four terms
  and left the sign implicit; adding depth made the score reward *distance from a root*, which is
  distance from being startable. Proven rather than argued: the corrected score's top pick was
  `BRSpotterSubsystem` — BP11, gated by BP08, gated by BP02+BP04 — winning on **depth 4 alone**,
  while the three test specs (depth 0, startable) ranked 7–9 in the same table that reported
  rung 2 BLOCKED *because* `Tests/` was empty. A "what to build next" score ranks **startable**
  work first. This does not bury deep units: `blockers` still lifts a unit many others wait on.

- **R33. A fifth score term, READINESS, and it is a GATE.** A unit whose declared inputs do not
  exist cannot be built, however valuable it is. `BRGA_WeaponFire` ranked #1 while three of its
  inputs were missing, and the builder packet stopped at law 5 on contact. Readiness is computed
  **mechanically** — declared-but-absent tags, row fields and CSV columns named by the unit's own
  §3 spec — never by a model.

- **R34. A score term expressing IMPOSSIBILITY is a gate and uses a magnitude that dominates.**
  Generalised from three bugs with one shape: `state` at 2/1/0 was swamped by a blocker term
  reaching 35 and the score picked an already-BUILT unit; `tier` needed −100 to stop a
  perpetually-MISSING Phase-2 unit being selected; readiness needs the same. **"This cannot be
  built" is not a preference to be outvoted.** Preference terms (depth, blockers) stay small;
  gate terms (state, tier, readiness) use magnitudes no sum of preferences can overcome.

- **R35. An include edge counts only if the included header EXISTS.** Step 6's F3: adding one
  line — `#include "BRGA_Grenade.h"` — to `BRCore.h` moved that unit +27 and to #1, and **the
  included header does not exist**. The include cannot compile and the scorer accepted it, so two
  of four terms were writable by the same builder the score directs. The graph stays (step 2
  names it); an edge whose target is absent from disk is not evidence. *Not closed by this:* a
  builder can still add a **valid** include to move its own unit. The score is only as
  trustworthy as the tree, and `check_authorisation.py` is what catches a landing it did not
  authorise.

- **R38. One log channel per §3 DISCIPLINE FOLDER. A sub-folder inherits its parent's; it does
  not get its own.** Asked by the cue-library packet, which wanted `LogBRCues` for
  `AbilitySystem/Cues/` and — correctly — refused to widen R24 by writing it in.

  R24's rule is *"a discipline folder that needs to speak gets `LogBR<Folder>`"*, and its own
  arithmetic names the unit: *"five channels for twelve discipline folders."* §3's tree
  enumerates exactly twelve, all top level. `AbilitySystem/Cues/` is a sub-folder of §3.3, so
  the channel R24 would mint for it is `LogBRAbilitySystem`, not `LogBRCues`.

  **Decided against the sub-folder channel for a reason that is not tidiness.** R24 exists
  because `Input/` could not speak *at all* — BP01 fell back to development-only `ensureMsgf`
  and its Done-when box 4 ("log-proven") was unsatisfiable. `AbilitySystem/` speaks fine;
  `LogBRCombat` carries all eleven files of §3.3. Granting a second channel there makes
  `BRCore.h`'s own claim — *a filtered log reads as one subsystem* — **worse**, because a reader
  filtering `LogBRCombat` would silently stop seeing cue diagnostics and would not know a second
  stream existed. And it sets the precedent by which `Abilities/` and `Effects/` each claim one,
  leaving §3.3 with four.

  *The real pain is real, and it is a verbosity problem, not a channel problem:* a cue's
  empty-slot warning currently lands in the same stream as the damage pipeline. The cue library
  already bounds it to **once per (tag, slot)**, which is the right shape. If it still drowns
  the stream, the answer is verbosity or a message prefix — not a channel a filter can miss.

  *Recorded, not fixed:* R24's premise is already only approximately true. `LogBRCombat` and
  `LogBRNet` map to no §3 folder and both span several, so the existing five are **discipline
  -themed**, not folder-named. That inconsistency predates this ruling and is left alone
  deliberately — renaming live channels to satisfy a rule about new ones would be the tail
  wagging the dog.

- **R36. R29.3 is widened: an editor session must not overlap ANYTHING THAT TAKES THE PROJECT
  LOCK, not merely "a build".** R29.3 named the operation this project rarely runs and missed the
  one it runs constantly — every editor-driving tool we own is a `-run=pythonscript` commandlet
  that takes the lock without being a build. **Demonstrated the same day:** with an editor open,
  `run-ubt.ps1 -Targets BreachpointEditor` compiled every translation unit and linked the `.lib`,
  then failed `LNK1104: cannot open file … UnrealEditor-Breachpoint.dll` because the editor held
  it. The compile was fine; the *lock* was not. It had bitten nobody earlier only because
  `build-input.ps1:180` and `rename-r26.ps1:88` guard on *"any editor process is live"* — the
  correct test — rather than on R29.3's text. **A ruling whose wording is narrower than the
  guards implementing it will eventually be read instead of the guards.**

## Authoring execution (1 Aug 2026 — forced by BP18's first step, not decided in the abstract)

- **R37. The MCP MAY execute an asset step. The committed plan plus a receipt is the reviewable
  artifact — never the asset alone.** Founder ruling, 1 Aug 2026, taking BP16 step 2's proposal
  **(a) MCP-as-executor**. `DECISIONS-OWED.md` carried this in the D-series with no R-number;
  BP18 operated under (a) as the *conservative reading* while unruled. It is now ruled, and the
  conservative reading is the actual one.

  **Why it could not stay owed.** BP18's Kickoff says `requires: editor-live`. Every generator
  the ticket names — `rename-r26.ps1`, `build-input.ps1` — carries an R21 guard that **refuses
  to launch while any editor process is live**. Both cannot hold: the ticket demands an open
  editor and its own tools demand a closed one. Steps 1–3 had no path that satisfied both. The
  founder was offered close-the-editor-and-run-headless (law-7 clean today, no ruling needed)
  and chose **editor open, MCP executes**, which is what forced the R-number.

  **The obligation, which is the whole content of this ruling.** For every asset landed by MCP:
  1. **A committed plan specifies it first.** The plan is a file in the repo — `rename_r26.py`,
     `input_plan.py`, `arena_manifest.json`. The MCP replaces the `unreal`-importing half of a
     generator, **not** the deciding half. An MCP call with no committed plan behind it is
     hand-placing with a different hand and is a `high` finding.
  2. **A receipt names every call and its result**, and is committed with the asset. The critic
     cannot diff a `.uasset`; the receipt is what it reviews instead.
  3. **Law 7 is NOT repealed.** Tier is still answered before the first call: if it is not
     Tier 4 of `BREACHPOINT-AUTHORING-MATRIX.md`, the answer is still C++ and the step is wrong.

  **The gap this ruling makes load-bearing, named out loud.** `guard_laws.py` gates `Edit`/`Write`
  by `file_path`. **An MCP tool call has neither** — so every mechanical protection this project
  owns is blind to asset authoring, exactly as BP16 warned. Before R37 that blindness was
  theoretical because no agent was permitted to land an asset that way. It is now the primary
  authoring route, and the only thing standing where the hook stands elsewhere is **receipt
  discipline and review**. That is a real reduction in enforcement, accepted deliberately, and
  a session that lands an MCP asset without a receipt has defeated the only control there is.

## Architecture manifest (3 Aug 2026 — D11, forced by BP60's budget parser)

- **R39. Real `BR*` C++ under `Source/` is either a NUMBERED UNIT in §3 or a NAMED EXCLUSION in
  §4 — silence is not a third option.** A unit's §3 declaration carries its **form**
  (`X.h/.cpp`, `X.h`, `X.cpp`), because `architect.py` classifies against the declared form: a
  header-only class declared as a pair reports finished work as a STUB and offers it up to be
  built. An exclusion is written **by name, with its reason, and WITHOUT a file extension** — the
  extension is what §3's `UNIT_RE` uses to tell a declaration from a reference.

  **Applied by BP61 (D11(b)):** `BRRootLayout.h/.cpp` and `BRUISettings.h` declared in §3.9
  (4 → 6); `BRUITypes` and `BRCombatCurves` excluded by name in §4 item 3. §3 sum 43 → 45,
  budget 44 → 46, asserted by `--all` against §4's `| **Total budget** |` row.

  **Enforced mechanically, not by prose.** `undeclared_files()` skipped only
  `declared | {GE_HEADER.stem}`, so a §4 named exclusion was still printed as UNDECLARED
  forever — a rule that read as enforced and was not, this project's most-repeated defect class.
  BP61 added `RULED_EXCLUSIONS` alongside it, printed with the other declared exclusions and
  self-checked: naming a class there that has no header on disk **fails the run**, because an
  exclusion for a file that no longer exists hides nothing and misleads everyone.

  **The limit, named because it is large.** R39 states the standard; it does not claim the
  codebase meets it. On the day it was written `--all` reported **41** undeclared `BR*` headers
  after D11's four were resolved — 37 in `UI/`, plus `BRGA_Jump`, `BRGameplayCues`,
  `BRExplosion`, `BRProjectile` (the subject of the still-open **D6**), and a `Camera/` folder
  that §3's tree does not list and §9's owner-path map does not own — so
  `BRPlayerCameraManager` has no owner under law 5. **D11 is decided; the drift it was a sample
  of is not.** R39 is what the next audit is judged against, not a description of today.

  *A near-miss recorded because the wrong version was briefly committed:* the no-extension half
  of this rule was first justified by claiming §4 falls inside §3.12's parse span. It does not —
  `parse_manifest` ends the last section at `text.find("\n## 4.", ...)`, which resolves. Proven
  by injecting extensions into §4 and re-parsing: unchanged, 45. The convention is still right,
  but it rests on the **end-of-file fallback** firing if that heading is ever renamed, not on a
  live bug. The first write also said "verified by running it" without that run having happened.

## Third-party content (3 Aug 2026 — founder ruling, amends R18's scope)

- **R40. Sourced third-party content is IN SCOPE and may ship, including Blueprint widgets.
  R18's "zero Blueprint classes" governs what THIS PROJECT AUTHORS, not what it adopts.**
  Founder ruling, given directly: *"I do not care to use others' assets that we did not make
  from scratch, we need to take advantage of everything."*

  **What changed.** R18 + R26 were written against work we write ourselves, where a Blueprint
  is a choice and C++ was always available. A sourced pack is not that choice — it arrives as
  Blueprints and the alternative is not "the same thing in C++", it is *not having it*.
  Reading R18 as a ban on adoption cost capability for no enforcement benefit.

  **What did NOT change.** Everything we author still obeys R18/R26: a `BR` class is C++, a
  GE is a C++ class, a cue handler is a C++ class, and `BP_<Cpp>` defaults-only children
  remain the single authored-Blueprint exception. R40 is a door for INCOMING content, not a
  reclassification of our own.

  **The boundary, so this is not read as "Blueprints are fine now".** Sourced content is
  adopted AS IS and lives in its own folder. The moment we need to change its behaviour, the
  behaviour moves to C++ and the asset keeps only what UE has no C++ path for (Tier 4). A
  sourced Blueprint that we start editing is authored work wearing a costume, and R18 applies
  to it again in full.

  **Applied 3 Aug 2026:** `Content/UI/Crosshair/WBP_DynamicCrosshair_{01,02,03,Base}` + their
  `Images/` set (~1.1 MB, ~June 2025). `WBP_DynamicCrosshair_Base` derives from
  `UMG.UserWidget` and carries its own widget tree — under R18 alone that was a `high`
  finding; under R40 it is adopted content. They sit beside `WBP_ReticleWidget`, which stays
  the C++-backed reticle the HUD binds (`UBRReticleWidget`, generated by
  `Tools/gen_ui/build_wbp.py`). **Both exist; neither replaces the other yet.** Which one the
  HUD actually uses is a separate call and is NOT decided here.

## Third-party code, as distinct from content (3 Aug 2026 — founder ruling)

- **R41. A third-party repository's LICENCE is judged BEFORE its contents, and it can refuse
  adoption outright. R40 opened a door for sourced CONTENT; it did not repeal copyright.**
  Founder ruling, given directly on `vinceright3/FrontendUIProgramming`: *clean-room, take the
  capability, take none of the code.*

  **What forced the ruling.** That repo was proposed for wholesale adoption ("put everything
  into our main project to make it ours"). Its `LICENSE.txt` reads, verbatim: *"All code should
  only be used for informational purposes… can only be used for informational purposes and not
  for commercial use of any kind. Whoever wants to use the code for commercial purposes must
  contact the author Vince Petrelli for permission."* That is all-rights-reserved with a
  read-to-learn carve-out. It grants no right to copy, modify or redistribute. Vendoring it
  would have been infringement the moment BREACHPOINT ships, and `THIRD-PARTY-NOTICES.md` could
  not have cured it — there is no notice that makes a non-commercial licence commercial.

  **The rule.** Before any external repository, pack or plugin is adopted, its licence is read
  and recorded. Permissive (MIT/BSD/ISC/Apache/OFL/CC0) or purchased-with-a-commercial-grant →
  R40 applies and it may land, notice-in-the-same-commit. Non-commercial, all-rights-reserved,
  GPL-family, or absent → it does NOT land, in any form, however convenient. A licence that
  cannot be found is a licence that does not permit anything.

  **Clean-room is the permitted alternative, and it has a protocol**, because "we looked at it
  and wrote our own" is only a defence if the boundary is real:
  1. Reference material may be READ (that is what "informational purposes" grants).
  2. What crosses the boundary is a BEHAVIOURAL SPEC — states, rules, edge cases — written in
     our own words, carrying no source, no structure and none of the original's names.
  3. Implementation proceeds from the spec. The reference is deleted from the working tree
     before implementation starts, and is never committed at any point.
  4. A `critic` REFUTER pass looks for structural copying — identical decomposition, tell-tale
     identifiers, imported typos — and a hit is a `high` finding.

  **Applied 3 Aug 2026 (BP78):** the settings registry, key rebinding and loading-screen
  systems were built as `BR` C++ against UE 5.8 under this protocol. Nothing from that repo is
  in this one — no `.cpp`, no `.uasset`, no 464 MB of its Content, and `THIRD-PARTY-NOTICES.md`
  is deliberately UNCHANGED by that packet. A diff there would have been the signal the
  boundary was crossed.

  **Not decided here:** whether to seek a written grant from the author. That remains open and
  would only ever ADD the option of lifting code verbatim; it changes nothing about what has
  already been built.
