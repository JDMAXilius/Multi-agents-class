# Slash Roller — Crew Roster & File Structure
## The complete agent framework: what exists, what we add, what each does

**Reads with:** `ENGINEERING-DISCIPLINES.md` (the D1–D8 boundaries this
roster staffs) and the UE5 Crew Kit (`CREW_PLAYBOOK.md`, contracts).
**Status:** inventory + plan — the six kit agents exist; five are proposed
(3 discipline builders + reference to curators). Nothing here is minted
until the eight-discipline split is confirmed.

---

## 1. The three KINDS of agent (this is the framework)

Every agent is exactly one of three kinds, defined by the **power it holds**.
This separation of powers is the whole methodology — mixing them is how
review theater happens.

| Kind | Power | Can write? | Verifies own work? | Examples |
|---|---|---|---|---|
| **Producer** (builder) | writes code/content in ONE owner_path | ✅ yes, scoped | ❌ never | builder, netcode-builder, sim-builder, ui-builder, anim-builder, services-builder, ai-builder |
| **Reviewer** | read-only, adversarial or proving | ❌ no | — | critic (attacks), verifier (proves) |
| **Curator** | read-only, RETURNS structured data against a schema | ❌ no (returns, never lands) | — | bot-trainer, arena-architect, balance-analyst |

Plus one **operating mode** (not an agent — a skill the lead session runs):
`game-lead`. And one **handoff mechanism** (also a skill): `tickets`.

---

## 2. Roster at a glance — the counts

| Category | Count | Files |
|---|---|---|
| **Producer agents — in kit** | 4 | builder, netcode-builder, sim-builder, ui-builder |
| **Producer agents — to add** | 3 | anim-builder, services-builder, ai-builder |
| **Reviewer agents — in kit** | 2 | critic, verifier |
| **Curator agents — to add** | 3 | bot-trainer, arena-architect, balance-analyst |
| **Skills (operating modes)** | 2 | game-lead, tickets |
| **Contracts (law files)** | 3 + 2 | netcode, data-and-assets, testing (+ animation, online-services proposed) |
| **Process docs** | 2 | CREW_PLAYBOOK.md, TICKET_TEMPLATE.md |
| **TOTAL AGENTS** | **12** | 9 producers/reviewers + 3 curators |

---

## 3. Producer agents — the builders (one owner_path each)

### In the kit already (4)

**`builder`** — the generalist.
- **Owns:** one work packet in one owner_path; module glue, integration,
  anything without a specialist.
- **Guards against:** scope creep, drive-by edits outside its path.
- **Discipline:** catch-all (+ D6 audio under a cue contract, D7 harness).

**`netcode-builder`** — D2 Multiplayer / Netcode.
- **Owns:** replicated surface of every module, server authority, RPCs,
  GAS prediction keys, `SoftLockTarget` replication, match/respawn authority.
- **Guards against:** the silent-confident bug — client-authoritative code
  that passes PIE and ships as a dupe/god-mode exploit.
- **Signature rule:** authority gate on every mutation; RPCs validate or
  don't ship; the attack ships with the feature.

**`sim-builder`** — D1 Gameplay Systems.
- **Owns:** deterministic combat math — the triangle, stamina/winded,
  damage/cooldown formulas, kill scoring; headless-testable pure C++.
- **Guards against:** gameplay math drifting into net/engine glue where it
  can't be tested; hardcoded numbers.
- **Signature rule:** determinism is law (seeded randomness passed in);
  numbers in DataTables; pin behavior with spec suites.

**`ui-builder`** — D5 UI / UX.
- **Owns:** CommonUI activatable stack, MVVM ViewModels, GAS-driven HUD,
  shared widget primitives.
- **Guards against:** per-screen style forks, tick-polling widgets, UI
  touching authoritative state.
- **Signature rule:** MVVM over polling; widgets read replicated state and
  send intent only; C++ base classes, Blueprint visuals.

### To add — new discipline builders (3)

**`anim-builder`** — D3 Animation Systems. *(new)*
- **Owns:** AL Framework, Motion Matching/PoseSearch, the 7 custom
  AnimGraph nodes, motion warping on strikes, locomotion/combat anim layers.
- **Guards against:** game-thread anim hacks (silent hitch bugs under load);
  warp targets drifting from replicated truth.
- **Signature rule:** anim logic on the worker thread; warp targets come
  from `SoftLockTarget` (D2); animation requests, never decides damage.

**`services-builder`** — D4 Online Services / Backend. *(new)*
- **Owns:** `OSSessionsSubsystem`, `OSLobbySubsystem`, listen-server host/
  invite, `IOSServerLifecycle` (the GameLift migration seam), platform
  trust boundary.
- **Guards against:** "works for the host" bugs; unvalidated platform trust;
  join-in-progress null state.
- **Signature rule:** abstraction at migration seams; platform trust is
  validated not assumed; join/travel honesty.

**`ai-builder`** — D8 AI Systems. *(new)*
- **Owns:** the deterministic bot decision layer (stance machine,
  perception, slot-fill) and the runtime Caster Agent HTTP client.
- **Guards against:** non-deterministic bots (unfair/irreproducible online);
  an LLM call blocking or steering the simulation.
- **Signature rule:** AI produces intent + replicated strings, never
  simulation state; bots activate abilities through the human input path;
  no LLM in the hot path; the API key never leaves the host.

---

## 4. Reviewer agents — read-only (2, both in kit)

