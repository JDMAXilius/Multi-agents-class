# BREACHPOINT NEXT — the domain map

**Cut:** 12 August 2026 · **Companion to:** [`BREACHPOINT-NEXT-STRUCTURE.md`](BREACHPOINT-NEXT-STRUCTURE.md) (v3.1)
· [`BREACHPOINT-NEXT-RESEARCH.md`](BREACHPOINT-NEXT-RESEARCH.md)

The structure doc says where every file lives. This document says **who covers it and how** —
the framework divided into programming domains, so work always starts from "which domain is
this" and never from "which file looks close."

**Derivation.** Three inputs, converging on the same split:
1. **Industry role divisions** — studios specialize into gameplay, engine, AI, UI,
   network/multiplayer, tools, audio, physics/graphics programmers; AAA divides hard, indies
   generalize (sources at bottom).
2. **This repo's own D1–D8** (`docs/method/ENGINEERING-DISCIPLINES.md`) — already drawn as
   *ownership boundaries with doctrine*, already validated against practice, already mapped to
   crew agents. The domains below are D1–D8 re-cut for the NEXT tree, not a new invention.
3. **The D1–D8 minting rule, reapplied:** a domain earns its own boundary only when mistakes
   there are *silent-and-confident* AND its doctrine is non-obvious. Everything failing both
   tests folds into Gameplay. This is why there are **seven**, not twelve.

---

## 1. The one distinction that makes the map work

**Resident domains own folders. Overlay domains own rules that cut across folders.**

Multiplayer is the proof: there is no `Multiplayer/` folder in the tree — and there must never
be one, because replication is not a place, it is a property of gameplay files. `BRCharacter`
has replicated state; `BRGA_Fire` has a prediction window; `BRPlayerState` replicates attributes.
If multiplayer were a folder, every one of those files would need two homes — the structure
doc's rule 3 violated everywhere, and authority bugs living in whichever copy nobody reviews.
The industry job title "network programmer" describes someone who works *inside other people's
systems* — the domain map has to say the same thing.

So: **five resident domains** (folders, files, a normal owner) and **two overlay domains**
(marked surfaces inside resident files, a doctrine, and veto power over their surface).

---

## 2. The seven domains

### Resident

| # | Domain | Mission — one line | Owns (from the v3.1 tree) | Lineage |
|---|---|---|---|---|
| **DOM-1** | **Gameplay** | The verbs and the truth: everything a player *does* and the rules that judge it | `Core/` · `Data/` (rows/loading) · `Input/` · `AbilitySystem/` (all) · `Characters/` · `Actors/` · `Weapons/` · `Match/` · `Interfaces/` · `Utilities/` | D1 |
| **DOM-2** | **Animation** | The body reads the sim and never writes it | `Animation/` (instances, layers, notifies) + the Tier-4 graph assets | D3 |
| **DOM-3** | **AI** | Bots that press the same tags players press — deterministic, no privileged path | `AI/` (controller, ambition scorer, StateTree) | D8 |
| **DOM-4** | **UI** | CommonUI + MVVM: render truth, mutate nothing | `UI/` (all) — subsystem, layout, components, ViewModels, screens | D5 |
| **DOM-5** | **Backend / Online Services** | The cloud around the game: identity, sessions, hosting — trust validated, never assumed | `Online/` (lifecycle seam, sessions) · EOSPlus/Steam config · GameLift Phase 2 · the meta-backend (Lambda/API GW, per GAMELIFT-PLAN) | D4 |

### Overlay

