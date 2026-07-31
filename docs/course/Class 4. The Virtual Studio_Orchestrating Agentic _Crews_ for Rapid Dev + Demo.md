![][image1]![][image2]  
04 

CLASS 

THE VIRTUAL STUDIO:   
ORCHESTRATING   
AGENTIC "CREWS"   
FOR RAPID DEV 

MULTI-AGENT AI FOR GAME DEVELOPMENT   
JOSHUA BURDICK  
![][image3]![][image4]![][image5]![][image6]

Keep cameras on for engagement during the session 

HOUSEKEEPING  
Stay muted when not speaking to avoid interrupting the 

instructor   
Save your questions for the Q\&A section at the end   
Lower your hand after   
your question has been addressed 

![][image7]  
**00** 

**Welcome Class** 

**05** 

**Dynamic Content Generation: RAG Powered Pipelines for Game Content \+ Demo** 

**10** 

**Emergent**   
**Chronicles:**   
**Engineering Multi Agent Narrative Engines**   
**01** 

**Foundations of Agency: An**   
**Introduction to**   
**Multi-Agent AI in Gaming \+ Case Study** 

**06** 

**From Agent Output to Playable Game \+ Demo** 

**11** 

**The Chaos Crew: Adversarial AI for Game QA & Balance Testing \+ Demo**   
**02** 

**The GDD Anatomy: Theory & Strategy** 

**07** 

**Autonomous**   
**Agency:**   
**Engineering Goal Oriented Reasoning & Intent \+ Demo** 

**12** 

**The AI Production Pipeline: From** 

**Agent Outputs to Shipped Game**   
**03** 

**From GDD to**   
**Prototype: The Agent Workshop \+ Demo** 

**08** 

**The Level**   
**Architect: Agentic Layout & Logic \+ Demo** 

**13** 

**The Final Sprint: Refine, Optimize and Ship Your** 

**Project**   
**04** 

**The Virtual Studio: Orchestrating**   
**Agentic "Crews" for Rapid Dev \+ Demo** 

**09** 

**Generating**   
**Content:**   
**Maintaining the Human Touch \+ Workshop** 

**14** 

**The Architect's Exit: Scaling,** 

**Ethics, and Industry Integration** 

SYLLABUS OVERVIEW

![][image8]  
![][image9]![][image10]  
01 DEMO \#3: CREATING AGENTS   
USING CLAUDE 

02 CREWAI ARCHITECTURE 

03 MANAGER AGENTS &   
DELEGATION 

04 SHARED MEMORY POOLS 

05 FILE SYSTEM & ENGINE   
INTEGRATION 

06 07   
DOCUMENTING YOUR ARCHITECTURE 

ASSIGNMENT \#3: BUILD AN AGENT CREW  
![][image11]![][image12]  
PICKING UP FROM   
LAST SESSION 

In S03, you stress-tested your GDD with an agent review   
crew and got feedback from your peers. 

**Today: you build the crew.** 

A multi-agent crew is how you coordinate multiple agents   
working on the same game \- each one with a specific role,   
producing specific output, in a specific order.  
DEMO \#3:   
CREATING AGENTS USING CLAUDE 

Using a roguelike as the demo source \- watch how agents map to a real game project.![][image13]  
![][image14]![][image15]  
SETTING THE BASELINE: THE   
ROGUELIKE DEMO PROJECT 

The demo uses a roguelike as its source game \- so you can see   
exactly how agents map to a real project, not a sanitized workspace. 

1REAL GAME AS SOURCE   
The roguelike's actual files, structure, and design   
decisions are the input. No abstraction. 

2PROMPT-FIRST FOCUS   
All effort goes into shaping how each agent interacts   
with the codebase and game design. 

3OBSERVABLE BEHAVIOR   
With a concrete game as context, agent behavior is   
easier to study and replicate.

LIVE DEMO: 3-AGENT CREW   
FROM SCRATCH 

A live walkthrough demonstrating the creation of a 3-agent   
crew from the ground up using Claude. 

DEFINE DISTINCT ROLES 

Each agent must have a clearly scoped   
responsibility within the development 

environment.   
ESTABLISH CLEAR GOALS 

Goals must be   
unambiguous and   
measurable so agents can evaluate their own task completion. 

PROVIDE NECESSARY CONTEXT 

Each agent needs the right contextual grounding to perform its function effectively within the crew.  
PROMPT ENGINEERING FOR DEVELOPER AGENTS 

