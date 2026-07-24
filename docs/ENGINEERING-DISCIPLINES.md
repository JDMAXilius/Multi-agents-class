# Slash Roller — Engineering Disciplines & Responsibilities
## Architecture Decomposition (pre-crew)

**Project:** Slash Roller (codename OnSight) — UE 5.8, pure native C++, GAS
**Purpose:** define the engineering disciplines as **ownership boundaries**
— who owns which systems, what doctrine binds each, and where the seams are
— so that the agent crew (next step) is minted *onto* real boundaries, not
invented ones. This is the map; the crew is the vehicle.

---

## 1. How the boundaries are drawn

A discipline is not "a job title." It is **one owner-path plus one body of
non-obvious doctrine**. The kit's rule for minting a specialist applies to
humans and agents alike — a discipline earns its own boundary only when
**both** are true:

1. **Mistakes there are silent and confident** — the code works in casual
   testing and fails in production (authority bugs, save corruption,
   cook-only breaks, prediction desyncs).
2. **The doctrine is real** — there are non-obvious domain laws a generalist
   will not reliably apply (server authority, GAS prediction keys, MVVM
   lifecycle, motion-matching thread safety).

Everything that fails *both* tests stays generalist. This is why the split
below has seven disciplines, not twenty — and why each maps cleanly to a
future agent.

**The trust model that shapes everything:** clients send *intent*, the
server simulates *truth*, clients render *results*. There is no anti-cheat
middleware behind the netcode discipline — server authority IS the entire
trust boundary. Every discipline's boundary is drawn to protect that line.

---

## 2. The discipline map

| # | Discipline | Owns (the risk it guards) | owner_path (module) | Maps to crew agent |
|---|---|---|---|---|
| D1 | **Gameplay Systems** (sim) | Deterministic combat math — the provably-right core | `Source/SR/Combat`, `Source/SR/Sim` | `sim-builder` |
| D2 | **Multiplayer / Netcode** | Replication, RPCs, authority, prediction — the silent-confident class | `Source/SR/Net`, replicated surface of all modules | `netcode-builder` |
| D3 | **Animation Systems** | Motion Matching, AL Framework, AnimGraph nodes, warping | `Source/SR/Animation` | *new:* `anim-builder` |
| D4 | **Online Services / Backend** | Sessions, lobby, listen-server host, GameLift seam | `Source/SR/Online` | *new:* `services-builder` |
| D5 | **UI / UX Systems** | CommonUI stack, MVVM, GAS-driven HUD | `Source/SR/UI` | `ui-builder` |
| D6 | **Audio Systems** | MetaSounds via GameplayCues, rollback-safe | `Source/SR/Audio` + cue assets | folds into `builder` (thin) |
| D7 | **Tools / Build / LiveOps** | Perforce, CI, Gauntlet, Steam depots, telemetry | `Tools/`, `.github/`, build scripts | `verifier` + `builder` |
| — | **Technical Direction** (you) | Cross-cutting architecture, the final call, seam arbitration | repo-wide (read), contracts (write) | `game-lead` (lead session) |
| — | **Adversarial review** | Breaking any dangerous-domain change before it lands | read-only | `critic` |

---

## 3. The disciplines in detail

### D1 — Gameplay Systems Engineer (the Sim)

- **Mandate:** own the deterministic core that decides damage, stamina,
  cooldowns, kill/scoring, win conditions. This is the part of the game
  that must be **provably right** — it runs headless under Automation Specs
  with zero dependence on rendering, timing, or network.
- **Owned systems (Slash Roller):** the combat triangle resolution
  (light/heavy/block/parry/dodge), stamina economy + winded threshold,
  damage/mitigation formulas, kill attribution rules, match-scoring and
  tiebreak logic.
- **Key types:** GAS `UGameplayAbility` execution logic, `UGameplayEffect`
  ExecutionCalculations, the attribute set (Health/Stamina/Magic), pure
  C++ rule functions called *by* abilities.
