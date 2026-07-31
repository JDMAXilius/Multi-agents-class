# Class 07 — Autonomous Agency: Engineering Goal-Oriented Reasoning & Intent (+ Demo)

**Course:** Multi-Agent AI for Game Development (ELVTR) · **Instructor:** Joshua Burdick
**Source:** full-resolution slide-panel capture of the deck (3024×19497), transcribed 31 Jul 2026
**Status:** ✅ complete — all 31 slides transcribed from the deck itself. Nothing inferred.

![Class 07 slide overview](images/class-07-slide-overview.png)

---

## The one-line version

> **"What if the agent could look at your codebase, figure out what's missing, and write it?"**

The session moves you **from agents that execute tasks to agents that decide what tasks to
execute** — and it insists you keep control of that by making the agent's reasoning visible
(the blackboard) and its memory durable (a Markdown state file).

---

## Class agenda (slide 4)

1. Goal-directed planning
2. Utility scoring
3. Codebase perception
4. Persistent state & priority scoring
5. Frameworks vs. raw orchestration
6. **Demo #6: Blackboarding to make agency visible**
7. Assignment #5: Goal-oriented coding agent

**Goal (slide 6):** *"Build goal-oriented dev agents that analyze your codebase, determine
what needs to be built, and take action. Agents that write code, create files, and structure
your project autonomously."*

Session topics as stated on the goal slide:

| Topic | One-liner |
|---|---|
| From task runner to goal-oriented agent | Shift from executing isolated tasks to pursuing a clear development objective. |
| Codebase perception | Understand files, structure, and context so the agent can reason about the project. |
| Utility scoring for development priorities | Rank next actions by impact, urgency, and expected value. |
| Taking action | Have the agent write code, create files, and move work forward autonomously. |
| Persistent state via Markdown | Use markdown as a durable memory layer for progress and decisions. |
| Frameworks vs. raw orchestration: the pivot | Know when to use a framework and when direct orchestration gives you more control. |

---

## 1 · From task runner to goal-oriented agent

### The Shift (slide 8)

