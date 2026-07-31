# Slash Roller — Architecture Validation
## Is the crew + engine approach actually best practice? A researched verdict.

**Question put to research:** are we using advanced, correct patterns on both
halves — (A) the UE5 multiplayer / GAS engineering architecture, and (B) the
multi-agent crew that builds it — and how do real studios and companies do
this? **Method:** checked our approach against Epic's own reference (Lyra),
the GAS community canon, industry team-structure guides, and Anthropic's
published multi-agent engineering. **Verdict up front: the approach is
sound and matches best practice on both halves — with three honest
adjustments below.**

---

## Part A — The engine architecture

### A1. Discipline split vs. how studios actually staff UE multiplayer

Our D1–D8 split is not invented — it mirrors the standard role decomposition
of real UE multiplayer teams. Industry guides list exactly these
specializations: **network/backend developers** (our D2/D4), **gameplay
engineers** implementing mechanics and core loops (D1), **AI programmers**
for NPC behavior (D8), **systems designers** for combat/economy/progression
(D1 data), and **technical designers** bridging design and code. [1][3]

**Verdict: CONFIRMED.** The only difference is that studios staff these with
humans and we staff them with agents — which is itself the current
recommended pattern (specialized subagents with clear boundaries, Part B).

### A2. GAS-native + server-authority + prediction

This is textbook Epic. The GAS canon confirms every load-bearing choice we
made:
- **Server authority with client prediction** via `NetExecutionPolicy`
  (LocalPredicted runs on client *and* server); the client produces
  TargetData and sends it to the server by RPC — exactly our "clients send
  intent, server simulates truth." [4][5]
- **Rollback via Gameplay Effects** — GEs support rollback if the server
  rejects a predicted ability, which is *why* GEs are the correct vehicle
  for tags, cues, and attribute changes. Our contract's "predicted state
  reconciles, cosmetic-only in OnRep" is the same rule. [5]
- **GAS set up in C++, abilities/effects authorable in Blueprint by
  designers** — precisely our "C++ for logic, Blueprints thin" line. [4]
- **Lyra is the canonical reference** for abilities, linked anim layers, and
  modular gameplay — which is the exact reference corpus our engine-doctrine
  notes already cite. [4]

**Verdict: CONFIRMED, strongly.** Our engine doctrine is the mainstream
best practice as published by Epic and the GAS community, not a bespoke
bet.

### A3. Data-driven + no-Tick + text-over-binary

- **DataTables/CSV for tuning numbers, C++ for rules** is the standard
  data-driven pattern and is what makes balance a designer-editable, git-
  diffable concern. [4]
- **No-Tick / event-driven** is advanced-correct: it's the performance-
  and-correctness discipline behind Epic's own modular gameplay (gameplay
  messages, ability events, timers over per-frame polling).
- **Text-over-binary** (keep logic/numbers out of `.uasset`) is *our*
  sharpening of the rule so a read-only reviewer can audit changes —
  defensible and, for an agent crew, necessary.

**Verdict: CONFIRMED.** These are "advanced programming / advanced data
structure" practices, correctly applied.

---

## Part B — The multi-agent crew

### B1. Separation of powers = orchestrator-worker + evaluator

Our crew is a textbook instance of the two patterns Anthropic and the
broader field converge on:
- **Orchestrator-worker:** a lead decomposes work into bounded subtasks and
  dispatches specialist workers; recommended as the default because it
  "handles the widest range of problems with the least coordination
  overhead." Our `game-lead` is the orchestrator; builders are the workers.
  [6][7]
- **Each subagent needs an objective, an output format, and tool/source
  guidance** — which is exactly what our packet (goal, owner_path,
  contracts, acceptance checks) provides. [6][8]