| # | Domain | Mission — one line | Its surface (inside resident files) | Lineage |
|---|---|---|---|---|
| **DOM-6** | **Multiplayer / Netcode** | Clients send intent, the server owns truth — on every wire, every time | Every `UPROPERTY(Replicated)` · every RPC + `_Validate` · `GetLifetimeReplicatedProps` · prediction keys and windows · `FSavedMove_BR` / CMC prediction · TargetData validation · net relevancy | D2 |
| **DOM-7** | **Data & Build Management** | The numbers, the pipelines, and the proof the build works | `Content/Data/*.csv` + `arena_manifest.json` (values; *schemas* are DOM-1's `BRDataRows.h`) · `Tools/` (generators, reimport, ladder scripts) · `Tests/` harness + CI · packaging | D7 (+ the data half of the old data-and-assets contract) |

**Your list, mapped:** gameplay ✓ DOM-1 · animation ✓ DOM-2 · AI ✓ DOM-3 · UI ✓ DOM-4 ·
backend ✓ DOM-5 · multiplayer ✓ DOM-6 · *"distributed/data management"* → DOM-7 (interpreted as
data + build management — rename if you meant something else).

**Deliberately NOT domains** (fail the minting rule): *Audio* — routed through GameplayCues,
which DOM-1 owns; becomes a domain only if a dedicated audio system (MetaSounds param graphs,
occlusion) actually lands. *Physics/Graphics/Engine* — engine-tier in UE; Breachpoint writes
none. *Tools as a separate role* — folded into DOM-7. *Testing* — not a domain: **every domain
owns its own specs**; DOM-7 owns the harness they run on.

---

## 3. File-by-file: the tree colored by domain

Resident ownership is total per folder (no shared folders — that is what made the structure
doc's rule 2 work), so the map is short:

```
Core/ Data/ Input/ AbilitySystem/ Characters/ Actors/ Weapons/ Match/     DOM-1 Gameplay
Interfaces/ Utilities/                                                    DOM-1 Gameplay
Animation/                                                                DOM-2 Animation
AI/                                                                       DOM-3 AI
UI/                                                                       DOM-4 UI
Online/                                                                   DOM-5 Backend
─────────────────────────────────────────────────────────────────────────
Tests/ · Tools/ · Content/Data/ values                                    DOM-7 Data & Build
DOM-6 Multiplayer = the replicated/predicted surface of ALL of the above
```

**The DOM-6 overlay, named file by file** (where the silent-and-confident bugs live):

| File | The multiplayer surface inside it |
|---|---|
| `BRAbilitySystemComponent` | prediction keys, batched activation RPC, input buffering across the wire |
| `BRGA_Fire` | the predicted window; client hit → server validation via TargetData |
| `BRAttributeSet` | attribute replication, `OnRep`, IncomingDamage as server-only meta |
| `BRCharacter` | movement replication, equipped-weapon `OnRep`, death propagation |
| `BRCharacterMovementComponent` | `FSavedMove_BR` compressed flags, grapple root-motion prediction, zero-residue rejection |
| `BRPlayerState` | the ASC host's replication; team, score |
| `BRGameState` / `BRMatchMessages` | match truth + verb-message replication to all clients |
| `BRWeapon` / `BREquipmentComponent` | replicated equip state; grant/revoke happens on authority only |
| `BRPlayerController` | client RPC target; ownership boundary |
| `BRServerLifecycle` | admission (`ValidateJoin`) — the trust line's front door |

Rule inherited from the crew law: **touching a DOM-6 surface = a DOM-6 review** (the critic
writes the cheat), no matter which resident domain's file it is in.

---

## 4. How each domain is covered — and who tests what

Founder's call, recorded: **you test; the crew builds.** In-editor and in-match verification
(PIE, feel, the multiplayer rungs) is yours. The crew verifies everything that needs no engine.

| Domain | The crew verifies (no engine) | You verify (engine/feel) |
|---|---|---|
| DOM-1 Gameplay | law greps (one damage door, no Tick, no literals), header hygiene, spec logic written | PIE: does it *feel* right; rung 1–2 runs |
| DOM-2 Animation | thread-safety rules (no graph computation), soft-ref discipline | in-editor: graphs read fields, poses look right |
| DOM-3 AI | determinism review (no wall-clock, no `RandRange`), StateTree node code | bots in PIE: believable, reproducible |
| DOM-4 UI | zero-polling proof (no Tick, no per-frame binds), ViewModel-only access | screens in editor: layout, focus, gamepad |
| DOM-5 Backend | seam shape (GameLift-ready interface), config review, token flow | Steam/EOS login, session join on real accounts |
| DOM-6 Multiplayer | `_Validate` completeness, prediction-window review, the written cheat | **the rung that matters: 2 clients + server — asserts in threes** |
| DOM-7 Data & Build | CSV↔row-struct schema match, generator determinism | table reimport in editor; packaged build boots |

*(The honesty ladder is unchanged: the crew's checks stop at "compiles/reviewed"; only your
runs earn "works," and every claim names its rung.)*

---

## 5. The starting slice, read through the domain map

The agreed first move — *not broken down here, just located*: the framework starts with
**DOM-1 as the trunk plus the minimum of DOM-2 and DOM-6 that makes DOM-1 honest**:

- **DOM-1:** `BRCharacter` → the player pawn path · `BRPlayerState` (ASC host) ·
  `BRPlayerController` (inputs) · the GAS spine (`BRAbilitySystemComponent`, `BRAttributeSet`,
  `BRGameplayAbility`, first abilities)
- **DOM-2, basic:** `BRAnimInstance` reading the sim
- **DOM-6, from line one:** those files' replicated surfaces built server-authoritative —
  *not* retrofitted later, because DOM-6 is an overlay, not a phase

DOM-3, DOM-4, DOM-5 join later; DOM-7's harness exists from the start because specs land with
the first math. The deep breakdown of this slice is the next conversation.

---

### Sources

- Industry role splits: [gamedesignskills.com — Game Programmer roles](https://gamedesignskills.com/game-programming/programmer/) ·
  [8bitplay — programmer job roles explained](https://8bitplay.com/blog/game-programmer-job-roles-explained-a-recruiter-guide/) ·
  [research.com — programming specializations](https://research.com/online-degrees/game-programming-development/how-to-choose-the-right-game-programming-and-development-specialization-online) ·
  [circuitstream — career pathways](https://www.circuitstream.com/blog/your-guide-to-the-different-career-pathways-you-can-take-in-the-game-development-industry)
- In-repo: `docs/method/ENGINEERING-DISCIPLINES.md` (D1–D8 and the minting rule) ·
  `BREACHPOINT-GAMELIFT-PLAN.md` (DOM-5's Phase-2 shape) · `docs/contracts/` (the doctrine each
  domain enforces)
