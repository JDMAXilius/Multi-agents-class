![][image1]![][image2]  
05 

CLASS 

DYNAMIC CONTENT   
GENERATION:   
RAG-POWERED   
PIPELINES FOR GAME   
CONTENT 

MULTI-AGENT AI FOR GAME DEVELOPMENT   
JOSHUA BURDICK  
![][image3]![][image4]![][image5]![][image6]

Camera on during the class 

HOUSEKEEPING  
Please make to mute yourself so that you don't accidentally interrupt the instructor   
Use the 'Raise hand' feature in Zoom if you want to ask a question   
Don’t forget to lower   
your hand, once   
finished 

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
01 RAG FOR CONTENT   
CONSISTENCY 

02 GENERATING DIVERSE GAME   
CONTENT 

03 PERSONA & TONE CONTROL 

04 OUTPUT QUALITY &   
AUTOMATED CONSISTENCY   
CHECK 

05 CASE STUDY \#3: FORTNITE NPC   
PERSONA CONSISTENCY 

06 DEMO \#4: RAG DEMO \- 4   
CONTENT TYPES FROM ONE   
LORE 

07 ASSIGNMENT \#4: DYNAMIC   
CONTENT PIPELINE  
RAG FOR CONTENT CONSISTENCY

![][image11]  
THE PROBLEM RAG SOLVES Your GDD is 15 pages. Your agent's context window can hold maybe 3 at a time. 

WITHOUT RAG 

The agent generates content from whatever it remembers from its training data. The output sounds like a generic game, not yours.   
WITH RAG 

Before generating anything, the agent pulls the specific sections of YOUR GDD that are relevant to what it's about to write. The output sounds like your game \- because it's working from your material. 

RAG \= your agent refers to your game's own documents before it writes anything.![][image12]  
HOW TO SET THIS UP 

01 

Put your GDD and game docs into a folder your agent can read. That's it. At this scale, your game's lore fits in the context window \- no database required. 

**TRY IT NOW:**   
02 

When your agent needs to generate content, load the relevant sections of your docs into the prompt first. 'Write a merchant's dialogue' \-\> load YOUR merchant lore, YOUR tone guide, YOUR world rules into context.   
03 

The agent writes FROM your material, not from scratch. The output sounds like your game because it's reading your game before it writes. 

Take one paragraph from your GDD. What content would you want an agent to generate that should sound like it came from the same game? That paragraph is what goes into the prompt as context.

![][image13]  
![][image14]![][image15]  
DEFINING RETRIEVAL AUGMENTED GENERATION 

Retrieval-Augmented Generation (RAG) empowers pipelines to   
dynamically retrieve relevant data before generating an output. 

VECTOR-EMBEDDED PIPELINES 

Encode lore, dialogue, and world data into searchable vector spaces for rapid, contextual retrieval at generation time. 

LORE ADHERENCE   
SHARED MEMORY POOLS 

All agents access a unified memory layer, ensuring every generated asset reflects the same foundational game lore. 

By grounding outputs in retrieved context, hallucinations and continuity errors are eliminated before they reach the engine.

SHARED LORE DATABASE 

A shared lore database acts as the **singular source of truth** for an   
entire multi-agent system. 

SINGLE SOURCE OF TRUTH 

Every agent in the pipeline references the identical foundational repository, eliminating inconsistencies across content types.   
REDUCED   
HALLUCINATIONS 

When all agents share the same lore base, the risk of fabricated facts and continuity errors drops significantly. 

SCALABLE CONSISTENCY 

As the game world expands, the central database grows with it, and all agents automatically inherit updated context.  
GENERATING   
DIVERSE GAME   
CONTENT

WHICH CONTENT DOES YOUR   
GAME ACTUALLY NEED? 

A RAG pipeline can generate many types of game content \- but your game is not   
every game. 

DIALOGUE LINES 

QUEST DESCRIPTIONS 

ITEM DESCRIPTIONS 

LORE ENTRIES 