- **Your agents so far** — follow instructions ("generate this dialogue," "create this
  content"). They execute tasks you define.
- **At S07, you know what's missing** — "You have a running game. You have a GDD. You know
  what needs to be built. The question isn't what's missing — you already know that."
- **The question is whether you can close those gaps** — "The goal-oriented agent does the
  mechanical build work while you stay focused on the game that needs to exist. You decide
  what to build. The agent executes."
- **This session** — "From executing tasks to pursuing goals. The agent reads your GDD, scans
  your codebase, scores what to build next — and writes it."

### Codebase Perception (slide 9)

*"The agent reads your project the way a new developer would."*

| # | Capability | What it does |
|---|---|---|
| 01 | Directory structure | Scans folder layout, file names, and project organization. |
| 02 | Existing files & imports | Reads what's wired up vs what's stubbed out. |
| 03 | Diff against GDD | Compares the feature list to what's actually been built. |
| 04 | Identify gaps | Determines what's missing and what needs to be built next. |

> **What the agent sees determines what it decides to build next.**

---

## 2 · Utility scoring for development priorities

### The Priority Problem (slide 11)

"Multiple things need building. The inventory system, the save/load logic, the enemy spawner,
a broken import in the main scene. How does the agent decide what to work on first?"

**Dependencies matter** — "You can't build the shop UI before the inventory system exists. The
agent must understand dependency order, blockers, and what the project actually needs right now."

**The scoring system** — students build a utility scoring system that accounts for:

- Dependency order (what must exist first)
- Blockers (what's preventing other work)
- Project priority (what the GDD marks as critical)
- Current state (what's partially built vs untouched)

### Example — Dungeon Crawler (slide 12)

```
Your GDD lists:   inventory system, enemy AI, save/load, level generation, UI polish
Your codebase has: player movement, basic combat, one hardcoded level

The agent scores:
  1. Inventory system   <- blocking shop UI and loot drops
  2. Enemy AI           <- blocking core game feel
  3. Save/load          <- blocking progression
  4. Level generation   <- not blocking anything yet
  5. UI polish          <- last, depends on everything above
```

> *The order isn't random. It's derived from your GDD's dependency graph — using your capstone
> game as the source.*

### Taking action — what it looks like (slide 13)

```
BEFORE (empty starter repo scaffold)        AFTER (goal-oriented agent has run)
/my-game                                    /my-game
  /src                                        /src
    game.py      <- exists (entry point)        game.py      <- unchanged
    player.py    <- exists                      player.py    <- unchanged
    inventory.py <- MISSING (GDD requires it)   inventory.py <- WRITTEN by agent (matches player.py patterns)
    dialogue.py  <- MISSING (GDD requires it)   dialogue.py  <- WRITTEN by agent (loads from data/items.json)
  /data                                       /data
    items.json   <- MISSING                     items.json   <- GENERATED by agent from GDD spec
```

"The agent scanned the codebase, found the gaps, scored priorities, and wrote working code.
No manual file creation. No copy-paste."

> **Your step after the agent runs:** "Read every generated file before it goes into your
> game. *'Matches player.py patterns'* is not the same as *'matches your game's design
> intent.'* The agent writes the mechanical code. You decide if it's right."

---

## 3 · Persistent state via Markdown

### How agents track progress (slide 15)

| | |
|---|---|
| **What's been built** — the agent logs every file created, every feature implemented, every class scaffolded. | **What failed** — failed attempts, compile errors, and dead ends are recorded so the agent doesn't repeat them. |
| **What's left** — remaining features from the GDD that haven't been started or completed yet. | **Decisions & reasoning** — why the agent chose one approach over another; readable by the student, editable, feedable back in. |

*No databases. The student can read it, edit it, feed it back into the next session. The agent
picks up where it left off.*

### Markdown as agent memory (slide 16)

Three sections, explicitly: **BUILT** (completed files, classes, features with timestamps) ·
**DECISIONS** (why the agent chose specific approaches, patterns used, architecture notes) ·
**NEXT** (remaining GDD features, priority order, known blockers).

---

## 4 · Frameworks vs. raw orchestration — *the pivot*

### The centerpiece of this session (slide 18)

"CrewAI is great for learning multi-agent patterns. But when your agent needs to read your
specific file structure, run your build tool, check for compile errors, and decide what to do
next based on the results — **you need raw orchestration**."

- **What raw orchestration gives you** — "Manual API calls, custom parsing, explicit control
  over every step. No abstraction layer between you and the model. You see every decision the
  agent makes about your game before it writes a file."
- **The live comparison** — "Same coding task in CrewAI vs a raw Python loop calling Claude.
  Students see exactly what they gain when they drop the framework."
- **Which approach is right for you?** — "It's not about which is more capable. It's about
  which gives you control when it matters. At S07, you are closing GDD gaps in a running game.
  Every agent decision has consequences for your codebase. For closing your GDD gaps before the
  final sprint, raw orchestration gives you more visibility — **but use whatever approach gets
  your game further along.**"

### The comparison (slides 19–20)

| Framework (CrewAI) | Raw (direct API calls) |
|---|---|
| Handles routing, retries, context passing for you | You control every step |
| Great for complex multi-agent crews | You see every decision the agent makes |
| Less visibility into what happens between steps | More work, but full transparency |
| Best for structured, predictable workflows | Best when you need full control over every decision |

> **For this assignment: RAW.** "You're writing code in your own codebase. You need to see
> every decision the agent makes about your game before it writes a file."

---

## 5 · Demo #6 — The Code Architect

### What the demo shows (slide 22)

- **Feature list from GDD** — agent receives a feature list and scans an existing game project.
- **Gap analysis** — identifies what's built and what's missing by diffing the GDD against the codebase.
- **Code generation** — starts writing code for the highest-priority missing feature.
- **Live blackboard** — shows reasoning live: what it sees, how it scored priorities, what code it's generating and where.
- **Markdown state file** — the state file updates in real time as the agent works.

### The Code Architect in action (slide 23)

| # | Step | |
|---|---|---|
| 01 | **Receive** | Takes in the GDD feature list and the existing project directory. |
| 02 | **Scan & diff** | Reads the codebase, identifies what's built vs what's missing. |
| 03 | **Score & prioritize** | Ranks missing features by dependency order and project needs. |
| 04 | **Build** | Writes code for the highest-priority missing feature, updates the markdown state file. |

*A blackboard shows its reasoning live throughout the entire process.*

### The blackboard is not optional (slide 24)

"When an agent writes code in your project, you need to see every decision it made."

- **What it scored** — every missing feature, ranked. The utility score for each one and why it was prioritized in that order.
- **What it issued** — the exact prompts sent to the model. What context was passed. What instructions shaped the output.
- **What it generated** — the code it wrote, the file it created, the decision it just made — **logged before it touches your codebase**.

> *Without the blackboard, you cannot tell whether the agent did what you intended. This is not
> a debugging tool. This is how you stay in control of your own codebase.*

### Exposing agent reasoning (slide 25)

Three panels the blackboard must surface — **codebase state** (directory structure, existing
files, stubbed vs implemented) · **priority scores** (dependency order, blockers, GDD priority,
current state, and the reasoning behind the order) · **live output** (code being written, file
being created, markdown state file updating in real time).

> "Your ReadMe should explain what the agent decided and why. **If you can't explain the
> agent's decisions, you weren't in control of them.**"

---

## Assignment #5 — Goal-Oriented Coding Agent

### What you're starting with (slide 27)

A starter scaffold is provided in the course resources.

| It contains | You build |
|---|---|
| A minimal LLM call loop (~30 lines) | The priority scoring logic |
| A file-system scanner stub (`list_files`, `read_file` helpers) | The gap-detection pass (compare scanned files vs. GDD features) |
| A GDD parser stub (`extract_required_features` from Markdown) | The code-writing step (agent writes a missing file) |
| An empty `priority_score()` function for you to implement | The iteration loop (re-scan after each write, stop when done) |
| A simple print-based log (what the agent scored and why) | |

> *"The starter is deliberately minimal. The goal is for you to understand the reasoning layer
> — how the agent decides what to build and in what order."*

### Deliverables (slide 28)

| | |
|---|---|
| **Deliverables** | Your agent's source code — the script that reads your GDD, scans your codebase, and generates code. Use whatever tools or approach works for you. |
| **Working feature** | One working feature in your game that the agent generated. **It must run.** Submissions not connected to your capstone game receive no credit. |
| **ReadMe** | A short ReadMe explaining: what feature did the agent build, why did it pick that one, and what did you change before accepting it into your game? |
| **Time estimate** | 6–10 hours |

⚠️ **Due date is contradictory on the slide itself.** The header reads **"DUE DATE: 13 AUGUST
11:59 PM ET"**; the table's DUE row reads **"Before S09 (6 August 11:59 ET)"**. Confirm with
the instructor — assume the earlier one (6 Aug) until corrected.

### Rubric (slide 29) — 10 points

| Criterion | Description | Points |
|---|---|---|
| **Working Feature** | The agent-generated feature runs in the student's capstone game. Grader can pull the repo, run the game, and see it working. Feature must come from the GDD, not be invented for the assignment. | 4.0 |
| **Agent Code** | The agent reads the student's GDD, scans their codebase, and generates code. Logic is readable — a reviewer can follow what the agent does. | 3.0 |
| **Judgment & Review** | ReadMe explains: what the agent built, why it picked that feature, and what the student changed before accepting it. Shows the student directed the agent, not just ran it. | 2.0 |
| **Code Quality** | Generated code follows the project's existing patterns. No obvious bugs, no hardcoded test values left in. | 1.0 |
| **Total** | | **/10** |

> *Submissions not connected to the student's capstone game receive no credit on Working
> Feature or Agent Code.*

---

## Syllabus position (slide 3)

00 Welcome · 01 Foundations of Agency · 02 The GDD Anatomy · 03 From GDD to Prototype ·
04 The Virtual Studio · 05 Dynamic Content Generation (RAG) · 06 From Agent Output to Playable
Game · **07 Autonomous Agency ←** · 08 The Level Architect · 09 Generating Content: Maintaining
the Human Touch · 10 Emergent Chronicles · 11 The Chaos Crew · 12 The AI Production Pipeline ·
13 The Final Sprint · 14 The Architect's Exit

Slides 30–31 are Q&A and the course-feedback prompt. Slide 2 is standard housekeeping boilerplate.

---

## What this means for BREACHPOINT

The rubric is graded against **a game a grader can pull and run**. That is the binding
constraint, and it collides with the repo's current state:

1. **The game repo doesn't exist yet** (see [root README](../../README.md) — M0 complete, next
   action is "create the game repo"). 4 of 10 points require a runnable capstone game with the
   agent-generated feature in it. Nothing else in the rubric substitutes for that.
2. **UE 5.8 / C++ raises the bar for "it must run."** A grader pulling a UE C++ repo has to
   compile it. A generated `.cpp/.h` pair that compiles and is visible in-game is the deliverable
   — pick the smallest such feature, not the most impressive one.
3. **The crew already satisfies most of the non-runtime rubric.** Utility scoring ≈ the ticket
   board's dependency order; Markdown-as-memory ≈ `crew/docs/` + the rulings ledger; blackboard
   ≈ the reviewers' logged reasoning (BP13's Log section is exactly the artifact slide 24 asks
   for). The gap is the **raw-orchestration script that reads the GDD, diffs the codebase, scores,
   and writes a file** — the crew currently routes through Claude Code, not a student-authored loop.
4. **Feature must come from the GDD.** Sourcing it from
   [`BREACHPOINT-GDD-VERTICAL-SLICE.md`](../../breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md) and
   scoring against [`BREACHPOINT-ARCHITECTURE.md`](../../breachpoint/BREACHPOINT-ARCHITECTURE.md)'s
   44 class-units gives a real dependency graph — a stronger input than the dungeon-crawler example.

**Note:** the root README says coursework is complete through Assignment #3. Assignment #5
here is unstarted, and #4 isn't accounted for anywhere in the repo.
