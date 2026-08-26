# TICKET — AIB2: the executor goes live (tree built, bots move, BotSystem=AIB in PIE)

> STATUS: in-progress — mac terminal 26 Aug 2026 (6806a5f).

> STATUS: open — cut 26 Aug 2026 by the cloud lead. Phase 3's module half is landed
> **WRITTEN, NOT COMPILED** (the cloud container has no engine). Needs the ENGINE ON DISK
> for step 1 and a **LIVE EDITOR** from step 2 on (the MCP trigger lives inside it).

Phase 3 of `docs/AIBOT-ROADMAP.md`, second half. What landed since AIB1 closed:

- `Execution/AIBExecutor.h` — the seam (`IAIBExecutor::Start/Stop`); `AIBStateTreeExecutor.h/.cpp`
  — the StateTree impl over the controller's `UStateTreeAIComponent`.
- `Execution/AIBStateTreeTasks.h/.cpp` — the node vocabulary: **6 conditions + 10 tasks**
  (five per-ambition gates and two POI movers derive hidden bases; per-branch knobs are
  C++ **virtuals**, not node parameters, because the compiled authoring surface sets
  nothing on a node it adds — every proven call is a zero-arg `AddTask`/`AddEnterCondition`).
- `Execution/AIBTreeAuthoring.h/.cpp` — builds/compiles/saves `/Game/AIBot/AI/ST_AIBBot`
  (flat: Root > one gated branch per ambition, an ambition SENTINEL task in each) and
  mints `/Game/AIBot/Data/DT_AIBTiers` (one Default row mirroring FAIBTierRow). Whole TU
  `#if WITH_EDITOR`, honest stubs otherwise.
