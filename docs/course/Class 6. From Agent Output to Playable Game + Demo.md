![][image1]![][image2]  
06 

CLASS 

FROM AGENT   
OUTPUT TO   
PLAYABLE GAME 

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
PICKING UP FROM LAST   
SESSION 

In S05, your agents generated content for your game. 

**What is still sitting in a text file that your players**   
**cannot touch yet?** 

This session closes that gap. 

BEFORE WE GO FURTHER 

Write down one piece of content your agent generated in S05 that is not yet live in your running game. That specific thing is what we are fixing today.

![][image9]  
OUTPUT FORMATS![][image10]  
![][image11]![][image12]  
WHAT ENGINES CONSUME 

Your agent produces output. Your engine needs to consume it. The   
format question is simple: what does your engine read? Start there. 

JSON \-   
SCRIPTABLEOBJECTS 

AI dialogue outputs   
structured as JSON are mapped directly to Unity ScriptableObjects for use in runtime game logic.   
CSV \- DATATABLES 

Tabular AI outputs formatted as CSV files are converted into engine-readable   
DataTables for rapid data driven gameplay. 

These format specs are reference material — you don't need to memorize them. When you need the integration code, describe what you need to your LLM and have it write it.  
ENGINE-SPECIFIC SPECIFICATIONS Every game engine handles data ingestion differently. Adapting agent outputs requires strict adherence to per-engine specifications. 

UNITY 

Expects JSON mapped to ScriptableObjects or C\# serializable classes. Strong typing is enforced at compile time.   
UNREAL ENGINE 

Consumes DataTables from CSV inputs. Blueprint accessible structs must match the column schema exactly.   
BROWSER-BASED 

Frameworks like Phaser or Three.js consume JSON directly via fetch calls, requiring UTF-8 clean outputs with no BOM characters. 

ENGINE   
SPECIFIC OUTPUT 

Customized data for game   
engines.   
UNITY JSON 

Unity platform.UNREAL CSV   
Formatted for the 

Data structured   
for Unreal   
Engine. 

BROWSER JSON 

Standard format for web   
browsers. 

Matching agent output structure to engine-specific ingestion requirements is non-negotiable for a functioning automated pipeline.![][image13]  
FORCING STRUCTURED OUTPUTS 

Agents naturally default to conversational text. Prompt engineering at this stage must aggressively constrain the AI to output only valid code or data structures. 

1 

THE PROBLEM 

LLMs default to natural language prose. Conversational filler text will break any data parser downstream and crash the integration pipeline.   
2 

THE SOLUTION 

Use explicit system-level instructions in your prompt: "Respond ONLY with valid JSON. No explanations. No markdown. No preamble." Constrain the output schema completely.   
3 

VALIDATION LAYER 

Always follow structured output prompts with a downstream validator that checks schema compliance before the data is passed to the engine integration script. 

Never assume an LLM will produce clean, parseable output without explicit constraints and a validation step.![][image14]  
IMPORT AUTOMATION & INTEGRATION CHECKPOINT

![][image15]  
WHAT HAPPENS WHEN YOU SKIP THE REVIEW STEP 

Your agent generates 20 NPC descriptions for your game. You load them all directly into the engine. No review. 

LORE CONTRADICTION 

Three of them contradict your GDD's faction lore. 

**A player finds these in the first 10 minutes.** THE LESSON:   
WRONG CHARACTER 

One of them uses a character name from a different game.   
MISSING LOCATION 

One of them describes a location that doesn't exist in your world. 

The Review step in your pipeline is not optional. It is not a rubber stamp. It is the moment where YOU decide whether this output belongs in YOUR game. Skip it, and your agent's mistakes become your game's bugs.

THE AUTOMATION WORKFLOW 

The fundamental loop of multi-agent production depends on a reliable handoff between each stage. The sequence follows a consistent pattern that includes a critical human decision point. 

GENERATE 

VALIDATE   
REVIEW 

IMPORT 

A well-structured **Generate → Validate → Review → Import** pipeline keeps human judgment in the loop at the review stage, ensuring only approved content reaches the engine. 

GENERATE 

The AI agent processes its prompt and emits a structured output — JSON, CSV, or plain text formatted per engine spec. 

REVIEW 

A developer or designer inspects flagged or newly generated content before it moves forward. This is where judgment calls happen \- not every output that passes schema validation is actually good.   
VALIDATE 

A translation script intercepts the output, validates the schema, sanitizes the data, and flags anything that doesn't conform to the engine spec. 

IMPORT 

Once reviewed and approved, the content is written into the engine's asset folder and loaded into the project. 

You don't need to build every step from scratch. Describe each step to your LLM and have it write the script. Your job is knowing what the steps ARE and reviewing the output at the Review stage.

FILE WATCHERS: A NOTE FOR LATER 

File watchers and batch importers are production infrastructure \- useful at scale, but not what you need right now. At capstone stage, you need to know which folder to put your file in. That's it. 

When you're ready to automate at scale: a file watcher monitors your agent output directory and moves new files to a staging folder for review. A batch importer processes multiple files in one pass. For now, manual placement into your engine's asset folder is the right approach. 

File watchers feed a staging directory, not the engine directly. The human review step between detection and import is what keeps the pipeline trustworthy.

YOUR INTEGRATION PLAN 

Before you leave this session, know three things: 

THE FILE 

What agent output do you need to get into your game? Name it and its format.   
THE TARGET 

Where does that file need to land in your engine's project structure?   
THE PROMPT 