Crafting prompts for development agents requires strict formatting and unambiguous instructions. Establishing constraints early prevents catastrophic code generation failures. 

DEFINE THE INPUT FORMAT 

The agent must know exactly what data 

structure it will receive. Ambiguous inputs lead to unpredictable   
parsing behavior and unreliable outputs.   
SPECIFY THE   
OUTPUT SCHEMA 

The exact output   
schema the agent must return should be 

defined upfront \- field names, types, and nesting included.   
SET   
CONSTRAINTS   
EARLY 

Hard boundaries \- what the agent must never 

do \- prevent runaway   
generation and protect the project's file 

integrity.

CREW AI   
ARCHITECTURE

HIERARCHICAL VS.   
SEQUENTIAL TASK   
EXECUTION 

Understanding the flow of operations is critical to building an   
effective multi-agent system. The execution model   
determines how efficiently complex problems are solved. 

SEQUENTIAL EXECUTION 

Tasks move down a rigid line, each completing before   
the next begins. Predictable but inflexible \- bottlenecks   
cascade through the entire chain. 

HIERARCHICAL ARCHITECTURE 

A top-level agent dynamically delegates tasks based on 

the current context, enabling parallel resolution and   
adaptive problem-solving across the crew.

WHAT CREWAI HIDES 

Frameworks like CrewAI abstract away significant complexity to   
provide a smooth developer experience. Understanding what lies   
beneath helps you debug when things go wrong. 

1 CONTEXT WINDOW TRACKING 

The framework continuously monitors token consumption   
across all active agents to prevent overflow failures. 

2 RATE LIMITING 

Rate limiting behavior depends on your CrewAI version   
and configuration. Test your crew with a small batch   
before running at scale. 

3 TOOL OUTPUT PARSING 

The persistent loop of parsing tool outputs back into the   
LLM's reasoning engine is handled entirely under the   
hood.  
MANAGER AGENTS & DELEGATION

TASK DECOMPOSITION 

Complex game development objectives must be broken   
down into atomic, solvable tasks before any agent can act   
effectively. 

1 Analyze Core Objective 

The Manager Agent receives the high-level goal and 

evaluates its full scope and complexity. 

2 Split into Atomic Tasks 

The objective is decomposed into the smallest 

independently solvable units of work. 

3 Assign to Specialists 

Each task is delegated to the sub-agent whose defined   
capabilities best match the requirement.

DEPENDENCY GRAPHS AND   
THE KICK-OFF PATTERN 

Tasks rarely exist in isolation. The kick-off pattern establishes   
the sequence of execution, ensuring agents act only when   
their required inputs are ready. 

1 CONCEPT DESCRIPTION AGENT 

Finalizes the creative brief and outputs a   
structured description for downstream agents. 

2 DEPENDENCY RESOLVED 

The kick-off condition is met \- prerequisite output   
is confirmed valid and available. 

3 ASSET GENERATION AGENT 

Begins work only after the concept description is   
finalized, preventing wasted generation cycles.  
SHARED MEMORY POOLS

STRUCTURED MESSAGE   
PASSING 

Agents communicate through structured message passing   
under the hood, ensuring context is accurately transferred   
without polluting the global state. 

ACCURATE   
CONTEXT   
TRANSFER 

Structured messages enforce a schema that prevents context from being misread or silently dropped between agent handoffs.   
GLOBAL STATE PROTECTION 

By isolating message payloads, the shared memory pool avoids cross-contamination between independent agent threads. 

SIMULTANEOUS ACCESS 

Multiple agents can reference the same foundational knowledge or project state at the same time without conflict.  
PREVENTING CONTEXT COLLAPSE 

As agents converse and iterate, memory pools can quickly overflow the LLM's context window. Long generation sessions demand proactive memory management strategies. 

SUMMARIZE PAST ACTIONS 

Periodically compress resolved conversation turns into concise summaries to free up context capacity without losing key decisions.   
ARCHIVE RESOLVED TASKS 

Move completed tasks out of the active context window into long term storage, retrievable only when explicitly needed.   
MAINTAIN TEAM   
COHERENCE 

A well-managed memory pool keeps the agent team responsive and coherent even during 

extended, complex generation sessions.

FILE SYSTEM &   
ENGINE   
INTEGRATION

READING SCENE FILES   
AND MODIFYING CONFIGS 