- **Doctrine (non-obvious laws):**
  - **Pure functions over actor spaghetti** — a rule that can't run in a
    headless spec is in the wrong place.
  - **Determinism is law** — no wall-clock, no frame delta, no
    `RandRange` inside a rule; randomness enters as a *seeded stream passed
    in* so tests pin exact outcomes and the server can reproduce any
    disputed result. (This is also what makes bots reproducible for QA.)
  - **No netcode in the math** — the sim neither knows nor cares about
    authority; the net layer calls it *on* the authority and replicates
    results.
  - **Numbers come from DataTables** — the rule says *how* damage combines;
    the CSV says *how much*. A gameplay literal in sim code is a violation.
  - **Pin behavior with spec suites** — exact known cases + invariants
    (more armor never increases damage; stamina never goes negative; a kill
    never mints a negative score). Any change that moves a pinned number
    moves it loudly, in the same packet, with the reason logged.
- **Does NOT own:** how state replicates (D2), how it's drawn (D5), when a
  sound plays (D6). Balance *values* are data, owned by design + curated,
  not by this discipline's code.
- **Seam:** exposes pure results; D2 decides authority and replication of
  those results.

### D2 — Multiplayer / Netcode Engineer

- **Mandate:** own the replicated surface of every module and the entire
  server-authority trust boundary. A client-authoritative bug does not
  crash — it ships, passes every playtest, and becomes a dupe/teleport/
  god-mode exploit the week real players arrive.
- **Owned systems (Slash Roller):** PlayerState-owned ASC replication,
  ability activation replication + **GAS prediction keys**, the
  `SoftLockTarget` replicated source of truth, match/round state
  replication (`MatchEndServerTime`, kill/death counts), respawn authority,
  hit-confirmation on the server, host-advantage review (listen-server
  topology).
- **Key types:** `UFUNCTION(Server, Reliable, WithValidation)` + real
  `_Validate` bodies, `DOREPLIFETIME_CONDITION`, `OnRep_` (cosmetic only),
  `FPredictionKey`, `IOSServerLifecycle` (the listen-server → GameLift
  migration seam).
- **Doctrine (full law: `contracts/netcode.md`):**
  - **Authority gate on every mutation** — `HasAuthority()` at the top of
    anything that changes gameplay state; the check IS the documentation.
  - **Server RPCs validate or don't ship** — range/rate/possession checks;
    an empty `return true;` fails review.
  - **Replicate results, not authority** — if removing every `OnRep_` body
    changed a gameplay outcome, the design is wrong.
  - **Minimum replication** — tightest condition that works;
    `COND_OwnerOnly` for private state (stamina *value*, loadout internals);
    dormancy for rare-change actors; bandwidth is a budget.
  - **Prediction reconciles** — a mispredict may never fork gameplay state;
    every predicted path has a correction path.
  - **The attack ships with the feature** — each new replicated surface
    lands with its cheat-attempt test (forged RPC, out-of-range value,
    spam) whose *rejection* is the acceptance criterion. The critic
    re-attacks independently.
  - **Rung-honesty** — single-process PIE proves nothing about replication;
    the floor for "works" is dedicated server + 2 clients under net
    emulation.
- **Does NOT own:** the math it replicates (D1), UI that reads replicated
  state (D5). Consumes D1's pure results; exposes replicated APIs to D5.
- **Seam (the critical one):** D2 is the only discipline that may add a
  replicated property, RPC, or authority decision. Any other discipline
  that needs one files a gap to D2 rather than winging a `Server` RPC.

### D3 — Animation Systems Engineer

- **Mandate:** own the deepest existing technical investment — the
  motion-matched animation stack that makes the melee *feel* land.
- **Owned systems (Slash Roller):** the AL Framework (KLS-derivative, 16
  files), the 7 custom C++ AnimGraph nodes, PoseSearch/Motion Matching
  schema + trajectory, locomotion/traversal/combat linked-anim layers
  (Lyra-style), **motion warping on strikes** (layered on target assist),
  IK, thread-safe anim patterns.
- **Key types:** custom `FAnimNode_*`, AnimInstance (game + worker thread),
  motion-warping targets sourced from `SoftLockTarget`.
