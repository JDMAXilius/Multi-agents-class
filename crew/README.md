# Breachpoint — Agent Crew (UE 5.8)

The **formulated crew for Breachpoint** — the UE5 Multiplayer Agent Crew Kit adapted
to this project: contracts filled with Breachpoint values, three discipline builders and
three curators added per `docs/ENGINEERING-DISCIPLINES.md` (D1–D8), first ticket cut.
The kit's methodology was battle-tested on a shipped production app (adversarial review
caught five real defects — two confidently-wrong — in a single working day).

## Install

Drop the contents of this folder at the **game repo's root** (`Breachpoint/`):

```
Breachpoint/
├── .claude/
│   ├── agents/          ← 9 agents + curators/ (3 data curators)
│   └── skills/          ← game-lead (operating mode) + tickets (handoff board)
└── docs/
    ├── CREW_PLAYBOOK.md ← the methodology — read this first
    ├── contracts/       ← netcode, data-and-assets, testing, animation, online-services, gas-purity
    └── tickets/         ← TICKET_BP00–BP04 are the Week-1/2 set; BP01 is the first pickup
```

Claude Code picks up `.claude/agents/*.md` and `.claude/skills/` automatically. Contracts are
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
| `bot-trainer` | `DT_BotTuning` rows per difficulty tier |
| `arena-architect` | `arena_manifest.json` (bounds, scored spawns, landmarks) |
| `balance-analyst` | tuning diffs when telemetry breaks the 45–55% band |

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