Agents are granted the capability to read existing scene files and   
modify configuration documents directly \- transforming them from   
text generators into active participants within the project's file   
structure. 

READ SCENE FILES 

Agents parse and analyze existing scene data to understand the current state of the game world before making changes.   
MODIFY CONFIG DOCUMENTS 

Configuration files are updated directly by agents, enabling real-time tuning of game parameters without manual intervention. 

ACTIVE FILE PARTICIPANTS 

This capability elevates agents beyond chatbots \- they become genuine collaborators within the project's file architecture.

CREATING THE BRIDGE 

Connecting the virtual studio to the game engine requires   
robust error handling at every integration point. 

DETECT   
MALFORMED OUTPUT 

If an agent writes malformed data to a configuration file, the integration layer must immediately catch the syntax error before the engine attempts to compile.   
PROMPT SELF   
CORRECTION 

The bridge feeds the detected error back to the responsible agent, 

prompting it to re-evaluate and produce a corrected output without human intervention. 

VALIDATE BEFORE COMPILE 

Only validated, well-formed data is passed to the engine \- the bridge acts as the final gate between agent output and engine state.  
DOCUMENTING   
YOUR   
ARCHITECTURE

THE NECESSITY OF VISUAL   
DOCUMENTATION 

As multi-agent systems grow, visual documentation   
becomes essential. 

FOR THE   
SYSTEMS   
ARCHITECT 

Diagrams quickly reveal bottlenecks before they cause failures.   
IDENTIFYING CIRCULAR   
DEPENDENCIES 

Visual maps expose loops that can stall agent workflows. 

FOR EXTERNAL STAKEHOLDERS 

Clear diagrams make system behavior easy to understand.

MERMAID DIAGRAMS FOR AGENT   
SYSTEMS 

Mermaid diagrams provide a code-based approach to visualizing agent   
architectures, generating real-time visual representations that evolve   
alongside the codebase. 

DEFINED IN MARKDOWN 

Structures are written as plain text in markdown, making   
diagrams version-controllable and diff-friendly alongside source   
code. 

AGENT HIERARCHY VISUALIZATION 

The full manager-to-sub-agent delegation tree is rendered   
automatically from the diagram definition, always reflecting the   
current architecture. 

MEMORY & DEPENDENCY MAPS 

Shared memory access patterns and task dependency chains   
are visualized in the same diagram, giving a complete picture of   
the agent system.  
ASSIGNMENT \#3: BUILD AN AGENT CREW

ASSIGNMENT \#3: BUILD AN AGENT CREW 

This is a mandatory assignment. Completion is required   
toward your course certificate. Due: Before S06 \- 28 July   
2026 11:59 ET 

Build a system with 3+ agents that work together to produce   
game-ready output. The crew should target your capstone game.   
Submissions not connected to a specific game receive no credit   
on Game Connection. 

DELIVERABLES 

CREW   
CODE 

Your CrewAI or raw orchestration code. 3+ agents must coordinate and produce output without crashing.   
MERMAID DIAGRAM 

A diagram showing agent roles, 

connections, and data flow.   
README 

What does this crew produce, and what game is it for? Name the game.

ASSIGNMENT \#3: GRADING   
RUBRIC 

Criterion Description Points 

| The crew runs and produces output. 3+ agents coordinate without  crashing. |
| ----- |
| The crew's output is designed for the student's capstone game. The  ReadMe names the game and explains what the crew produces for it. |
| Each agent has a specific role with defined input and output. No agent could be removed without breaking the pipeline. |
| Mermaid diagram accurately shows agent roles, connections, and data flow. |
| Describes the crew, its purpose, and its connection to the student's game. |

Working Crew / 3.0 

Game Connection / 3.0 

Role Clarity / 2.0 

Architecture Diagram   
/ 1.0 

ReadMe / 1.0 Total / 10  
Q\&A 

WE ENCOURAGE YOU TO ASK QUESTIONS   
ABOUT CREWAI,MEMORY MANAGEMENT,OR   
ENGINE INTEGRATION.KEEP YOUR   
QUESTIONS SHORT AND CONCISE.

DID YOU ENJOY   
THE CLASS? 

PLEASE TAKE A MOMENT TO COMPLETE A SHORT   
SURVEY AT THE END OF THIS SESSION. YOUR FEEDBACK   
HELPS US IMPROVE YOUR LEARNING EXPERIENCE. NEXT   
UP: DYNAMIC CONTENT GENERATION PIPELINES.