What do you ask your LLM to build? 'I have \[this file\] in \[this format\]. I'm using \[this engine\]. Write me a loader that reads this file and creates \[game objects\] from it.' 

That prompt is your integration plan. The LLM writes the code. You review it and test it. 

**THE INTEGRATION CHECKPOINT (end of this session): You need agent output loading in your running game before S07.**

THE INTEGRATION   
CHECKPOINT 

This stage represents a critical **Pass/Fail** milestone in the   
production pipeline. 

✅PASS 

Agent-generated content successfully loads into the game engine via the automated conversion script with zero compilation errors. The content is live and playable.   
❌FAIL 

The conversion script throws an error \- typically caused by malformed JSON, schema mismatch, or LLM hallucination. The pipeline halts. The developer must diagnose why the output was bad, adjust the prompt or schema, and then regenerate deliberately. 

The Integration Checkpoint is non-negotiable. Content that fails this gate must never proceed to the engine \- doing so risks crashing the project build.  
DEMO \#5: LIVE   
TERMINAL & GAME ENGINE

LIVE DEMO: THE   
AUTOMATED PIPELINE 

A live demonstration features the terminal running the AI agent operating directly   
alongside an open Game Engine. 

TERMINAL (LEFT MONITOR) 

The AI agent processes prompts and generates new game logic in real-time,   
writing structured output files to the watched directory. 

GAME ENGINE (RIGHT MONITOR) 

As agent outputs land in the staging directory, the developer reviews the content,   
approves it, and triggers the import \- showing exactly where human judgment fits   
in the pipeline. 

Pay attention to the review step between generation and import \- this is   
where your judgment as a developer matters most.  
HOW TO CREATE A SCRIPT

THE TRANSLATION LAYER 

Your agent outputs JSON. Your engine reads JSON. So why do you need a translation layer? Because the agent's JSON schema and your engine's expected schema are almost never the same. 

AGENT PRODUCES OUTPUT IN   
ITS FORMATTRANSLATION SCRIPT   
CONVERTS TO YOUR ENGINE'S   
FORMAT 

YOU DON'T WRITE THIS FROM SCRATCH   
VALIDATION CHECKS THE RESULT BEFORE LOADING 

Tell your LLM: 'I have a JSON file with NPC dialogue in this format: \[paste your agent's output\]. My engine expects this format: \[paste your engine's schema\]. Write me a Python script that converts one to the other and validates the result.' 

Your LLM writes the script. You review it. That's the pattern for every integration problem in this course. 

import json, os 

def process\_agent\_output(raw\_output, target\_dir):   
\# Sanitize and parse   
data \= json.loads(raw\_output.strip())   
out\_path \= os.path.join(target\_dir, "dialogue.json")   
with open(out\_path, "w") as f:   
json.dump(data, f, indent=2)   
print(f"Written to engine: {out\_path}")

HANDLING BAD DATA 

A robust script must anticipate LLM hallucinations. Implementing error-handling within the translation script is not optional \- it is a core architectural requirement. 

DETECT MALFORMED JSON 

Wrap all JSON parsing in a try/except block. A json.JSONDecodeError signals that the LLM generated invalid syntax \- do not proceed to the engine directory.   
DEBUG BEFORE YOU RETRY 

Automatic retry sounds clean, but in practice you need to understand why the output was bad before   
resubmitting. Was it the prompt? The schema? An edge case in the data? Diagnose first, adjust the prompt or schema, then regenerate deliberately. This debugging step is where the real learning happens.   
LOG & ALERT 

Every error event should be logged with the raw output preserved. This creates an audit trail for debugging and helps identify recurring 

hallucination patterns. 

Never let malformed LLM output reach the game engine. A single bad import can corrupt project assets or trigger a compilation failure.

CONSUMABLE   
FORMATS

GETTING CONTENT   
INTO THE GAME 

Generating the content is meaningless if the engine cannot   
consume it. Understanding consumable formats is entirely   
about the mechanics of getting generated content physically   
into the game environment. 

JSON 

The most universally consumable format. Ideal for dialogue trees, item databases, quest 

parameters, and any hierarchical game data structure. 

PLAIN TEXT   
CSV 

Best for tabular data: enemy stats, loot tables, level configuration. Maps directly to Unreal 

DataTables and Unity   
ScriptableObject arrays. 

Used for localization strings, UI copy, and narrative barks. Can be parsed line-by-line and indexed for runtime lookup.  
GETTING AGENT OUTPUT INTO YOUR ENGINE 

The pattern is always the same \- regardless of your engine or your data type. 

01 

YOUR AGENT PRODUCES A FILE (JSON,CSV,TEXT) 

ASK YOUR LLM:   
02 

YOUR ENGINE NEEDS TO LOAD THAT FILE   
03 

YOU NEED A LOADER THAT CONNECTS THEM 

I have a JSON file with \[describe your data\]. I'm using \[your engine\]. Write me a loader that reads this file and creates game objects from it. 

The LLM knows your engine's API better than a slide can teach it. Your job is knowing WHAT you need loaded and WHERE it goes \- the LLM writes the HOW.

Q\&A 

QUESTIONS ABOUT GETTING YOUR AGENT   
OUTPUT INTO YOUR GAME? ASK NOW.

DID YOU ENJOY   
THE CLASS? 

PLEASE TAKE A MOMENT TO COMPLETE A SHORT SURVEY AT   
THE END OF THIS SESSION.REMEMBER TO VERIFY YOUR   
INTEGRATION CHECKPOINTS BEFORE THE NEXT CLASS.YOUR   
FEEDBACK HELPS US IMPROVE YOUR LEARNING EXPERIENCE.