- **Doctrine:**
  - **Anim logic on the worker thread** where the pattern allows; no
    game-thread anim hacks.
  - **Warp targets are replicated values, not local guesses** — strikes
    warp toward `SoftLockTarget` (already replicated by D2), so
    server/proxy warp divergence stays bounded.
  - **Montage windows drive gameplay events, gameplay events drive damage**
    — the notify raises the event; D1 computes; D2 confirms on the server.
    Animation *requests*, it never *decides* damage.
  - **Cosmetic prediction is fine, authoritative prediction is not** — a
    predicted swing animation that rolls back must leave no gameplay state.
- **Does NOT own:** the damage the strike deals (D1), whether the hit is
  server-confirmed (D2). It owns *alignment and feel*, not *outcome*.
- **Seam:** consumes `SoftLockTarget` (D2) and combat state (D1); exposes
  animation states + notify windows.
- **Why its own discipline:** meets both tests hard — motion-matching and
  thread-safe AnimGraph nodes are deep non-obvious doctrine, and a
  game-thread anim mistake is a silent perf/hitch bug that only shows under
  load.

### D4 — Online Services / Backend Engineer

- **Mandate:** own everything between the platform and the match — session
  lifecycle, lobby, the listen-server host path, and the abstraction seam
  that lets dedicated servers arrive later without a rewrite.
- **Owned systems (Slash Roller):** `OSSessionsSubsystem` (Steam OSS,
  `FindAndJoinBestSession`), the planned `OSLobbySubsystem`, listen-server
  host/invite flow, `IOSServerLifecycle` (listen → GameLift/FlexMatch
  migration seam), the session/matchmaking trust boundary (what is trusted
  from Steam vs validated in-game).
- **Key types:** `UGameInstanceSubsystem`, OSS delegates,
  `IOSServerLifecycle`, `OSSessionsSubsystem::FindAndJoinBestSession`.
- **Doctrine:**
  - **Abstraction at migration seams** — anything that swaps vendor later
    (Steam sessions → FlexMatch, listen → GameLift) hides behind an
    interface now; callers never know the backend.
  - **Platform trust is validated, not assumed** — a Steam ID or session
    token is an input to validate, not a fact. The session boundary is
    where "trusted from platform" ends and "validated in-game" begins.
  - **Join/travel honesty** — every consumer handles late-arriving state;
    PlayerState may be null on first frame; seamless travel re-creates
    actors. "Worked from map start" is not a claim about join-in-progress.
  - **Listen-server reality** — the host shares a process with a client:
    host-advantage and "works for the host" bugs are this discipline's
    standing review item.
- **Does NOT own:** in-match authority (D2 owns gameplay replication; D4
  owns getting players *into* the match and the server's lifecycle around
  it). The boundary between `OSLobbySubsystem` and `OSSessionsSubsystem` is
  a named contract, not ambient.
- **Seam:** hands a running, populated session to D2's match flow; exposes
  lifecycle events (`IOSServerLifecycle`) the rest of the game consumes.

### D5 — UI / UX Systems Engineer

- **Mandate:** own the widget layer — screen management, the CommonUI
  activatable stack, ViewModels, and the shared primitives every feature
  consumes.
- **Owned systems (Slash Roller):** the CommonUI layer stack (GameHUD /
  Menu / Modal), the GAS-driven HUD (health/stamina bars, magic-slot
  cooldown sweep, round timer, kill-leaderboard, kill feed), lobby +
  loadout-select screens, the death overlay (respawn + swap-while-dead),
  the match-end scoreboard.
- **Key types:** `UCommonActivatableWidget`, `ULocalPlayerSubsystem` UI
  manager, MVVM `UMVVMViewModelBase`, GAS attribute-change delegates,
  `RegisterGameplayTagEvent` for cooldowns.