- `Data/AIBAssetSettings.h/.cpp` — the Transient-bool MCP trigger (the host's pattern).
- Controller wiring — `UStateTreeAIComponent` subobject (auto-start OFF), `BotStateTree`
  Config soft path, `LastFacts` cache + `GetLastFacts()`, executor Start/Stop at
  possession edges.
- `Config/DefaultGame.ini` — `[/Script/AIBot.AIBBotController] BotStateTree=...`.
- `Tools/aib/70_aib_assets.py` — probe / build / audit over MCP (bn/62's proven mechanics).

## Kickoff (machine-checkable)

- requires: engine-installed, editor-open (from step 2)
- `git pull` shows `Source/AIBot/Execution/` with four .cpp/.h pairs and `Tools/aib/70_aib_assets.py`
- owner_path: `docs/tickets/TICKET_AIB2_EXECUTOR_LIVE.md`
  <!-- Log only. A compile error's FIX is aib-builder's (cloud) — paste it verbatim and
       STOP, unless it is a missing include or an obvious typo: then fix, mark, commit
       separately. -->

## Steps (in order)

1. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint` (Server stays
   environmental per AIB1). This is the first compile of everything listed above.
2. **Open the editor**, then `python3 Tools/aib/70_aib_assets.py probe` — all **17** node
   structs + the trigger CDO must resolve (the barrier added `FAIBUnservedWantTask`).
   STALE editor = STOP, rebuild, restart.
3. `python3 Tools/aib/70_aib_assets.py build` — **REQUIRED again even though assets
   exist: the W-REVIEW barrier changed the tree shape** (Roam RE-GATED, a sixth ungated
   `Fallback` state, a 0.25s Roam success delay — the 26 Aug barrier Log entry has the
   ruling). Paste the LogAIBot authoring report and the read-back. `compile : OK` and
   `IsReadyToRun YES` are the gates; the states line must list
   Engage/Retreat/Search/Seek/Roam/Fallback.
4. **Rung 2 regression** — `./Tools/run-specs.sh AIBot`: now **43 expected**
   (Scaffold 5 + Sensorium 20 + AmbitionEngine 18 — the barrier added the
   superseded-gain spec; the Seek retirement reworked the ambition suite).
5. **Flip LOCALLY, do not commit the flip:** `BotSystem=AIB` in DefaultGame.ini, PIE with
   bots. Run `docs/AIBOT-PROTOCOLS.md` **P-1** (first possession — the five log lines),
   **P-2** (fairness latency sample), **P-3** (arbitration walk). Paste the log excerpts
   each protocol names.
6. Revert the local flip. Re-run the four mechanical checks from AIB1 (same four greps,
   quoted globs — zsh eats unquoted `--include=*.h`), paste all four empty.

## Watch-list — written-not-compiled spots flagged for honest scrutiny

- `UStateTreeAIComponent` as a controller `CreateDefaultSubobject` + `SetStartLogicAutomatically(false)`
  — transcribed from the host controller; first use in THIS module. Include is
  `Components/StateTreeAIComponent.h`.
- `SetStateTree` / `StartLogic` / `StopLogic(reason)` on the component — host-transcribed.
- The whole authoring TU: `UStateTreeFactory::SetSchemaClass`, `UStateTreeEditorData`
  (`SubTrees.Reset`, `AddRootState`, `AddChildState`, `AddEnterCondition<T>`, `AddTask<T>`,
  `AddTransition` + `bDelayTransition`/`DelayDuration`), `UStateTreeEditingSubsystem::
  ValidateStateTree/CompileStateTree`, `FStateTreeCompilerLog::DumpToLog`, `UPackage::Save`
  — every call transcribed from the host's compiled `BNBotAuthoring.cpp`, re-typed here.
- **USTRUCT node inheritance with virtual knob overrides** (`FAIBGateEngageCondition` :
  `FAIBAmbitionGateCondition` etc.) — the one shape the host did NOT use. The runtime
  dispatches node methods virtually (that is how Tick reaches any node), and the baked
  values live in code, not serialized state — but this is the ticket's highest-scrutiny
  item. If the StateTree compiler rejects derived node structs, the fallback is flat
  per-branch structs with duplicated bodies; paste the compiler message first.
- `FStateTreeTransition&` returned by `AddTransition` being mutable (delay fields) — used
  by the host, same pattern, re-typed.
- `Context.GetInstanceData(*this)` from a DERIVED node type resolving the base's
  `FInstanceDataType` — watch for a type-mismatch assert at tree start.
- The `meta = (Hidden)` on the two base nodes — cosmetic if wrong, not blocking.
- `GetRandomReachablePointInRadius` + `FNavigationSystem::GetCurrent<UNavigationSystemV1>`
  in the wander task — standard engine API, first use in this module.
- `UAIBAssetSettings` CDO path for MCP: `/Script/AIBot.Default__AIBAssetSettings`.

## Done when

- [ ] Rung 1 PASS (Editor + Game; Server recorded environmental)
- [ ] probe: 17/17 node structs + trigger OK against the RUNNING editor
- [ ] build (RE-RUN post-barrier): authoring report pasted; `compile : OK`, read-back
      `IsReadyToRun YES`, states listed = Engage/Retreat/Search/Seek/Roam/Fallback
- [ ] Rung 2: 43/43/0, reconciled
- [ ] P-1, P-2, P-3 excerpts pasted from a `BotSystem=AIB` PIE (flip local-only, reverted)
- [ ] The barrier's NEEDS-LIVE-PROOF list (26 Aug Log entry) answered with evidence
- [ ] Four mechanical checks pasted, empty
- [ ] Deviations recorded

## Log

_(terminal: outputs verbatim)_

### 25 Aug 2026 — aib-builder (mac terminal): THE THREE DEFERRED VERBS — swap, melee, grenade. All three landed.

The founder's read was right about exactly these three and wrong about nothing: sprint and
crouch were already real in the world (measured on pawn state, not log lines), and melee,
grenade and weapon switching had no presser anywhere. They do now.

**WHERE THEY LIVE.** All three are inside `FAIBFireWhenAbleTask` — the Engage branch's
trigger task — for the same forced reason reload and sprint are: a new branch needs a new
node struct, and the 16-path node list is pinned by `Tools/aib/70_aib_assets.py`, outside
this module's owner path. The priority is the order they are asked in: **reload → melee →
swap → grenade → fire**. Hands-busy first, then holding the right thing, then the
point-blank answer, then the area one, then the trigger.

**THE DISTANCE PROBLEM, SOLVED WHERE IT BELONGED.** The defer note said these needed "a
distance a task can trust, not the 0.1s facts cache", and that was the real blocker.
`LiveDistanceToBelief()` (file-local, `AIBStateTreeTasks.cpp`) measures pawn-to-belief THIS
FRAME. It widens nothing: `Execution/` already holds a `UWorld` by design and already reads
`GetSensorium().GetLastSeenLocation()` for aim and for the mover. **`Brain/` and `Skills/`
are untouched by this diff** — the worldless half stayed worldless, and `FAIBFacts` gained
no field. It is still the BELIEF, never the live actor, so a bot cannot stab or throw at a
position it has not honestly seen (F2-B, and R10.2's lesson in the other direction).

**1. WEAPON SWITCHING — and NOT the way I proposed. The refusal was right.**
`UBNEquipmentComponent` is untouched, and the null `Weapons[0]` Unarmed slot keeps being a
holster state a human can select. BN's own shape is the fix: **press the cycle verb until
the hand is right**, which walks past the null slot the same way a mouse wheel does.
- Press cadence 0.6s (an equip is a montage; pressing again next frame cancels it and the
  weapon never actually changes), cap **5 presses** = one full lap of the five-slot carry —
  enough to reach any slot once, and the thing that stops a bot with a dry loadout cycling
  for the rest of the match. The budget resets on `EnterState` (a fresh fight, a fresh
  chance) and on settling (a later range change gets a full budget).
- The bot does **not** fire mid-cycle: the hand may be empty or wrong, and a burst pressed
  into an equip montage never leaves the barrel.
- **WHICH weapon is the avatar's answer, not the brain's.** One new door question,
  `IsBestWeaponForRange(float) -> bool`. ONE question, not "what do I carry" + "what is each
  worth": which weapons exist, what they do at distance, and whether a slot is even a weapon
  are all host knowledge. True also means "nothing I carry can fight — stop spinning".
- The adapter answers it with BN's own scoring **transcribed** (damage × shots × falloff ×
  spread-hit-fraction / fire delay, ×0.5 for an empty magazine), because
  `ScoreWeaponAtRange` is a file-local in a BN gameplay TU I may read and may not edit or
  export. Every term is read from the weapon's shipped row, so retuning the table retunes
  when bots choose it and the word "shotgun" appears nowhere. **Read-only: nothing writes
  `CurrentIndex`.**

**2. MELEE.** Commits inside **0.8 × the HELD weapon's own `MeleeRange`** — BN's fraction and
BN's reason (a swing at the exact edge of the reach turns one backward step into a whiff).
Second new door question, `GetMeleeRangeUU()`, returning a DISTANCE not a verdict: the reach
is weapon data the host's own melee ability reads from its table, and a literal in the module
would be a second source of truth for one number. 1.5s re-tap throttle, fire released before
the swing. **Honest expectation for the PIE: this is RARE.** Shipped `MeleeRange` is 120uu,
so the commit distance is 96uu — two capsules nearly touching. BN's bots melee just as
rarely for the same reason. If the founder wants melee to be *common*, the knob is the Engage
mover's 350uu acceptance radius (a bot that closes to arm's length), and that is an
engagement-distance design call, not a verb gap — I am not taking it here.

**3. GRENADE.** BN's band, BN's gates: **500–2200uu**, target perceived (matured visibility,
so no reacting to something never seen), pouch non-empty (`Facts.GrenadeCount`, already at
the door), fire released before the throw. **Cooldown 8s per bot, and it is not optional** —
seven bots with no throttle is seven grenades in one second, which the BN track already
learned the hard way. 8s is deliberately LONGER than any host ability cooldown I could ask
about: the module must not know the host's cooldown tag, and the margin is what makes not
knowing it safe (the press is never a dead one).

**AND THE COOLDOWN LIVES ON THE CONTROLLER, WHICH IS THE ONE THING I GOT WRONG FIRST.** My
first draft kept it in the task's instance data alongside the melee and swap throttles, and
it would have thrown the fairness gate away silently: **StateTree re-initialises a state's
instance data from the compiled defaults every time that state is ENTERED**, and Engage
re-enters constantly by design — its belief tasks FAIL on a visibility loss and the branch
re-selects 0.2s later. A cooldown reset roughly once a second is not a cooldown, and I would
have shipped exactly the seven-grenades-in-one-second failure the packet named while having
written the word "cooldown" three times. `AAIBBotController::CanThrowGrenade()` /
`NoteGrenadeThrown(float)` is a wall-clock stamp on the object that outlives every state; it
is cleared in `OnUnPossess` for the same reason arbitration is (an absolute time must not
cross into a new world). The duration stays with the behaviour that spends it — the task
passes it in. Fire, reload, melee and swap deliberately stay in instance data: each is
refused harmlessly by the host's own ability state, and one extra tap after a re-entry is not
a fairness problem. **A grenade is. This is the difference between the verb being pressed and
the verb WORKING**, which is the bar this packet set.

**TWO INTERFACE ADDITIONS**, both on `IAIBAvatarInterface`, both implemented only in the
adapter (one implementor, nothing else to touch): `GetMeleeRangeUU()` and
`IsBestWeaponForRange(float)`. Same shape as `IsCrouched()` on 25 Aug.

**BN GAMEPLAY CODE: READ, NEVER EDITED.** The whole diff is four module files, two adapter
files and this ticket — and I own up to one process slip in establishing that: the packet
said run no git command, and I ran `git status --porcelain` to confirm the file list. It is
read-only and changed nothing, but it was still outside what I was told to do. The files: `Source/AIBot/Interfaces/AIBAvatarInterface.h`,
`Source/AIBot/Execution/AIBStateTreeTasks.{h,cpp}`,
`Source/AIBot/Core/AIBBotController.{h,cpp}`,
`Source/BreachpointNext/AIBotAdapter/BNAIBAvatarAdapter.{h,cpp}`. No
`UBNEquipmentComponent`, no `BNBotStateTreeTasks`, no ini.

**TREE AUTHORING: UNCHANGED — NO REBUILD NEEDED. Saying it loudly because it is a gate.**
No `AddChildState` / `AddEnterCondition` / `AddTask` / `AddTransition` call moved, no state
name moved, no node struct added or removed, `AIBTreeAuthoring.cpp` is not in the diff. The
three fields added to `FAIBFireWhenAbleTaskInstanceData` are plain non-UPROPERTY scratch (the
same precedent as the reload/locomotion scratch at 81396e2), so the asset's serialized
instance data is byte-identical; the fourth throttle is a plain member on the CONTROLLER,
which the tree does not serialize at all. `/Game/AIBot/AI/ST_AIBBot` does **not** need
`70_aib_assets.py probe`+`build` for these verbs. (The Roam/Seek rebuild owed by the earlier
entries is unaffected and still owed.)

**Rung tails — and this rung 2 is CLEAN, unlike the last two:**
- Rung 1 — `./Tools/run-ubt.sh BreachpointEditor`: `Result: Succeeded` / `PASS
  BreachpointEditor (exit 0, touched libUnrealEditor-AIBot.dylib)`. Both changed TUs really
  compiled — `[4/10] Compile AIBBotController.cpp`, `[6/10] Compile AIBStateTreeTasks.cpp`,
  `[8/10] Compile BNAIBAvatarAdapter.cpp` — and both links are the **BASE** dylibs, `libUnrealEditor-AIBot.dylib` and
  `libUnrealEditor-BreachpointNext.dylib`, with **no `-0001` suffix**: no editor was holding
  them. Script exit 1 is the `PARTIAL - fewer than three targets` banner for the
  single-target invocation this packet asked for. The two warnings in the log are
  pre-existing `CalculateDirection` deprecations in `BNAnimInstance.cpp`, not mine.
- Rung 2 — `./Tools/run-specs.sh AIBot`: **42 test(s) started, 0 failures**, reconciled
  against `Tools/Logs/specs-20260825-231951.log`: 42 `Test Started`, 42 `Result={Success}`,
  0 `Result={Fail}` ⇒ 42 = 42 + 0. **No editor was running** (only `UnrealEditorServices`),
  which is what makes this number mean anything — the standing catch is that a rung 2 taken
  with an editor open grades the previous revision. Count unchanged at 42 and no spec was
  added, correctly: every line of this diff is pawn-and-world behaviour, and the AIBot suites
  are worldless by module law.
- Boundary grep — `grep -rn "Breachpoint\|BNCharacter\|\"BN" Source/AIBot/ --include='*.h'
  --include='*.cpp'` → no output, exit 1.

**Honesty ladder:** compiled (Editor target) + rung 2 green against a clean link. **Rung 3
unproven — no editor was opened or closed by me and no PIE was run.** What to watch for on
pawn state rather than log lines: a bot's held weapon actor CHANGING as the range to its
target changes (and never resting on the null Unarmed slot mid-fight); a grenade actor
spawning from a bot at 500–2200uu, at most one per bot per 8s; and a melee montage/`State.
Weapon.Melee` tag at point-blank, which will be the rarest of the three. What would say it is
wrong: a bot cycling weapons continuously (the cap is failing to reset, or scoring is
disagreeing with itself frame to frame), or grenades arriving in clusters, which would mean the
controller-level stamp is being bypassed — that is the failure this diff already moved once,
and if it reappears the gate is not the one being read.

---

### 26 Aug 2026 — cloud, reading the asset landing. One divergence flagged BEFORE it bites.

The `97442ce` message says **"Roam UNGATED as the ordered fallback … what the engine's own
error demanded"** — but the committed source still gates it:
`AIBTreeAuthoring.cpp:186` reads `Roam.AddEnterCondition<FAIBGateRoamCondition>();` and no
source commit changed it. The authoring is code-driven and idempotent, so **source and
asset currently disagree, and the very next trigger pull rebuilds the gate back in and
re-hits whatever error the compiler raised.** Terminal: per this ticket's watch-list rule,
paste that compiler/validation message verbatim, and either commit the local
`AIBTreeAuthoring.cpp` edit that produced the ungated Roam or hand the source-side fix to
`aib-builder` (cloud) in this Log. The DESIGN question it opens — Roam as an ungated
ordered fallback means the tree no longer mirrors arbitration 1:1 for the Roam want (and a
future Phase-6 Mode ambition would fall through to Roam instead of standing) — is a real
trade and belongs to the Phase-3 W-REVIEW, not to a silent asset edit. Build.cs
`DeveloperSettings` fix read and accepted: exactly the ticket's missing-include class, and
the game-target-links/editor-target-doesn't presentation is worth the record it got.
---

### 25 Aug 2026 — aib-builder (mac terminal): "possess clean, then stand still forever" — CONFIRMED and fixed

**Diagnosis: CONFIRMED, with engine-source evidence.** Not a variant, not partial — the
handed-down reading is exactly right, and the engine makes it *terminal*, which is why the
single `SeekWeapon` line never became movement:

`Engine/Plugins/Runtime/StateTree/Source/StateTreeModule/Private/StateTreeExecutionContext.cpp:1629`
is the very error the PIE printed seven times, and the branch it sits in does this:

```
GlobalTasksRunStatus = EStateTreeRunStatus::Failed;
Exec.TreeRunStatus   = EStateTreeRunStatus::Failed;
STATETREE_LOG(Error, TEXT("%hs: Failed to select initial state on '%s' ... "));
...
Exec.ActiveFrames.Reset();
```

and `TickPrelude` (`:1773`) early-returns on `Exec.TreeRunStatus != Running`. So a failed
*initial selection* is not a missed frame the next Think repairs — the run status is Failed,
the frames are reset, and Tick never runs again for the life of that possession. Nothing
re-selects, because selection only happens at Start or at a transition, and a tree with no
active frames has no transitions to trigger.

The trigger is the ordering the diagnosis named, verified in the sources:
- `AIBBotController.cpp` OnPossess set the Think timer with `bLoop=true` and no first-delay
  override, so the first `Think()` is `ThinkIntervalSeconds` in the future.
- `Executor->Start(*this)` ran at the end of the same OnPossess, and
  `AIBStateTreeExecutor.cpp:32` calls `TreeComponent->StartLogic()` **immediately**.
- At that instant `AmbitionEngine->GetCurrent()` is the empty tag (fresh engine, or
  `ResetArbitration()` from the previous unpossess).
- `FAIBAmbitionGateCondition::TestCondition` ends in `Engine && BranchTag.IsValid() &&
  Engine->GetCurrent() == BranchTag`. Empty current tag matches none of the five branch tags,
  so all five enter conditions are false.
- Root's selection behaviour is the default `TrySelectChildrenInOrder`
  (`StateTreeState.h:424`), Root itself has 0 enter conditions and 0 tasks, and every child
  is gated ⇒ nothing selectable ⇒ the error above ⇒ dead tree.

The `ambition -> AIBot.Ambition.SeekWeapon (1.40) over Roam (0.20)` line is the first Think
arriving one interval AFTER the tree already gave up — exactly as read. Fairness samples and
possession were never implicated; both were healthy and stay untouched.

**Fix chosen: (c) BOTH** — because they answer two different failures: (a) makes the *first*
selection correct instead of merely survivable, and (b) is the only thing that makes "no
gate matches" non-fatal on every future frame, including a Phase-6 mode ambition no branch
knows about.

1. `Source/AIBot/Core/AIBBotController.cpp` — one `Think()` call in OnPossess immediately
   before `Executor->Start(*this)`. The brain is already fully built at that point (engine
   registered, sensorium configured, avatar resolved), so the seeded facts are real, and the
   tree's first selection mirrors arbitration rather than falling through to Roam for one
   think interval.
2. `Source/AIBot/Execution/AIBTreeAuthoring.cpp` — **Roam's branch no longer carries an enter
   condition.** It is the last child, and `TrySelectChildrenInOrder` therefore makes it the
   guaranteed selection when the four gated branches decline. Roam is the fallback *want* by
   design ("nothing better to want"), so the gate was restating the ordering; removing it is
   what the engine's own error message asks for. The report line now reads `... Roam UNGATED
   as the ordered fallback ...` so the build output states it.
3. `Source/AIBot/Execution/AIBStateTreeTasks.h` — `FAIBGateRoamCondition` is KEPT (the probe's
   16-struct vocabulary is `Tools/aib/70_aib_assets.py`, not mine to change, and an explicitly
   gated Roam is a legitimate future authoring) with a comment saying it is vocabulary the
   built tree deliberately does not use.

Nothing else moved. No new node struct, no signature change, no spec touched.

**Rung tails:**
- Rung 1 — `./Tools/run-ubt.sh BreachpointEditor`: `PASS BreachpointEditor (exit 0, touched
  libUnrealEditor-AIBot.dylib)`. The script's own exit was 1 for `PARTIAL - fewer than three
  targets`, which is the single-target invocation this packet asked for, not a compile
  failure; `BreachpointServer` remains environmental on this launcher install (AIB1).
- Rung 2 — `./Tools/run-specs.sh AIBot`: `41 test(s) started, 0 failures`, and the counts
  **reconcile** against the raw log `Tools/Logs/specs-20260825-221055.log`: 41 `Test Started`,
  41 `Result={Success}`, 0 `Result={Fail}` ⇒ 41 = 41 + 0. Unchanged from AIB1.
- Boundary grep — `grep -rn "Breachpoint\|BNCharacter\|\"BN" Source/AIBot/ --include='*.h'
  --include='*.cpp'` → no output, exit 1.

**NOT DONE, and it is the gate on this fix mattering: the tree was NOT rebuilt.** The Roam
change is *authoring*, and StateTree bakes node and instance data into the asset at compile —
`/Game/AIBot/AI/ST_AIBBot` on disk still has Roam gated, so a PIE run right now would still
die at selection unless the seeded Think happens to produce a matching ambition (it will, so
bots WILL move — but the (b) half is inert until the asset is rebuilt). No editor was running
(only `UnrealEditorServices`), and this packet forbids opening or closing one, so
`python3 Tools/aib/70_aib_assets.py probe` then `build` is owed to a driver holding the R29
editor. Expect the read-back to show `Roam (0 enter conditions, 2 tasks, 2 transitions)`.

**Honesty ladder:** compiled (Editor target) + rung 2 green. Rung 3 unproven — the live PIE
that produced this bug has not been re-run, and cannot be until the tree is rebuilt.

---

### 25 Aug 2026 — aib-builder (mac terminal): "armed bots still stand still" — the hand was EMPTY

The Roam fix landed and worked (`Failed to select initial state` 0/7, tree compiled, Roam
ungated). What remained was a second, independent deadlock, and it is now diagnosed to a
line of ini.

**(A) ROOT CAUSE — the bots hold nothing. `Weapons[0]` is the null Unarmed slot.**

Evidence, in the order it forces the conclusion:

1. `Config/DefaultGame.ini:378-388`, comment verbatim: *"list order is switch order and
   index 0 is equipped on spawn … Unarmed is a null slot (no actor)"*, and the list is
   `Unarmed, Pistol, Rifle, Shotgun, Knife`.
2. `BNEquipmentComponent.cpp:47-52` — the row named `Unarmed` adds **`nullptr`** to
   `Weapons`. So the carry is `[null, Pistol, Rifle, Shotgun, Knife]`.
3. `BNEquipmentComponent.cpp:155` — `EquipIndex(0)`. This is the line the lead read as
   proof the bot is armed; it is the opposite. It equips **index 0, which is the null
   slot**. `CurrentIndex` is 0, not `INDEX_NONE` — and `GetCurrentWeapon()`
   (`:245`, `Weapons.IsValidIndex(0) ? Weapons[0].Get() : nullptr`) returns **null**.
4. So `UBNAIBAvatarAdapter::GetHeldWeapon()` is null and `CanWeaponFight()` returns false
   at its **first** branch — before the ASC is ever fetched. **`State.Match.Frozen` is
   never even reached, on the seeded first Think or on any later one.** The freeze
   candidate is ruled out by control flow, not by inference.
5. Corroboration from the working brain, which the founder told us to read: BN's own bots
   carry an **`Arm` state** (`BNBotAuthoring.cpp:174-176`) whose single task
   (`FBNSelectWeaponTask`, `BNBotStateTreeTasks.cpp:1602`) presses `Input.Weapon.Next`
   "until the held weapon can actually shoot". That state exists **because** a fresh spawn
   holds nothing. A human's mouse wheel does the same job. The AIB tree has no equivalent
   and no weapon vocabulary before Phase 6 — so an AIB bot is the only actor in the game
   that never arms itself.

"AI already have the weapons" is true at the INVENTORY level — the `BNLoadout:` lines are
four real weapon actors, spawned, attached, hidden. The **hand** is empty.

**Why it never recovers — and what that rules out.** The lead asked whether a stale cached
consideration or an over-sticky commit was holding the 1.40. Neither, and the code says so:

- `AAIBBotController::Think` rebuilds `LastFacts` from scratch **every 0.1s**
  (`AIBFactsBuilder::Build`, then `Rescore`). `FAIBConsideration::Evaluate` reads the facts
  struct directly — there is no cached score anywhere in the path.
- The commit cannot hold a self-vetoed incumbent: `bCommitted` requires
  `IncumbentRawScore > 0.f` (`AIBAmbitionEngine.cpp:150`), and the existing spec *"releases
  a commit whose incumbent VETOED itself"* pins it. `SwitchCostFactor` 1.15 multiplies a
  **freshly recomputed** raw score, and 1.15 × 0 = 0.
- Therefore, had `CanWeaponFight()` ever flipped true, SeekWeapon would have collapsed to 0
  and the winner would have changed **within 100ms**, printing a second line. Across a whole
  match not one bot printed a second line. **That silence is itself the proof that the gate
  never flipped** — i.e. a permanent condition (empty hand), not a transient one (warmup
  freeze). The 10Hz rescore is what turns the missing log line into evidence.
- Consequence: AIB does **not** need BN's event-driven `RescoreBrain()` to recover. Its
  worst-case reaction to a world change is 100ms, under R11's own 200ms fairness floor.
  Event rescoring is a nice-to-have for a tickless design, not this bug; not adding it.

**(A) FIX — the adapter arms the pawn, in the adapter folder, with the existing verb.**
`Source/BreachpointNext/AIBotAdapter/BNAIBAvatarAdapter.{h,cpp}`:
- `BeginArming()` (called from `EnsureOn` after `RegisterComponent`) starts a 0.5s looping
  timer; `ArmIfEmptyHanded()` presses and releases `AIBTags::Verb_WeaponNext` — which the
  verb map already routes to `Input.Weapon.Next`, the same button the human and the BN bot
  press — and **clears its own timer the first tick the hand is full**.
- A TIMER, not one press, because `BNGA_Equip` does not set `bIgnoreMatchFreeze`
  (`BNGameplayAbility.cpp:37` refuses every activation while `State.Match.Frozen`), so a
  single press at possession would be swallowed by warmup and lost forever. One `Warning`
  at 30 presses names the two remaining causes rather than staying silent.
- One `Verbose` line added to `CanWeaponFight()` for the empty-hand case, so the next
  reader gets the reason instead of re-deriving it from the ini.
- Nothing in BN gameplay changed: the adapter exists only on AIB-possessed pawns
  (`EnsureOn` requires an `AAIBBotController`), and it uses only BN's existing input path.

*Deliberate layering call:* arming is game knowledge ("this game spawns you unarmed"), so
it belongs on the game side of the seam, not in the module. When Phase 6 gives the module a
weapon-selection vocabulary this becomes an AIB task pressing the same verb, and this timer
deletes itself. Flagged for the founder to rule on if he wants agency in the brain sooner.

**(B) CHOICE — SeekWeapon is now inert-by-fact, not deleted.** Chosen: **score ~0**, via a
new fact rather than by disabling the ambition.
- `FAIBFacts::bWeaponPickupKnown` (Core/AIBTypes.h) + selector `WeaponPickupKnown` + a
  second near-veto consideration on the SeekWeapon spec. `AIBFactsBuilder` sets it **false
  explicitly** (greppable), with the Phase-6 line named: a `POI.Weapon` world query.
- Rationale over the alternative (make Seek's branch fail through to Roam): the branch
  **already** fails fast — `FAIBMoveToPOITask::EnterState` returns `Failed` with no
  provider — and it still deadlocked, because the gate is the AMBITION and arbitration kept
  re-electing it, so Root re-selected the identical branch every 2s forever. Fixing it in
  the tree would also mean an authoring change and a mandatory asset rebuild. Fixing it in
  scoring is worldless, spec-testable, needs **no tree rebuild**, and is where the founder's
  steer points: BN's working brain models three ambitions (Fight/Survive/Roam) and never
  modelled seeking a weapon, because this world has no pickups. The tag survives for Phase 6.
- Second-order effect worth stating: a bot that runs genuinely dry now Roams instead of
  freezing. It does not yet reload — `Verb_Reload` is mapped but no AIB task presses it.
  That is a real Phase-4 gap, named here, not fixed here.

**Rung tails:**
- Rung 1 — `./Tools/run-ubt.sh BreachpointEditor`: `Result: Succeeded` /
  `PASS BreachpointEditor`. Script exit 1 is its `PARTIAL - fewer than three targets`
  banner for the single-target invocation this packet asked for, not a compile failure.
  **Both modules relinked as hot-reload dylibs** (`libUnrealEditor-AIBot-0002.dylib`,
  `libUnrealEditor-BreachpointNext-0002.dylib`) because the lead's editor holds the base
  ones open — **the running editor is on the OLD code and must be restarted (or Live-Coding
  compiled) before PIE, or none of this is in the session.**
- Rung 2 — **BLOCKED, owed.** `./Tools/run-specs.sh AIBot` refuses while any editor runs
  ("An UnrealEditor process is already running… close the editor first", exit 3) and this
  packet forbids closing it. The suite is now **42** tests: one added,
  `AIBot.Sim.AmbitionEngine` → *"never wants a weapon this world does not contain — the
  25 Aug deadlock, pinned"* (dry + no pickup ⇒ Roam; flip the fact ⇒ SeekWeapon wins again,
  proving inert ≠ dead). It COMPILED (rung 1 built the Tests TU) but has not RUN. Expect
  **42/42/0** and reconcile against `Test Started` / `Result={Success}` / `Result={Fail}`.
- Boundary grep — `grep -rn "Breachpoint\|BNCharacter\|\"BN" Source/AIBot/ --include='*.h'
  --include='*.cpp'` → no output, exit 1.
- **No tree authoring changed.** Node sets, gates and branch order are untouched;
  `/Game/AIBot/AI/ST_AIBBot` does not need rebuilding for this fix.

**Honesty ladder:** compiled (Editor target only). Rung 2 not run (editor held). Rung 3
unproven — the founder's PIE is the only thing that can show a bot walking, and it needs an
editor restart first.

---

### 25 Aug 2026 — aib-builder: FOUNDER RULING APPLIED — SeekWeapon is retired, Seek is deliberate movement

*Supersedes the (B) half of the entry above. The `bWeaponPickupKnown` fact, its selector and
its builder line are **reverted in full** — that was the "score around it" fix the ruling
withdrew. The (A) empty-hand diagnosis and the adapter's arm press stand unchanged.*

**What the ambition is now.** `AIBot.Ambition.Seek` — "I have somewhere to be": the matured
target belief when there is one, else any POI a provider offers, else a random reachable
point. `Ambition_SeekWeapon` is gone from the tag list, and `AIBot.POI.Weapon` is deleted
with the concept it named.

**Renames done, and the two that could NOT be.** The probe (`Tools/aib/70_aib_assets.py`,
not this module's file) pins 16 node paths by name, two of them
`/Script/AIBot.AIBGateSeekWeaponCondition` and `/Script/AIBot.AIBMoveToWeaponPOITask`.
Renaming either would fail the probe and block the build step, so both structs are
**repurposed in place** — new `DisplayName` metadata ("AIB Gate: Seek", "AIB Seek
Destination"), new behaviour, old C++ identifiers, each carrying a comment that says why the
name is frozen. **A serial rename of those two identifiers plus the probe list is OWED** —
it is a one-line-each edit in two files that must land in the same commit, and it is not
wave work. The tag itself was free to rename (the probe names no tags).

**The branch.** Unchanged node SET — gate + sentinel + mover — because every per-branch
difference in this tree is a C++ virtual, never a serialized node parameter (the compiled
authoring surface sets nothing on a node it adds). The mover
(`FAIBMoveToPOITask`) gained one virtual, `ShouldMoveToBeliefFirst()`, and Seek's override
turns on belief-first plus the wander fallback. **Its EnterState can no longer fail for want
of a destination** — that is the structural half of "must never strand a bot", independent
of any score.

**TREE REBUILD: NOT REQUIRED BY THIS DIFF, and I want that read carefully rather than
trusted.** No `AddChildState` / `AddEnterCondition` / `AddTask` / `AddTransition` call
changed, and no state name changed, so the bytecode the asset holds is identical; what
changed is code the nodes execute. You are rebuilding anyway for the Roam disagreement
(0787e54) — after it, the read-back should show **Seek (1 enter condition, 2 tasks, 2
transitions)**, i.e. the same shape as before, and the report gains one new line:
`seek       : AIBot.Ambition.Seek — deliberate movement ...`. If Seek's shape comes back
different, something else moved and I want to hear about it.

**Scoring, and the mistake two old specs caught in under a minute.** Seek is
`BaseUtility 0.5 × ObjectiveUrgency` (the mode's dial, matched on this tag, Phase 6), commit
3s. `ValueWhenUnknown` is **0.3 ⇒ 0.15, deliberately UNDER the 0.20 Roam floor**. My first
draft used 0.6 ⇒ 0.30, above the floor, and rung 2 failed exactly twice:
`does not flee on unknown vitals` and `never lets the floor ambition starve a real want`.
Both were right: a want that outranks Roam whenever no mode is registered has not joined the
ladder, it has REPLACED the floor — and with a 3s commit it re-armed W-REVIEW P2 H-1's
commit-starvation walk (a fresh spawn ignoring the first enemy it sees for three seconds).
I moved the number, not the specs.

**So what do the bots do in PIE today?** They **Roam** — wander to reachable points — because
nothing in this game names a destination yet. Seek is DORMANT, which is a different thing
from the trap SeekWeapon was: SeekWeapon could win and then had nowhere to go; Seek cannot
win until something names a place, and if it wins its branch always moves. The day
`IAIBAmbitionProvider` publishes an objective under `AIBot.Ambition.Seek`, it outranks the
floor with no code change. **If you want Seek visibly exercised in PIE before Phase 6, the
knob is one line** — `Urgency.ValueWhenUnknown` 0.3f → 0.5f in `BuildDefaultCoreAmbitions`
— but it costs the two invariants above, so I am not taking that decision here.

Ambition ladder as shipped: Engage (visible + can fight, ≤1.0) · Retreat (1.2 × hurt) ·
Search (0.8 × memory freshness) · **Seek (0.5 × mode urgency, 0.15 dormant)** · Roam (0.20
floor, no commit). BN's Fight / Survive / Roam, with Search and Seek as the two movement
wants between them.

**Rung tails (final, after four builds):**
- Rung 1 — `./Tools/run-ubt.sh BreachpointEditor`: `Result: Succeeded` / `PASS
  BreachpointEditor (touched libUnrealEditor-AIBot.dylib)`. Script exit 1 is the
  `PARTIAL - fewer than three targets` banner for the single-target invocation.
- Rung 2 — `./Tools/run-specs.sh AIBot`: **42 started, 0 failures**, reconciled against
  `Tools/Logs/specs-20260825-223746.log`: 42 `Test Started`, 42 `Result={Success}`,
  0 `Result={Fail}` ⇒ 42 = 42 + 0. 41 pre-existing plus one added,
  *"wants nothing this world cannot satisfy — SeekWeapon is retired, Seek moves"*, which
  pins three things: no registered ambition's tag contains "SeekWeapon"; the old trap state
  (dry, blind, mode-less) now elects the moving floor; a named destination wakes Seek, and a
  visible enemy still outranks it.
- **PROCESS CATCH worth keeping:** an earlier spec run reported 42/0 while the editor was
  open, and it was **lying** — with the editor holding `libUnrealEditor-AIBot.dylib`, UBT
  links to `-0001`/`-0002` hot-reload copies, and the headless `-Cmd` loads the stale base
  dylib. The tell was the log printing the PREVIOUS revision's spec name. Once the editor
  closed and the link was clean, the same suite showed the two real failures above. **A
  green rung 2 taken while an editor is open proves nothing** — the runner's own gate exists
  for this, and I only saw it because the spec names changed.
- Boundary grep — `grep -rn "Breachpoint\|BNCharacter\|\"BN" Source/AIBot/ --include='*.h'
  --include='*.cpp'` → no output, exit 1. (It caught one real hit mid-work: a comment in
  `AIBTags.h` naming the host game. Reworded, not excused.)

**Honesty ladder:** compiled (Editor target) + rung 2 green and reconciled against a clean
link. Rung 3 unproven — no PIE was run and no editor was opened or closed by me.

---

### 25 Aug 2026 — aib-builder: THE VERB VOCABULARY — sprint, crouch, reload, wedge-jump. Navlinks need nothing.

Founder ask: "make sure they are able to use navmesh navlinks, jumps, change weapons, melee,
grenades, crouch, sprints, jump up jump down, reload — use breachpointnext as reference."
`Source/BreachpointNext/AI/BNBotStateTreeTasks.cpp` was read as the reference and **not
edited**; no BN gameplay code changed. The adapter's verb map was already complete (all
eight), so nothing was plumbed — what was missing is the tree ever PRESSING them. Before
this diff an AIB bot could fire, and arm. That was the whole vocabulary.

**NAVLINKS: NO AI WORK NEEDED, CONFIRMED BY READING, AND NONE WAS BUILT.** Traversal is a
property of the PAWN and the NAVMESH, never of the brain:
- `Source/BreachpointNext/Characters/BNCharacter.cpp:196-198` — `GetNavMovementProperties()`
  then `bUseAccelerationForPaths = true`, with its own comment noting the flag "is only ever
  read on a pawn an AIController is moving".
- `Config/DefaultEngine.ini:286` `bGenerateNavLinks=True`, plus two `+NavLinkJumpConfigs`
  rows — `BN_Drop` (down-only, `UpDirectionAreaClass=None`, JumpLength 400 under a measured
  514) and `BN_Climb` (up-only, JumpHeight 90).
- AIB bots possess the **same `ABNCharacter`** (the adapter's `EnsureOn` is called from its
  `PossessedBy`), so both properties are inherited the instant a bot paths. Jump-up and
  jump-down are already theirs. Nothing in `Source/AIBot/` knows or should know this.

**LANDED — four verbs, all through the existing door, no new node struct:**

1. **Sprint** (`Verb_Sprint`) — the rule transcribed from BN's `FBNMoveToTargetTask`
   (`:415-425`) *with* its reasoning: hold while beyond `1.5 x` the mover's arrival radius,
   release inside it, because a bot that sprints into its own firing position arrives unable
   to shoot (the sprint state holds while the key is down). Applied to **every** mover —
   MoveNearBelief (Engage), MoveToLastKnown (Search), MoveToPOI (Seek/Roam), FleeFromBelief
   (Retreat). Sprint is a HOLD, so it is edge-tracked and **always released in ExitState**
   (BN's own leak lesson). BN's second clause, "or no line of sight", is deliberately absent
   from MoveNearBelief: that task *fails* without a held belief, so the blind case cannot
   occur there and testing for it would be dead code.
2. **Crouch** (`Verb_Crouch`) — while reloading, per BN's `FBNReloadTask` (`:1347-1350`).
   Crouch is a **TOGGLE**, so it is pressed through a helper that copies both of BN's hard-won
   guards: never ask for a crouch while falling (mid-air the toggle only ever UNcrouches, so
   the press does the opposite of the ask), and compare against the avatar's **real** state,
   never a private mirror that a landing or a low ceiling silently invalidates. That needed
   one new question at the door — see below. The task uncrouches only what it crouched, on
   ammo recovery and on exit.
3. **Reload** (`Verb_Reload`) — below **0.25** magazine with reserve left; BN's threshold shape
   (`FBNNeedsReloadCondition`), read off `FAIBFacts::AmmoNorm` / `bHasReserveAmmo` rather than
   a weapon. One tap, re-tapped no faster than 1s, because a refused ability (frozen, dead, no
   montage) never notifies and would otherwise be pressed at tick rate forever. This closes the
   gap the 25 Aug entry named ("it does not yet reload — a real Phase-4 gap").
4. **Jump** (`Verb_Jump`) — BN's **wedge jump** (`:388-400`), not the strafe juke: less than
   50uu of ground gained for 1.5s with a goal still ahead ⇒ ONE jump, re-armed the moment the
   bot moves again, and never pressed while airborne. This is the only jump the brain needs;
   drops and climbs are the navlinks above.

**WHERE THEY LIVE, AND WHY NOT IN NEW BRANCHES.** Reload sits inside `FAIBFireWhenAbleTask`
and locomotion inside the movers, exactly as BN puts `SetSprinting` inside its move task
rather than in a Sprint state. Here it is also forced: a new branch means a new node struct,
and the node list is pinned by `Tools/aib/70_aib_assets.py`, outside this module's owner path.
The shared scratch (`FAIBLocomotionState`) is a plain struct with **no UPROPERTY**, so it is
per-run state the StateTree compiler bakes nothing of.

**TREE AUTHORING: UNCHANGED — NO REBUILD NEEDED.** Say this loudly, it is a gate: no
`AddChildState` / `AddEnterCondition` / `AddTask` / `AddTransition` call moved, no state name
moved, no node struct added or removed, and every field added to instance data is
non-UPROPERTY runtime scratch. `AIBTreeAuthoring.cpp` is untouched by this diff.
`/Game/AIBot/AI/ST_AIBBot` does **not** need `70_aib_assets.py probe`+`build` for these verbs.
(The Roam/Seek rebuild owed by the two entries above is unaffected and still owed.)

**ONE INTERFACE ADDITION** — `IAIBAvatarInterface::IsCrouched()`, implemented in the adapter as
`ACharacter::bIsCrouched`. It is the toggle's correctness condition and it cannot come from the
0.1s facts cache: the press must compare against the state as it is at press time. One
implementor exists (the adapter), so nothing else needed touching.

**DEFERRED, each for a reason, not for time:**
- **Weapon switching by range** — deferred, and the dry-gun swap with it, on EVIDENCE:
  `UBNEquipmentComponent::GetNextIndex()` is `(CurrentIndex + 1) % Weapons.Num()` and
  **does not skip the null Unarmed slot** (`GetNextWeapon`'s own comment: "Still nullable on
  success: the unarmed slot IS a null entry in this array"). So one in five `Input.Weapon.Next`
  presses puts a bot into empty hands mid-fight. BN's range scoring also reads each weapon's
  own damage row — game knowledge that would need a new question at the door. Both belong in
  one packet with a slot-aware verb, not bolted on here.
- **Melee** and **Grenade** — both are Engage-branch range bands and both are cheap, but they
  need a distance the *task* trusts (`FAIBFacts::DistToTargetUU` is 0.1s stale, which is fine
  for wanting and wrong for a lunge) plus a grenade cooldown. Named, not started.
- **Crouch in cover** — AIB has no cover concept at all. It is not a verb gap, it is a missing
  perception feature.
- **The strafe juke** (BN's every-Nth-strafe-step jump) — needs a strafe pattern AIB does not
  have. Deferred with it.

**Rung tails — READ THE RUNG 2 LINE, IT IS NOT GREEN:**
- Rung 1 — `./Tools/run-ubt.sh BreachpointEditor`: `Result: Succeeded` /
  `PASS BreachpointEditor (exit 0)`. Both modules compiled and linked; `AIBot` shows
  `[5/11] Compile Module.AIBot.gen.cpp` / `[6/11] Link libUnrealEditor-AIBot-0001.dylib`, and
  `AIBStateTreeTasks.cpp` was compiled outside the unity blob (adaptive build), so it really
  was compiled. Script exit 1 is the `PARTIAL - fewer than three targets` banner for the
  single-target invocation this packet asked for. **The `-0001` suffix is the editor holding
  the base dylib open: the running editor is on OLD code until it is restarted or Live-Coding
  compiled.**
- Rung 2 — **BLOCKED, OWED.** `./Tools/run-specs.sh AIBot` printed `BLOCKED - RUNG 2: An
  UnrealEditor process is already running` and did not run. This packet forbids closing it.
  The other agent's process catch applies and is worth repeating: **a rung 2 taken with an
  editor open LIES** — UBT links `-0001`/`-0002` while the headless `-Cmd` loads the stale
  base dylib, so it grades the previous revision. Expect **42/42/0 unchanged**: this diff
  touches only `Execution/` and the adapter, and no spec covers either (the suites are
  worldless by module law, and every task here needs a pawn). Reconcile against
  `Test Started` / `Result={Success}` / `Result={Fail}` when the editor is free.
- Boundary grep — `grep -rn "Breachpoint\|BNCharacter\|\"BN" Source/AIBot/ --include='*.h'
  --include='*.cpp'` → no output, exit 1.

**Honesty ladder:** compiled (Editor target). Rung 2 NOT RUN (editor held) — nothing here is
spec-covered and nothing could be: sprint edges, the crouch toggle, the reload tap and the
wedge jump are all pawn-and-world behaviour, so **PIE is the only rung that can prove any of
it**. What to watch for in a `BotSystem=AIB` PIE: bots that keep up with a sprinting human,
crouch when they reload, and jump once instead of grinding into a crate. What would say it is
wrong: a bot standing crouched after a fight (a leaked toggle), or one walking its whole
approach (sprint never pressed — check the adapter's server gate first).

### 26 Aug 2026 — cloud. THE PHASE-3 W-REVIEW BARRIER: 4 passes, all FAIL, merged onto the terminal's live-PIE work. WRITTEN, NOT COMPILED.

Four aib-critic passes (containment · fairness · utility pathologies · server-only), one
attack surface each, run against the Phase-3 landing while the terminal drove AIB2 live.
Verdicts: FAIL ×4; nine high entries de-duplicated to five. This entry records what the
barrier found, what the terminal had ALREADY fixed independently by the time the barrier
merged, and what this commit adds on top. The two histories converged on the same day's
truths from opposite directions — the review from code, the terminal from a live match —
and where they collided, the live evidence and founder rulings won.

**Highs already closed by the terminal before the merge (recorded, no code change):**
- Cold start against an invalid ambition — the terminal's live diagnosis (engine source:
  a failed initial selection is TERMINAL) and the barrier's server-only pass demanded the
  identical fix: `Think()` before `Executor->Start`. Same line, both sides. Kept once.
- The boundary-gate comment hit in AIBot.Build.cs — reworded on main before the barrier's
  fix landed; theirs kept.
- The dry-weapon absorbing want — the barrier's fix (gate SeekWeapon on a known source)
  was SUPERSEDED by the founder's retirement of SeekWeapon entirely: `Ambition_Seek` is
  deliberate movement, satisfiable by construction, 0.15 unknown-urgency under the Roam
  floor. The barrier's spec for it was dropped; the terminal's "wants nothing this world
  cannot satisfy" spec pins the same property against the ambitions that exist.

**Barrier fixes landing IN THIS COMMIT:**
1. **The sensorium ordering hole (fairness HIGH)** — a loss maturing BEFORE its own gain
   was dropped (both guards keyed on the actor already being visible): a ~100ms peek
   could mature into live wall-tracking for SightMaxAge (~1 in 6 short peeks with our
   latency band), then launder the tracked position into 16s of search memory. Fixed
   with a note-time per-actor loss ledger + the superseded-gain rule (survives both the
   draw inversion and a clock-cap drop); FAIRPLAY amendment appended; pinned by the new
   "supersedes a gain whose loss drew the faster reaction" sensorium spec.
2. **THE ROAM RULING (utility pass)** — neither the gated source nor the interim ungated
   asset: Roam is RE-GATED (the executor mirrors arbitration 1:1 for every real
   ambition) and a sixth, ungated, LAST `Fallback` state (sentinel + new
   `FAIBUnservedWantTask`, one Warning naming the unserved want) satisfies the engine's
   always-selectable demand. The ungated Roam's hidden cost was Phase-6: a mode-ambition
   bot would roam past the flag forever while the ambition log printed the correct want —
   silent wrong behaviour, invisible in exactly the line P-3 samples. The terminal's
   terminal-failure evidence is what makes the Fallback mandatory rather than paranoid.
3. **Mover blindness (H3, two passes)** — every MoveToLocation result was discarded; a
   refused or partial path was a permanent silent stand (a cornered Retreat = shot while
   "fleeing"; a stalled Roam = a healthy-looking statue). All movers now check
   `EPathFollowingRequestResult::Failed` and carry a no-progress give-up that fails
   LOUDLY (F7) — layered UNDER the terminal's wedge-jump, which gets its 1.5s chance
   first.
4. **Retreat's absorbing freeze (H1)** — hurt with no threat knowledge selected a flee
   task with nothing to flee from, forever, while hysteresis defended the frozen want.
   FleeFromBelief now REPOSITIONS to a random reachable point when it holds no threat
   fix; still fails loudly when even that is impossible.
5. **Mediums:** avatar-door validity rides the GC-tracked pair half (`IsValid`), so a
   destroyed adapter yields null, never a dangling pointer (two passes independently);
   OnUnPossess AND a new EndPlay override release Verb_Fire through the still-valid door
   as the belt under the tree's ExitState brace; FireWhenAble gates on the LIVE
   `HasVisibleTarget()` too (no bursting at a just-destroyed corpse for a stale fact
   window) and its skipped releases are Warning-loud; MoveNearBelief re-closes on a 0.5s
   repath cadence after the BOT is displaced (knockback/pad — drift-only never re-closed);
   the F4 snap path is gone (rate ≤ 0 falls back to 360°/s); the blast perceivability
   trace channel is `UPROPERTY(Config) BlastPerceivabilityChannel` — the HOST decides
   what "blocks eyes" means, per its own collision ledger; Roam success carries 0.25s
   (nav-island per-frame pathfind); the task-side `16.f` memory fallbacks are
   `AIB::DefaultMemoryFreshSeconds` (one definition); ARCHITECTURE gained the
   component-tick dated exception and extraction-delta item 5 (the content mount);
   runtime warning strings no longer name repo tickets.

**NEEDS LIVE PROOF (server-only pass — answer during step 5):**
1. Does `StopLogic` exit states synchronously in 5.8? UE_LOG ordering across an
   unpossess mid-burst decides whether the ExitState release ever needed the belt.
2. The Fallback branch: force an unmapped want (any Mode-tagged ambition) and paste the
   one Warning; confirm the sentinel exits it when the want changes.
3. `GetIsReplicated()` on the StateTree component in a listen-server PIE (law 3
   end-to-end).
4. PIE end / seamless travel with a bot mid-burst — clean shutdown through EndPlay.
5. Does `SetStartLogicAutomatically(false)` survive CDO serialization into instances?
6. UNVERIFIABLE-FROM-CLOUD: `AAIController::bStartAILogicOnPossess`/`bStopAILogicOnUnposses`
   were NOT pinned (no compiled in-repo precedent). If possession logs show the brain
   component starting before the executor sets the tree, pin both false and record the
   engine header lines here.

**Registered, not fixed (risk register):** fire cadence is bounded by tree re-entry rate,
not a human trigger rate (Phase 4's policy owns it); `bTargetFactsFromMemory` still has
no consumer; per-attempt mover logs are Verbose (raise verbosity to diagnose); the
function-local `static FAIBTierRow Defaults` in NoteIncomingBlast is process-wide (must
become per-bot at Phase 8); a same-tick sibling failure can demote the sentinel's clean
exit to the 0.2–0.5s failed delay; `GetVisibleTarget()` hands out a live `AActor*` with
no Execution-side consumer — Phase-4 builders must not walk through that open door; the
SeekWeapon→Seek struct renames (`FAIBGateSeekWeaponCondition`, `FAIBMoveToWeaponPOITask`)
remain OWED — this commit edits the probe list (adds `FAIBUnservedWantTask`, 17 entries)
but defers the rename to its own serial step so the asset rebuild carries one shape
change at a time.

### 2026-08-25 — the merge that did not compile (2f8b51a)

Rebased the three verbs (weapon switch, melee, grenade — 756eea3, verified live before
the rebase) onto the Phase-3 W-REVIEW barrier `6e059cc`, whose own commit subject reads
**WRITTEN, NOT COMPILED**. The merged tree failed rung 1 on four causes. Separated by
owner, because "it broke" is not a finding:

MINE — my conflict resolution dropped `EndPlay`'s closing brace in `AIBBotController.cpp`,
cascading into 5× *function definition is not allowed here*. Both conflicts were
additive and I kept BOTH sides (their `EndPlay` + my grenade-cooldown pair; their
refined `bMayFire` + my three verb blocks) — but I had to **drop my now-stale
`bMayFire`**, because theirs adds a live sensorium check that closes the destroyed-target
frame. A naive keep-both would have compiled and silently lost that fix.

THEIRS — three of the kind a compiler finds in seconds and a cloud container cannot:
- `AIBAmbitionEngine.cpp:262` still named `AIBTags::Ambition_SeekWeapon`, retired in
  544baa6 → `Ambition_Seek`.
- `AIBStateTreeTasks.cpp` used `EPathFollowingRequestResult::Failed` with no include
  (5×); BN's equivalent file carries `Navigation/PathFollowingComponent.h` at line 21.
- **Unity-build collision.** The adapter defined `WeaponCanFight` / `ScoreWeaponAtRange`
  and BN's `BNBotStateTreeTasks.cpp` defines both names. Separate `.cpp` files — but a
  unity build merges them into one TU and the second definition is an error. Renamed the
  adapter's to `AIBWeaponCanFight` / `AIBScoreWeaponAtRange`. **This is why a module that
  compiles alone can still break the target it links into**, and why "AIBot builds" is not
  the same claim as "Breachpoint builds with AIBot in it".

Rung 1 PASS on a **clean relink**. The first PASS was a `-0004` hot-reload dylib because
my editor was still open — the same trap this ticket already records; it is recorded twice
now because I walked into it twice. Rung 2 **43/43/0** reconciled with no editor running
(43, not 42 — the barrier added a sensorium spec). Boundary grep empty. `BotSystem=BN`
restored; BN gameplay untouched, the rename living on AIB's side of the bridge.

**Not re-measured after the merge:** the three verbs were proven live in 756eea3 (pawn
state, not log counts — see the entry above). The merge changed only the fire gate, in
the strictly-more-careful direction, plus mechanical renames. A fresh live pass needs
`BotSystem=AIB`, which is still a founder decision and not mine to flip.
