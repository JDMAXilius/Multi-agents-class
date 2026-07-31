# Class 7. Autonomous Agency: Engineering Goal-Oriented Reasoning & Intent + Demo

Multi-Agent AI for Game Development · Joshua Burdick · ELVTR

Verbatim slide-by-slide transcript of the deck (31 slides).

---

## Slide 1 — Title

**07 CLASS**

**AUTONOMOUS AGENCY: ENGINEERING GOAL-ORIENTED REASONING & INTENT + DEMO**

MULTI-AGENT AI FOR GAME DEVELOPMENT
JOSHUA BURDICK

---

## Slide 2 — Housekeeping

- Keep cameras on for engagement during the session
- Stay muted when not speaking to avoid interrupting the instructor
- Save your questions for the Q&A section at the end
- Lower your hand after your question has been addressed

---

## Slide 3 — Syllabus Overview

| # | Class |
|---|---|
| 00 | Welcome Class |
| 01 | Foundations of Agency: An Introduction to Multi-Agent AI in Gaming + Case Study |
| 02 | The GDD Anatomy: Theory & Strategy |
| 03 | From GDD to Prototype: The Agent Workshop + Demo |
| 04 | The Virtual Studio: Orchestrating Agentic "Crews" for Rapid Dev + Demo |
| 05 | Dynamic Content Generation: RAG-Powered Pipelines for Game Content + Demo |
| 06 | From Agent Output to Playable Game + Demo |
| **07** | **Autonomous Agency: Engineering Goal-Oriented Reasoning & Intent + Demo** |
| 08 | The Level Architect: Agentic Layout & Logic + Demo |
| 09 | Generating Content: Maintaining the Human Touch + Workshop |
| 10 | Emergent Chronicles: Engineering Multi-Agent Narrative Engines |
| 11 | The Chaos Crew: Adversarial AI for Game QA & Balance Testing + Demo |
| 12 | The AI Production Pipeline: From Agent Outputs to Shipped Game |
| 13 | The Final Sprint: Refine, Optimize and Ship Your Project |
| 14 | The Architect's Exit: Scaling, Ethics, and Industry Integration |

---

## Slide 4 — Class Agenda

01. GOAL-DIRECTED PLANNING
02. UTILITY SCORING
03. CODEBASE PERCEPTION
04. PERSISTENT STATE & PRIORITY SCORING
05. FRAMEWORKS VS. RAW ORCHESTRATION
06. DEMO #6: BLACKBOARDING TO MAKE AGENCY VISIBLE
07. ASSIGNMENT #5: GOAL-ORIENTED CODING AGENT

---

## Slide 5 — Picking Up From Last Session

In S06, you connected agent output to your game engine.
Your pipeline now generates content AND gets it into the game.

**TODAY'S QUESTION:**

**WHAT IF THE AGENT COULD LOOK AT YOUR CODEBASE,
FIGURE OUT WHAT'S MISSING, AND WRITE IT?**

We're going from agents that execute tasks
to agents that decide what tasks to execute.
That's the shift from task-runner to goal-oriented agent.

---

## Slide 6 — Goal

Build goal-oriented dev agents that analyze your codebase, determine what needs to be built, and take action. Agents that write code, create files, and structure your project autonomously.

**SESSION TOPICS**

**FROM TASK RUNNER TO GOAL-ORIENTED AGENT**
Shift from executing isolated tasks to pursuing a clear development objective.

**CODEBASE PERCEPTION**
Understand files, structure, and context so the agent can reason about the project.

**UTILITY SCORING FOR DEVELOPMENT PRIORITIES**
Rank next actions by impact, urgency, and expected value.

**TAKING ACTION**
Have the agent write code, create files, and move work forward autonomously.

**PERSISTENT STATE VIA MARKDOWN**
Use markdown as a durable memory layer for progress and decisions.

**FRAMEWORKS VS. RAW ORCHESTRATION: THE PIVOT**
Know when to use a framework and when direct orchestration gives you more control.

---