**`critic`** — adversarial reviewer, rung V2. Two modes set by the packet:
- **REFUTER:** actively break it. For netcode, *write the cheat* (forged
  RPC, out-of-range value, spam, desync repro). For sim, the input that
  breaks an invariant (minted currency, negative cooldown). For UI, the
  stale/null replicated state on first frame.
- **JUDGE:** score competing designs against the contracts, name the winner,
  say what to graft from the losers.
- **Law:** every finding is `input → wrong output`; read-only is its
  integrity — it never fixes, only reports.

**`verifier`** — runs the validation ladder, rung V1. Read-only **by
capability** (no write tools at all — "quietly patched the test to pass" is
structurally impossible).
- **Runs, in order:** clean compile → headless Automation Specs → functional
  tests → **Gauntlet dedicated-server + 2 clients** → perf spot-checks.
- **Law:** run don't fix; report verbatim; a rung that can't run is BLOCKED,
  never skipped; every "works" names its rung.

---

## 5. Curator agents — read-only data producers (3, to add)

These are the crew form of your GDD's dev-time content agents. A curator
**returns structured records against a fixed schema; it never writes files.**
The critic refutes samples, a builder lands the data. This keeps
agent-produced content reviewable and out of binaries.

**`bot-trainer`** — returns `DT_BotTuning` rows (aggression, parry %,
reaction ms, stamina discipline) per difficulty tier. Owns the bot
*numbers* (the `ai-builder` owns the bot *code*).

**`arena-architect`** — returns `arena_manifest.json` (bounds, spawn points
with scoring hints, landmarks) from a one-paragraph brief; drives the UE MCP
to block out geometry, iterates from screenshots.

**`balance-analyst`** — reads match telemetry, returns DataTable tuning
diffs + rationale when a loadout/verb exceeds win-rate bounds (55% triggers
review).

*(Combat QA from the GDD maps onto the `verifier` running nightly bot-vs-bot
soaks, not a separate curator — it proves, it doesn't produce data.)*

---

## 6. Skills — operating modes (2, not agents)

**`game-lead`** — the lead session's operating mode: decompose work into
packets, dispatch the right builder, enforce contracts, keep the tickets
board true, apply the honesty laws. This is *you* (or the lead session)
running the crew — it holds all powers because it never builds directly.

**`tickets`** — the session-to-session handoff board. `docs/tickets/*.md` is
the shared memory; git is the channel. `/tickets list | <name> | done`.

---

## 7. Contracts — the law files every agent obeys (3 + 2 proposed)

| Contract | Binds | Status |
|---|---|---|
| `netcode.md` | D2 — authority, RPC validation, replication minimums, prediction | in kit (fill-ins pending) |
| `data-and-assets.md` | all — numbers in DataTables, logic in C++, BP-thin, binary locking | in kit |
| `testing.md` | verifier — the five ladder rungs, Gauntlet scenario | in kit (fill-ins pending) |
| `animation.md` | D3 — worker-thread rules, warp-target sourcing *(proposed)* | to add |
| `online-services.md` | D4 — migration-seam abstraction, trust boundary *(proposed)* | to add |

*(D8/AI doctrine is small enough to live in the `ai-builder` definition +
the netcode contract's "AI sends intent only" clause; it can graduate to its
own contract if it grows.)*

---

## 8. The full file tree (once formulated)

```
SlashRoller/                         ← game repo root
├── .claude/
│   ├── agents/                       ← 9 agent definitions
│   │   ├── builder.md                    (kit)
│   │   ├── netcode-builder.md            (kit)   D2
│   │   ├── sim-builder.md                (kit)   D1
│   │   ├── ui-builder.md                 (kit)   D5
│   │   ├── critic.md                     (kit)   review
│   │   ├── verifier.md                   (kit)   review
│   │   ├── anim-builder.md               (NEW)   D3
│   │   ├── services-builder.md           (NEW)   D4
│   │   ├── ai-builder.md                 (NEW)   D8
│   │   └── curators/                  ← 3 read-only data producers
│   │       ├── bot-trainer.md            (NEW)
│   │       ├── arena-architect.md        (NEW)
│   │       └── balance-analyst.md        (NEW)
│   └── skills/
│       ├── game-lead/SKILL.md            (kit)   operating mode
│       └── tickets/SKILL.md              (kit)   handoff board
└── docs/
    ├── CREW_PLAYBOOK.md                  (kit)   the method
    ├── contracts/
    │   ├── netcode.md                    (kit)   + SR fill-ins
    │   ├── data-and-assets.md            (kit)   + SR fill-ins
    │   ├── testing.md                    (kit)   + SR fill-ins
    │   ├── animation.md                  (NEW)
    │   └── online-services.md            (NEW)
    └── tickets/
        ├── TICKET_TEMPLATE.md            (kit)
        └── *.md                          ← live work board
```

---

## 9. Summary — the roster in one breath

**11 agents total (v2 — curators consolidated per the S03 scope red-flag: bot-trainer + balance-analyst → tuning-curator):** 7 producers (4 kit + 3 new), 2 reviewers (kit), 3
curators (new) — governed by 2 skills, bound by 3–5 contracts, coordinated
through 1 tickets board. Every producer owns exactly one discipline
(D1–D8); no producer verifies its own work; both reviewers are read-only;
curators return data they never land. That separation of powers is the
entire safety guarantee — and it is exactly the "crew that ships a small
game fast, professionally" thesis, made concrete.

**Next step:** on confirmation, author the 3 new agent files + 3 curators
+ 2 new contracts, fill the kit contract `[ ]` blanks with Slash Roller
values, and drop the whole tree into the repo ready to run.