NPC BACKSTORIES 

**FORCING QUESTION:** 

Which of these content types is currently the biggest gap between your   
prototype and a full game? That gap is what you build toward today \- and what   
Assignment \#4 requires you to fill.  
STRUCTURING OUTPUT FOR THE ENGINE Generating the content is only the first half of the pipeline: formatting it is the second. 

WHY STRUCTURE MATTERS 

Outputs must be in precise formats like **JSON** or **CSV** to be cleanly digested by engines like Unity or Unreal. Raw unstructured text cannot be parsed by game engine pipelines. 

JSON for NPC dialogue trees and quest data CSV for item databases and stat tables 

Structured schemas prevent parsing failures   
FORMATTED OUTPUT EXAMPLE 

{   
"npc\_id": "elder\_voss",   
"dialogue": \[   
{   
"trigger": "first\_meeting",   
"line": "The northern vaults   
have been sealed   
for three centuries.",   
"tone": "grave"   
}   
\],   
"faction": "Order of Ash",   
"lore\_verified": true   
}

PERSONA & TONE CONTROL

YOUR GAME'S VOICE \- NOT THEIRS 

The examples above are someone else's game. Your game has a different voice. **EXERCISE (do this now):** 

1\. Open your GDD. Find the section that describes your game's tone, world-feel, or character voice. 

2\.   
Write two prompts: one for a high-stakes moment in your game, one for ambient world-building in your game. 

3\. Run both against your lore document. 

**ASK YOURSELF:** 

Do the outputs sound like your game \- or like someone else's game? If it sounds generic, the fix is in the prompt and the retrieved context. We'll work through that in Assignment \#4.

OUTPUT QUALITY &   
AUTOMATED CONSISTENCY CHECK

THE "GENERATE 10,KEEP   
3" WORKFLOW 

Evaluating output quality at scale requires systemic filtering.   
Developers implement a **"generate 10, keep 3"** workflow, allowing the   
system to produce variations and autonomously select only the   
highest-quality content for engine integration. 

GENERATE 10   
SELECT TOP 3 

EVALUATE 

This approach guarantees that only lore-accurate, tonally consistent, and mechanically valid content reaches the player \- without requiring manual review at scale.  
THE CRITIC AGENT IN ACTION 

Automated consistency checking relies on **adversarial review**. A secondary "Critic Agent" compares newly generated content against your game docs \- catching contradictions before they reach the player. 

THE PATTERN (TEACH THIS TO YOUR LLM) 1\. Load your game docs as context   
PYTHON SNIPPET \- CRITIC AGENT 

2\.   
Feed the new generated content \+ your game docs to your LLM in one prompt 

3\.   
Ask: "Does this new content contradict anything in the existing lore?"   
def critic\_agent(   
new\_quest, lore\_db   
):   
conflicts \= lore\_db.check( 

4\. If yes \- regenerate. If no \- keep it.   
new\_quest   
)   
if conflicts:   
return {   
"status": "fail",   
"reason": conflicts   
}   
return {"status": "pass"} 

Your LLM writes the implementation. You describe what your game docs look like, what the generated content looks like, and what "contradiction" means for your game.

CASE STUDY \#3: EPIC'S PERSONA DEVICE

CASE STUDY: EPIC'S   
PERSONA DEVICE 

Epic shipped a tool that lets any developer build AI-powered NPCs in Fortnite \- turning what used to require a writing team, voice actors, and a dialogue engineer into a prompt and a voice selection. 

HOW IT WORKS 

Define who the   
character is with a simple prompt 

(personality,   
knowledge,   
behavior), pick a voice, and the NPC talks to players, remembers what they've done, and can trigger 

gameplay events.   
TECH STACK 

Google Gemini for dialogue generation \+ ElevenLabs for voice synthesis. Available now in UEFN (Experimental mode).   
THE   
CONSISTENCY CHALLENGE 

The same problem you're solving with your content 

pipeline: how do you keep generated content on-voice for YOUR game? 