- **Evaluator/critic loop:** the adversarial `critic` + read-only `verifier`
  are the *evaluator-optimizer* pattern (a separate agent scores/attacks the
  producer's output). This is a recognized production pattern, not overkill.
  [7]

**Verdict: CONFIRMED.** Builder / critic / verifier / game-lead maps 1:1
onto orchestrator + worker + evaluator — the validated shape.

### B2. Ephemeral workers vs. persistent teammates

The field distinguishes **subagents** (spawned for one bounded task, then
terminate) from **teammates** (persist across many assignments, accumulate
context). Our builders are deliberately **ephemeral** — "you are disposable;
the packet is the job" — which matches the subagent model recommended when
subtasks are "short, focused, and produce clear outputs." [6][7]

**Verdict: CONFIRMED, and deliberate.** Ephemeral + owner-path scoping is
the lower-coordination-cost choice, correct for a small team.

### B3. Cost model

Anthropic's guidance: run the orchestrator on a capable frontier model and
workers on cheaper task-specific ones — a **40–60% cost cut** vs. frontier-
everything. Our design already does this: `verifier` is pinned to **Haiku**,
the lead/critic use stronger models, builders sit in between. [6]

**Verdict: CONFIRMED.** The model tiering is already best practice.

---

## Part C — The three honest adjustments (where research says tighten)

Research also flags where multi-agent efforts *fail*, and two of those
apply directly to us:

### C1. Orchestration design is the real risk — not agent count.
**57% of multi-agent project failures originate in orchestration design, not
agent capability.** [6] Implication: the highest-leverage part of our system
is **not** how many disciplines we drew — it's the `game-lead` operating
mode, the **contracts**, and the **tickets** handoff board. **Adjustment:**
treat the contract `[ ]` fill-ins and the ticket discipline as the priority
deliverable, above minting more agents. We already have this instinct;
research says double down on it.

### C2. Don't parallelize what doesn't decompose.
"Adding workers to a task that does not decompose cleanly buys coordination
cost with no quality return." [6] **Adjustment:** the crew is for *bounded,
independent packets* (one system, one owner_path). Cross-cutting work
(a refactor touching netcode + anim + UI at once) is a **lead-session job or
a serialized ticket chain**, not a parallel fan-out. Our owner-path law
already enforces this; keep it strict.

### C3. Multi-agent is token-expensive — reserve it for value.
Multi-agent systems burn far more tokens than single-agent, so the field
reserves them for high-value, parallelizable work. [6] **Adjustment for a
5-week solo timeline:** don't route trivial one-file edits through the full
builder→critic→verifier ladder — reserve the adversarial pass for the
**dangerous domains only** (netcode, sim math, data schema, save/load),
exactly as the playbook already says. The ladder is graduated, not
all-or-nothing.

---

## Part D — What the research did NOT change

- The **8-discipline split** stands — it matches real UE team roles (A1) and
  decomposes cleanly (each owns one testable boundary).
- **GAS-native, server-authoritative, predicted** stands — it's Epic's own
  best practice (A2).
- **Separation of powers** stands — it's the validated orchestrator +
  evaluator shape (B1), and the read-only verifier is a genuine structural
  guarantee, not ceremony.

---

## Verdict

**Both halves are best-practice and "advanced-correct."** The engine side is
mainstream Epic/Lyra doctrine applied rigorously; the crew side is the
orchestrator-worker-evaluator pattern the field converges on, with correct
model tiering and ephemeral scoping. The research's warnings are about
**operational discipline** (orchestration, decomposition, cost), not about
the architecture — and our contracts/tickets/graduated-ladder already answer
them. **Recommendation: proceed to formulate the crew, and make the
contract fill-ins + ticket bootstrap the first ticket (per C1).**

---

## Sources

1. [How to Create Multiplayer Games in Unreal Engine (MoldStud)](https://moldstud.com/articles/p-how-to-create-multiplayer-games-in-unreal-engine-a-complete-guide-for-beginners-and-experts)
2. [Networking Overview for Unreal Engine — Epic](https://dev.epicgames.com/documentation/unreal-engine/networking-overview-for-unreal-engine)
3. [Key roles in a game development team (Pingle Studio)](https://pinglestudio.com/blog/key-roles-in-a-game-development-team-2024-edition)
4. [Abilities in Lyra — Epic Developer Community](https://dev.epicgames.com/documentation/en-us/unreal-engine/abilities-in-lyra-in-unreal-engine)
5. [GAS Conceptual Overview — X157 Dev Notes](https://x157.github.io/UE5/GameplayAbilitySystem/) · [tranek/GASDocumentation](https://github.com/tranek/GASDocumentation)
6. [How we built our multi-agent research system — Anthropic Engineering](https://www.anthropic.com/engineering/multi-agent-research-system)
7. [Multi-agent coordination patterns — Claude by Anthropic](https://claude.com/blog/multi-agent-coordination-patterns) · [Orchestrator-Worker Pattern — AgentPatterns.ai](https://agentpatterns.ai/multi-agent/orchestrator-worker/)
8. [orchestrator_workers — anthropic-cookbook](https://github.com/anthropics/anthropic-cookbook/blob/main/patterns/agents/orchestrator_workers.ipynb)