- **Doctrine:**
  - **C++ base classes, Blueprint visuals** — logic/state/binding in
    C++/ViewModels; the BP subclass holds layout and animation only.
  - **MVVM over tick-polling** — a widget polling game state in `Tick` is a
    violation *and* a perf bug at scale; state pushes through ViewModels.
  - **One screen-management spine** — screens push/pop through the CommonUI
    stack; nobody hand-toggles visibility to fake navigation; back-handling
    comes from the stack.
  - **UI never touches authoritative state** — widgets read replicated
    state via ViewModels and send *intent* through the owning
    PlayerController. A widget calling a `Server` RPC directly is a netcode
    finding filed to D2.
  - **Replication-timing honesty** — bind defensively; PlayerState may be
    null for frames after join/travel; show honest empty states.
  - **Gamepad-navigable by default** (Steam/console reach); widget pooling
    for lists (scoreboard, kill feed); style from shared tokens, no
    per-screen forks.
- **Does NOT own:** any gameplay decision. It is a passive view + an intent
  sender.
- **Seam:** reads D1/D2 replicated state; sends intent to D2.

### D6 — Audio Systems Engineer (thin, folds into generalist)

- **Mandate:** own combat and UI audio via engine-native **MetaSounds**,
  triggered through GameplayCues, rollback-safe.
- **Owned systems (Slash Roller):** swing/hit/parry-clang/winded-gasp/
  kill-sting/countdown cues; MetaSound source assets.
- **Doctrine:**
  - **GameplayCues are the trigger path** — audio fires through the GAS cue
    system, never side-channel.
  - **No audio survives a cancelled ability** — looping/stateful sounds are
    `WhileActive`/`Removed` cue pairs (removal on cancel/rollback stops the
    MetaSound); confirmed-event one-shots fire from `Executed` cues on
    server-confirmed events, so a rolled-back whiff never made a sound.
- **Why not its own agent:** real doctrine, but small surface and the
  rollback rule is enforceable as a checklist — it rides `builder` with a
  cue-discipline contract rather than minting a specialist.

### D7 — Tools / Build / LiveOps Engineer

- **Mandate:** own the machinery that proves and ships the game — version
  control discipline, the validation ladder's execution, and the release
  pipeline.
- **Owned systems (Slash Roller):** Perforce/LFS locking discipline, CI
  (rungs 1–3 per push, rung 4 on netcode branches), the **Gauntlet**
  dedicated-server + 2-client harness, Steam depot upload (two App IDs,
  separate VDFs, restricted access), telemetry ingestion for the Balance
  Analyst.
- **Doctrine:**
  - **The ladder is law** — compile → headless specs → functional →
    Gauntlet networked smoke → perf; a rung that can't run is BLOCKED, never
    skipped.
  - **One owner per binary file per ticket** — `git lfs lock` / P4 checkout
    before editing a `.uasset`/`.umap`; two writers on one binary is
    unresolvable.
  - **Derived artifacts are never hand-edited** — fix the source, regenerate.
  - **Packaged sanity before any milestone** — editor ≠ packaged; cook
    strips and config layering only surface on the packaged build.
- **Maps to:** the `verifier` (runs the ladder, read-only) plus `builder`
  for the harness/scripts.

### Technical Direction (you — the lead)

- **Mandate:** own the cross-cutting architecture, arbitrate seams, and make
  the final call. Decompose work into packets; dispatch the right
  discipline; enforce the contracts; keep the tickets board true.
- **The standing laws you enforce** (the honesty ladder): Compiles ≠ works.
  PIE ≠ multiplayer. Listen ≠ dedicated. Live-coding ≠ clean build. Editor
  ≠ packaged. Multiplayer claims come in threes: server, acting client,
  observing client.
- **Maps to:** the `game-lead` skill (lead session).

### Adversarial Review (cross-cutting)

- **Mandate:** try to *break* any dangerous-domain change (netcode, sim
  math, data schema, save/load) before it lands — with a concrete attack,
  not vibes. Every finding is `input → wrong output`. Read-only by
  integrity: never fixes, only reports.
- **Maps to:** the `critic` (JUDGE / REFUTER).

---

## 4. The seams (where disciplines meet — the failure-prone joints)