## Slide 7 — Section

**FROM TASK RUNNER TO GOAL-ORIENTED AGENT**

---

## Slide 8 — The Shift

**YOUR AGENTS SO FAR**
Your agents follow instructions - "generate this dialogue," "create this content." They execute tasks you define.

**AT S07, YOU KNOW WHAT'S MISSING**
You have a running game. You have a GDD. You know what needs to be built. The question isn't what's missing - you already know that.

**THE QUESTION IS WHETHER YOU CAN CLOSE THOSE GAPS**
The goal-oriented agent does the mechanical build work while you stay focused on the game that needs to exist. You decide what to build. The agent executes.

**THIS SESSION**
From executing tasks to pursuing goals. The agent reads your GDD, scans your codebase, scores what to build next - and writes it.

---

## Slide 9 — Codebase Perception

The agent reads your project the way a new developer would.

**01 DIRECTORY STRUCTURE**
Scans folder layout, file names, and project organization.

**02 EXISTING FILES & IMPORTS**
Reads what's wired up vs what's stubbed out.

**03 DIFF AGAINST GDD**
Compares the feature list to what's actually been built.

**04 IDENTIFY GAPS**
Determines what's missing and what needs to be built next.

*What the agent sees determines what it decides to build next.*

---

## Slide 10 — Section

**UTILITY SCORING FOR DEVELOPMENT PRIORITIES**

---

## Slide 11 — The Priority Problem

Multiple things need building. The inventory system, the save/load logic, the enemy spawner, a broken import in the main scene. How does the agent decide what to work on first?

**Dependencies Matter**

You can't build the shop UI before the inventory system exists. The agent must understand dependency order, blockers, and what the project actually needs right now.

**The Scoring System**

Students build a utility scoring system that accounts for:

- Dependency order (what must exist first)
- Blockers (what's preventing other work)
- Project priority (what the GDD marks as critical)
- Current state (what's partially built vs untouched)

---

## Slide 12 — Example — Dungeon Crawler

```
Your GDD lists:
  inventory system, enemy AI, save/load,
  level generation, UI polish

Your codebase has:
  player movement, basic combat, one hardcoded level

The agent scores:
  1. Inventory system    <- blocking shop UI and loot drops
  2. Enemy AI            <- blocking core game feel
  3. Save/load           <- blocking progression
  4. Level generation    <- not blocking anything yet
  5. UI polish           <- last, depends on everything above
```

*The order isn't random. It's derived from your GDD's dependency graph — using your capstone game as the source.*

---

## Slide 13 — Taking Action - What It Looks Like

**BEFORE** — empty starter repo scaffold

```
/my-game
  /src
    game.py       <- exists (entry point)
    player.py     <- exists
    inventory.py  <- MISSING (GDD requires it)
    dialogue.py   <- MISSING (GDD requires it)
  /data
    items.json    <- MISSING
```

**AFTER** — goal-oriented agent has run

```
/my-game
  /src
    game.py       <- unchanged
    player.py     <- unchanged
    inventory.py  <- WRITTEN by agent (matches player.py patterns)
    dialogue.py   <- WRITTEN by agent (loads from data/items.json)
  /data
    items.json    <- GENERATED by agent from GDD spec
```

The agent scanned the codebase, found the gaps, scored priorities, and wrote working code. No manual file creation. No copy-paste.

**YOUR STEP AFTER THE AGENT RUNS:**

Read every generated file before it goes into your game. "Matches player.py patterns" is not the same as "matches your game's design intent." The agent writes the mechanical code. You decide if it's right.

---

## Slide 14 — Section

**PERSISTENT STATE VIA MARKDOWN**

---

## Slide 15 — How Agents Track Progress

**WHAT'S BEEN BUILT**
The agent logs every file created, every feature implemented, every class scaffolded.

**WHAT FAILED**
Failed attempts, compile errors, and dead ends are recorded so the agent doesn't repeat them.

**WHAT'S LEFT**
Remaining features from the GDD that haven't been started or completed yet.

**DECISIONS & REASONING**
Why the agent chose one approach over another - readable by the student, editable, feedable back in.

No databases. The student can read it, edit it, feed it back into the next session. The agent picks up where it left off.

---

## Slide 16 — Markdown as Agent Memory

Agents track their own progress in simple markdown files. No databases, no complex infrastructure - just readable, editable text files.

**BUILT**
Completed files, classes, and features with timestamps.

**DECISIONS**
Why the agent chose specific approaches, patterns used, and architecture notes.

**NEXT**
Remaining GDD features, priority order, and known blockers.

The student can read it, edit it, and feed it back into the next session. The agent picks up where it left off.

---

## Slide 17 — Section

**FRAMEWORKS VS. RAW ORCHESTRATION: THE PIVOT**

---

## Slide 18 — The Centerpiece of This Session

CrewAI is great for learning multi-agent patterns. But when your agent needs to read your specific file structure, run your build tool, check for compile errors, and decide what to do next based on the results - you need raw orchestration.

**WHAT RAW ORCHESTRATION GIVES YOU**
Manual API calls, custom parsing, explicit control over every step. No abstraction layer between you and the model. You see every decision the agent makes about your game before it writes a file.

**THE LIVE COMPARISON**
Same coding task in CrewAI vs a raw Python loop calling Claude. Students see exactly what they gain when they drop the framework.

**WHICH APPROACH IS RIGHT FOR YOU?**
It's not about which is more capable. It's about which gives you control when it matters. At S07, you are closing GDD gaps in a running game. Every agent decision has consequences for your codebase. For closing your GDD gaps before the final sprint, raw orchestration gives you more visibility - but use whatever approach gets your game further along.

---

## Slide 19 — CrewAI vs. Raw Orchestration

**CREWAI**
Great for learning multi-agent patterns. High-level abstractions, quick setup, built-in agent roles. Best for structured, predictable workflows.

**VS**

**RAW ORCHESTRATION**
Manual API calls, custom parsing, explicit control. Read your file structure, run your build tool, check compile errors. Best when you need full control over every decision.

---

## Slide 20 — Framework vs. Raw Orchestration

**FRAMEWORK (CREWAI)**
- Handles routing, retries, context passing for you
- Great for complex multi-agent crews
- Less visibility into what happens between steps

**RAW (DIRECT API CALLS)**
- You control every step
- You see every decision the agent makes
- More work, but full transparency

**FOR THIS ASSIGNMENT: RAW**

You're writing code in your own codebase. You need to see every decision the agent makes about your game before it writes a file.

---

## Slide 21 — Section

**DEMO #6: THE CODE ARCHITECT**

---

## Slide 22 — What the Demo Shows

**FEATURE LIST FROM GDD**
Agent receives a feature list and scans an existing game project.

**GAP ANALYSIS**
Identifies what's built and what's missing by diffing the GDD against the codebase.

**CODE GENERATION**
Starts writing code for the highest-priority missing feature.

**LIVE BLACKBOARD**
Shows reasoning live - what it sees, how it scored priorities, what code it's generating and where.

**MARKDOWN STATE FILE**
The state file updates in real time as the agent works.

---

## Slide 23 — The Code Architect in Action

A goal-oriented agent receives a feature list from a GDD, scans an existing game project, and starts building.

**01 RECEIVE**
Takes in the GDD feature list and the existing project directory.

**02 SCAN & DIFF**
Reads the codebase, identifies what's built vs what's missing.

**03 SCORE & PRIORITIZE**
Ranks missing features by dependency order and project needs.

**04 BUILD**
Writes code for the highest-priority missing feature, updates the markdown state file.

Footer note: A blackboard shows its reasoning live throughout the entire process.

---

## Slide 24 — The Blackboard Is Not Optional

When an agent writes code in your project, you need to see every decision it made. The blackboard makes that visible.

**WHAT IT SCORED**
Every missing feature, ranked. The utility score for each one and why it was prioritized in that order.

**WHAT IT ISSUED**
The exact prompts sent to the model. What context was passed. What instructions shaped the output.

**WHAT IT GENERATED**
The code it wrote, the file it created, the decision it just made — logged before it touches your codebase.

*Without the blackboard, you cannot tell whether the agent did what you intended. This is not a debugging tool. This is how you stay in control of your own codebase.*

---

## Slide 25 — Exposing Agent Reasoning

The blackboard isn't just for debugging. It's the mechanism by which you stay in control of an agent that is writing code in your project. Without it, you're flying blind.

**CODEBASE STATE**
What the agent read from the project - directory structure, existing files, stubbed vs implemented features.

**PRIORITY SCORES**
How each missing feature was scored - dependency order, blockers, GDD priority, current state. The reasoning behind the order.

**LIVE OUTPUT**
The code being written, the file being created, the markdown state file updating in real time.

Your ReadMe should explain what the agent decided and why. If you can't explain the agent's decisions, you weren't in control of them.

---

## Slide 26 — Section

**ASSIGNMENT #5: GOAL-ORIENTED CODING AGENT**

---

## Slide 27 — Assignment #5 - What You're Starting With

A starter scaffold is provided in the course resources: [LINK]

**IT CONTAINS:**
- A minimal LLM call loop (~30 lines)
- A file-system scanner stub (list_files, read_file helpers)
- A GDD parser stub (extract_required_features from Markdown)
- An empty priority_score() function for you to implement
- A simple print-based log (what the agent scored and why)

**YOU BUILD:**
- The priority scoring logic
- The gap-detection pass (compare scanned files vs. GDD features)
- The code-writing step (agent writes a missing file)
- The iteration loop (re-scan after each write, stop when done)

*"The starter is deliberately minimal. The goal is for you to understand the reasoning layer — how the agent decides what to build and in what order."*

---

## Slide 28 — Assignment #5: Goal-Oriented Coding Agent

**DUE DATE: 13 AUGUST 11:59 PM ET**

| | |
|---|---|
| **DELIVERABLES** | → Your agent's source code — the script that reads your GDD, scans your codebase, and generates code. Use whatever tools or approach works for you. |
| **WORKING FEATURE** | → One working feature in your game that the agent generated. It must run. Submissions not connected to your capstone game receive no credit. |
| **README** | → A short ReadMe explaining: what feature did the agent build, why did it pick that one, and what did you change before accepting it into your game? |
| **TIME ESTIMATE** | 6–10 hours |
| **DUE** | Before S09 (6 August 11:59 ET) |

> Note: the slide's header says 13 August; the DUE row says 6 August. Both appear on the slide as printed.

---

## Slide 29 — Assignment #5: Goal-Oriented Coding Agent — Rubric

| CRITERION | DESCRIPTION | POINTS |
|---|---|---|
| Working Feature | The agent-generated feature runs in the student's capstone game. Grader can pull the repo, run the game, and see it working. Feature must come from the GDD, not be invented for the assignment. | / 4.0 |
| Agent Code | The agent reads the student's GDD, scans their codebase, and generates code. Logic is readable — a reviewer can follow what the agent does. | / 3.0 |
| Judgment & Review | ReadMe explains: what the agent built, why it picked that feature, and what the student changed before accepting it. Shows the student directed the agent, not just ran it. | / 2.0 |
| Code Quality | Generated code follows the project's existing patterns. No obvious bugs, no hardcoded test values left in. | / 1.0 |
| **Total** | | **/ 10** |

*Submissions not connected to the student's capstone game receive no credit on Working Feature or Agent Code.*

---

## Slide 30 — Q&A

**Q&A**

---

## Slide 31 — Did You Enjoy the Class?

**DID YOU ENJOY THE CLASS?**

PLEASE TAKE A MOMENT TO COMPLETE A SHORT SURVEY AT THE END OF THIS SESSION. YOUR FEEDBACK WILL HELP US IMPROVE YOUR LEARNING EXPERIENCE.