WHY THIS MATTERS: This used to require a writing team, voice actors, and a dialogue system engineer. Now it's a prompt and a voice selection. Source: dev.epicgames.com \- AI and NPCs in Unreal Editor for Fortnite

DEMO \#4: RAG DEMO   
GENERATING 4 CONTENT TYPES

LIVE DEMO: 27 NPCS BEFORE   
THE PLAYTEST 

The scenario: you have 10 hours before a playtest. Your world has 30 NPC slots   
and only 3 written characters. Your lore document exists. Your tone exists. You   
need 27 more NPCs who sound like they live in the same world \- not in a   
generic fantasy RPG. 

NPC VOICE   
PROFILES 

Personality, speech pattern, and 

behavioral traits pulled from your lore   
DIALOGUE   
LINES 

Character-voiced lines consistent with world tone and 

faction   
BACKSTORY SUMMARIES 

Character histories grounded in your existing world canon 

RELATIONSHIP FLAGS 

How each NPC relates to the 3 written characters 

THIS IS WHAT RAG SOLVES: Not 'generate content at scale.' Generate content at scale that sounds like your game \- because it's reading your game before it writes.  
ASSIGNMENT \#4: DYNAMIC CONTENT PIPELINE

ASSIGNMENT \#4: DYNAMIC CONTENT PIPELINE \- DUE BEFORE S07 (30 JULY, 11:59 ET) 

DYNAMIC CONTENT PIPELINE 

Build a pipeline that generates content for your game using your GDD as the source material. Your agent reads your game docs before generating \- so the output sounds like your game, not generic content. 

**DELIVERABLES:** 

Your pipeline (however you built it \- script, notebook, or LLM-assisted workflow) 

Three generated outputs that your game actually needs 

A short ReadMe: what content did you generate, does it sound like your game, and what did the critic agent catch? 

The pipeline must target your capstone game. Submissions using placeholder lore receive no credit on Content Quality or Game Connection. 

This is a mandatory assignment critical for your final playable game project. Ensure your consistency check loop is fully functional before submission.

ASSIGNMENT \#4 \- RUBRIC 

Note on Technical Execution: Code that does not run receives 0 across all criteria. Functional code is the minimum bar for submission, not a graded achievement. 

**Criterion Description Points** 

**Game-Anchored Source**   
Knowledge Base is the student's GDD lore document (or direct extension). Placeholder or generic lore \= 0 on this criterion and Content Fit.   
/ 2.0 

Content Fit The three generated content types are ones the student's game   
/ 2.5   
specifically needs. Submission names the gap ('my game is thin on X')   
and output fills it. Generic or irrelevant types \= partial credit at most. 

RAG Implementation Retrieval is accurate \- generated output reflects retrieved context.   
/ 2.0   
Demonstrated by showing query, retrieved chunk, and output side by   
side. 

Consistency Checking   
Critic Agent catches and corrects at least one lore break or tone drift. The correction is shown, not just claimed.   
/ 2.0 

Voice Judgment Report contains self-assessment: do outputs sound like the student's   
/ 1.5   
game? Includes at least one concrete prompt or retrieval tweak made to   
improve game-fit. 

**TOTAL / 10\.0**  
Q\&A 

WE ENCOURAGE YOU TO ASK QUESTIONS   
ABOUT RAG IMPLEMENTATION,VECTOR   
EMBEDDINGS,OR CONFIGURING YOUR CRITIC   
AGENTS.KEEP YOUR QUESTIONS SHORT AND   
CONCISE.

DID YOU ENJOY   
THE CLASS? 

PLEASE TAKE A MOMENT TO COMPLETE A SHORT SURVEY   
AT THE END OF THIS SESSION.YOUR FEEDBACK HELPS US   
IMPROVE THE LEARNING EXPERIENCE.BEGIN   
ARCHITECTING YOUR CONSISTENCY CHECKS BEFORE THE   
NEXT SESSION\!