```
   D4 Online ──(populated session)──► D2 Netcode ──(replicated results)──► D5 UI
   Services                              ▲   │                               │
      │                                  │   │ (authority + prediction)      │ (intent)
      │(IOSServerLifecycle)              │   ▼                               ▼
      ▼                            D1 Sim (pure math) ◄──(seeded inputs)── D-AI bots
   GameLift (future)                     ▲
                                         │(warp targets: SoftLockTarget)
                                   D3 Animation ──(notify windows → gameplay events)
                                         │
                                   D6 Audio (GameplayCues on confirmed events)

   D7 Tools/Build proves ALL of the above via the ladder.
   Technical Direction arbitrates every ──► ; Critic attacks every dangerous ──►.
```

**The five seam laws (where bugs actually live):**
1. **D1↔D2:** sim returns results; only D2 decides authority + replication.
   Sim never calls `HasAuthority`; net never inlines a damage formula.
2. **D2↔D5:** UI reads replicated state and sends intent; never mutates a
   replicated property or calls a Server RPC directly.
3. **D3↔D1/D2:** animation requests (notify → gameplay event); it never
   computes damage (D1) or confirms a hit (D2).
4. **D4↔D2:** services deliver players into a match; in-match authority is
   D2's. The lobby/sessions boundary is a named contract.
5. **Everyone↔D2:** any new replicated surface is a D2 packet. Others file a
   `contract_gap`, never wing a `Server` RPC.

---

## 5. Slash Roller: Arena — every shipped system tagged to its owner

| System (from the GDD) | Primary | Consults |
|---|---|---|
| Combat triangle + stamina/winded math | D1 | D3 (feel), design (numbers) |
| Damage / mitigation / cooldown formulas | D1 | — |
| Ability replication + prediction keys | D2 | D1 |
| `SoftLockTarget` + target assist replication | D2 | D3 |
| Match timer / kill scoring / respawn authority | D2 | D1 |
| Motion warping on strikes | D3 | D2 |
| Locomotion / combat anim layers | D3 | — |
| Steam listen-server host / invite | D4 | D2 |
| Lobby + loadout select (backend) | D4 | D5 |
| Deterministic bots (GAS-native state machine) | D1 + a bot owner | D2 |
| CommonUI stack + GAS HUD | D5 | D2 |
| MetaSounds combat cues | D6 | D1/D2 (cue triggers) |
| Caster Agent (runtime LLM, host-side HTTP) | D4 | D2 (replicate strings) |
| Telemetry ingestion → Balance | D7 | D1 |
| Gauntlet ladder + Steam depot | D7 | all |

Two notes this table surfaces:
- **Bots** sit across D1 (deterministic decision logic, seeded, testable)
  and a thin bot-owner for perception wiring — they are a *player the AI
  drives*, so they live mostly in the sim discipline, not a separate AI
  engine.
- **The Caster Agent** (runtime LLM) is a **D4/services** concern, not
  gameplay: it's an async host-side HTTP call whose only output is
  replicated strings. It deliberately touches nothing D1/D2 own.

---

## 6. What this unlocks (next step, not this doc)

With the disciplines drawn, the crew formulation becomes mechanical — each
discipline becomes an agent definition with the owner_path and doctrine
already written above:

- D1 → `sim-builder`, D2 → `netcode-builder`, D5 → `ui-builder` — already
  in the kit; we fill their owner_paths and Slash Roller doctrine.
- D3 → **new `anim-builder`**, D4 → **new `services-builder`** — mint these
  two (both pass the silent-confident + real-doctrine test).
- D6 → folds into `builder` under a cue-discipline contract.
- D7 → `verifier` (ladder) + `builder` (harness).
- Direction → `game-lead`; Review → `critic`; bulk content (bot tables,
  arena manifests) → the **data-curator** pattern when we import it.

The contracts (`netcode.md`, `data-and-assets.md`, `testing.md`) get their
`[ ]` fill-ins from this doc: net topology = **listen servers allowed**
(host-advantage review on every packet); movement/ability stack = **GAS
prediction keys**; prefix = `OS`; source of truth for numbers = DataTable
CSVs in `Content/Data/` + `Source/SR/*/Data/`.

**Decision needed before we formulate:** confirm the seven-discipline split
(especially minting `anim-builder` and `services-builder` as their own
agents vs. folding animation into sim and services into netcode). Once you
approve the boundaries, the crew authoring is a direct transcription of §3.
