# Breachpoint — Agent Crew (UE 5.8)

The **formulated crew for Breachpoint** — the UE5 Multiplayer Agent Crew Kit adapted
to this project: contracts filled with Breachpoint values, three discipline builders and
the curators added per `docs/method/ENGINEERING-DISCIPLINES.md` (D1–D8), first ticket cut.
The kit's methodology was battle-tested on a shipped production app (adversarial review
caught five real defects — two confidently-wrong — in a single working day).

## Install

Drop the contents of this folder at the **game repo's root** (`Breachpoint/`):

```
Breachpoint/
├── CLAUDE.md            ← always-on project memory: the 8 laws in one page (read first)
├── .claude/
│   ├── settings.json    ← wires the law-enforcement hook
│   ├── hooks/           ← guard_laws.py: owner-path + banned-API blocks at tool-call time
│   ├── agents/          ← 9 agents + curators/ (3: 2 convergent, 1 divergent) — each with
│   │                       ROUTING (one obvious owner per task) · I/O (schema'd handoffs)
│   │                       · KICKOFF (machine-checkable start conditions)
│   └── skills/          ← game-lead (dispatch + rulings duty) · tickets (kickoff-gated claims)
│                          · gas-purity (HOW the lawful GAS patterns are built; contract wins)
│                          · ue-editor (Python/commandlet automation; generated-scripts-over-
│                            live-MCP doctrine for blockouts, reimports, screenshots)
│                          · ue5-ui-architecture (CommonUI layer stack + MVVM ViewModels;
│                            BP10's reference layer — zero-polling, honest join-in-progress) ⚠
│                          · gauntlet-testing (HOW rung 4 is built: roles, assert-in-threes
│                            via role artifacts, emulation, BLOCKED) ⚠ — BP00 step 3
│                          · cmc-prediction (saved moves, compressed-flag budget, grapple
│                            root-motion source, zero-residue rejection) ⚠ — BP02/BP06
│                          ⚠ = UNVERIFIED draft: written from docs, never run against a
│                            build. The packet that first uses one corrects it in that
│                            same packet and drops the banner.
└── docs/
    ├── CREW_PLAYBOOK.md   ← the methodology (§9–13: severity gate, cost-ordered gates,
    │                        memory policy, parallel pods, hooks — all run-proven)
    ├── CREW_MAP.md        ← the diagrams: crew graph w/ printed exits · ticket DAG ·
    │                        invocation matrix (who wakes at which milestone)
    ├── DESIGN-RULINGS.md  ← the ledger reviews judge against (closed doubts stay closed)
    ├── contracts/         ← netcode, data-and-assets, testing, animation, online-services, gas-purity
    │                        (D5 UI and D6 audio have no contract of their own: UI's law is
    │                         CLAUDE.md 3/4 + ARCHITECTURE §3.9, its patterns are the
    │                         ue5-ui-architecture skill, and its grep gates are enforced from
    │                         testing.md. Cut one only if that split starts costing packets.)
    └── tickets/           ← TICKET_BP00–BP15: the full board; BP01 is the first code pickup.
                              BP15 is the Architect — deterministic perception + scoring in
                              front of the gates; an LLM never chooses the unit.
                              BP13 (the data crew) has RUN — table + manifest landed and
                              verifier-proven; BP08 recut for the three-layer bot brain
                              (BREACHPOINT-AI-BOTS.md); BP14 is terminal-only (needs UE)
```

Claude Code picks up `CLAUDE.md`, `.claude/agents/*.md`, and `.claude/skills/` automatically.
Day-to-day operations (session topology, model assignment, escalation, metrics):
`docs/method/CREW-OPERATIONS.md` in the planning repo. Contracts are
already filled for Breachpoint (UE 5.8 · listen server behind IBRServerLifecycle · GAS
prediction keys · `BR` prefix, one module · soft-refs + generic-GE laws · CSVs in `Content/Data/`).

## The crew at a glance

**Producers** (write code, one owner_path each):

| Agent | Discipline | The danger it guards |
|---|---|---|
| `builder` | generalist (+ D6 audio, D7 harness) | scope creep, drive-by edits |
| `sim-builder` | D1 gameplay math | math drifting into net/engine glue, hardcoded numbers |
| `netcode-builder` | D2 replication/authority | the **silent-and-confident** bug: works in PIE, dupe exploit in production |
| `anim-builder` | D3 animation systems | game-thread anim hacks, warp targets from local guesses |
| `services-builder` | D4 sessions/lobby/lifecycle | "works for the host", unvalidated platform trust |
| `ui-builder` | D5 CommonUI/MVVM | tick-polling widgets, UI touching authoritative state |
| `ai-builder` | D8 bots + runtime Caster | non-deterministic bots, an LLM anywhere near the sim |

**Reviewers** (read-only):

| Agent | Role |
|---|---|
| `critic` | JUDGE or REFUTER — for netcode it **writes the cheat**; findings need input → wrong output |
| `verifier` | runs the ladder verbatim; **no write tools by capability** — cannot fake a pass |

**Curators** (read-only, RETURN data against a schema; critic refutes, a builder lands):

| Agent | Returns |
|---|---|
| `arena-architect` | `arena_manifest.json` (bounds, scored spawns, landmarks) |
| `tuning-curator` | ALL gameplay numbers: `DT_Weapons` + `DT_BotTuning` + `DT_BotAmbitions` rows and balance diffs (**convergent**: proposes only outside the 45–55% band — restraint is a deliverable) |
| `spotter` | Every line the game speaks: `DT_SpotterLines` fallback, coach lines, medal names (**divergent**: returns a pool of ~10 per slot, critic ranks, top ~3 land) |

## The five laws (from the playbook — the part that must survive any adaptation)

1. **Separation of powers.** Builders write, the critic refutes, the verifier proves. Nobody
   verifies their own work; the verifier cannot write at all.
2. **Owner-path scoping.** A packet names the one folder/module its builder may touch. A diff
   outside it is rejected, never "just a tiny fix."
3. **Contracts are law.** Missing something? File a `contract_gap` and stop — never edit shared
   code to unblock yourself.
4. **Honesty law.** Compiles ≠ works. PIE ≠ multiplayer. Single-process ≠ networked.
   Live-coding ≠ clean build. Editor ≠ packaged. Every claim of "works" names the rung of the
   ladder that proved it.
5. **Adversarial review before landing.** Every engine-law change (netcode, sim math, data
   schema) gets a REFUTER pass with a concrete attack, not vibes. Findings need a failure
   scenario: input → wrong output.

## Version note

Written against **UE 5.8** (the final UE5 release — performance-focused; the networking
framework is unchanged from prior 5.x, so all doctrine here is stable and should carry to UE6
with path updates). Multi-instance functional testing remains weak in-engine; the ladder's
multiplayer rung uses **Gauntlet** (see `docs/contracts/testing.md`).
