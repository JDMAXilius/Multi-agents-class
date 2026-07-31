# Crew Map — structure, cycles, and when to call whom

The visual layer of the crew (class doctrine: diagrams reveal bottlenecks and circular
dependencies before they cause failures). Two graphs and one matrix. **Every intentional
cycle prints its exit condition on the edge** — a cycle without one is the deadlock the
first pipeline build actually hit.

## 1. The crew graph — 12 agents, 3 powers, exits printed

```mermaid
flowchart TD
    LEAD["YOU + game-lead skill<br/>(the manager — human + deterministic script,<br/>NOT an LLM agent, by ruling R14)"]
    ORCH["deterministic orchestration<br/>run_crew.py · tickets board · hooks/guard_laws.py"]
    LEAD --> ORCH

    subgraph PROD["PRODUCERS (write, one owner_path each)"]
        B1["builder<br/>generalist·audio·harness"]
        B2["sim-builder<br/>gameplay math"]
        B3["netcode-builder<br/>replication law"]
        B4["anim-builder (M2)"]
        B5["services-builder (M5)"]
        B6["ui-builder (M4)"]
        B7["ai-builder (M3)<br/>3-layer bot brain"]
    end

    subgraph CUR["CURATORS (read-only, RETURN data)"]
        C1["arena-architect<br/>arena_manifest"]
        C2["tuning-curator<br/>ALL numbers incl. DT_BotAmbitions"]
        C3["spotter — DIVERGENT<br/>pool of ~10 → critic ranks → top 3 land<br/>fallback+medals: NOW · coach lines: M4"]
    end

    subgraph REV["REVIEWERS (read-only)"]
        R1["critic (opus)<br/>JUDGE / REFUTER<br/>reads DESIGN-RULINGS.md"]
        R2["verifier (haiku)<br/>NO write tools<br/>cost-ordered rungs"]
    end

    ORCH --> PROD & CUR
    PROD -- "diff + report" --> R1
    CUR -- "schema'd records" --> R1
    R1 -- "findings (HIGH only blocks)<br/>EXIT: ≤3 bounces, final round =<br/>hard violations only, else escalate" --> PROD
    R1 -- "same exit" --> CUR
    R1 -- PASS --> R2
    R2 -- "FAIL → back to producer<br/>PASS → land" --> MEM

    MEM[("SHARED MEMORY (git)<br/>many readers · ONE writer per artifact<br/>contracts · DESIGN-RULINGS · tickets+Logs ·<br/>Content/Data/*.csv · arena_manifest")]
    MEM -. "context minimalism:<br/>ticket + named contracts only" .-> PROD
```

## 2. The ticket dependency graph (the board as a DAG — cycle-hunting surface)

```mermaid
flowchart LR
    BP00["BP00 ladder"] --> BP02 & BP03 & BP04
    BP01["BP01 skeleton+input"] --> BP00 & BP02
    BP02["BP02 GAS core"] --> BP03 & BP05
    BP03["BP03 weapons+fire"] --> BP05 & BP09
    BP04["BP04 match frame"] --> BP08 & BP10 & BP11
    BP05["BP05 golden triangle ⚠fun gate"] --> BP06
    BP06["BP06 grapple"] --> BP07
    BP07["BP07 arena"] --> BP08
    BP08["BP08 bots (3-layer brain)"] --> BP10
    BP09["BP09 rocket"] --> BP08
    BP10["BP10 HUD+frontend"] --> BP12
    BP11["BP11 Steam"] --> BP12["BP12 ship"]
    BP13["BP13 data crew ✅ran"] -- DT_Weapons --> BP03
    BP13 -- manifest --> BP07
    BP14["BP14 engine bridge"] -.-> BP00
```

No cycles — and keeping it that way is this diagram's job. A new ticket that would create
one (X needs Y, Y needs X) gets split at cut time, not discovered mid-milestone.

## 3. Invocation matrix — who wakes when

| Trigger | Invoke | Mode |
|---|---|---|
| Tuning numbers / arena data / bot ambitions | `run_crew.py --job …` | pipeline (runs anywhere) |
| Any code ticket | `/tickets <name>` → game-lead dispatches per ticket's Crew line | terminal build session |
| M1 Foundry (BP00–BP04) | builder · sim-builder · netcode-builder (+critic, verifier always) | terminal |
| M2 Triangle (BP05–BP06) | + **anim-builder wakes** | terminal |
| M3 Playground (BP07–BP09) | + **ai-builder wakes** (brain per BREACHPOINT-AI-BOTS.md) | terminal |
| Flavor text the game ships (`DT_SpotterLines`, medals) | **spotter — fallback half, available now** (its inputs are the GDD, the manifest's callouts, and `DT_Weapons` — no telemetry involved) | pipeline |
| M4 First Match (BP08/BP10) | + **ui-builder wakes** · **spotter's coach half wakes** (telemetry finally exists) · nightly soaks start | terminal + cron |
| M5 Two Boxes (BP11) | + **services-builder wakes** | terminal + 2 machines |
| Any replicated surface, any time | netcode-builder + critic-writes-the-cheat, non-negotiable | — |
| A ruling is questioned | founder/lead edits DESIGN-RULINGS.md — never a review outcome | — |

**Parallel pods (simultaneous builders):** one git worktree per active builder, each claimed
to a different ticket (different owner_paths — the hook enforces non-collision), binaries
locked per law 7, lead session reconciles by pulling the board. Recipe: CREW_PLAYBOOK §12.

**Standing rule:** at any moment you run 4–7 agents, not 12. Dormant ≠ deleted — a dormant
agent costs nothing until its KICKOFF conditions turn true.
