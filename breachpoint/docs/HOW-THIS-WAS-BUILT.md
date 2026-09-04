# HOW THIS WAS BUILT — a game project run by a crew of AI agents

**Project:** BREACHPOINT — a Halo-inspired 4v4 arena FPS · Unreal Engine 5.8 · pure native
C++ · GAS
**Author and director:** Juan Diego Lugo
**Course:** Multi-Agent AI for Game Development (ELVTR)
**Period covered:** 21 July – 4 September 2026

---

## 0. What this document is, and how to read it

`PROJECT-OVERVIEW.md` describes **the game**: what it is, how it is architected, what has
been verified. `HOW-TO-PLAY.md` describes **the controls**. This document describes **the
method** — how a game of this size was produced almost entirely by AI agents working under
one human engineer, what that took, what it cost, and where it failed.

It is the more interesting half of the submission, because the game is the artifact and the
method is the experiment.

Two kinds of statement appear below, and they are marked, because they carry different
weight:

- **Verifiable in the repository.** Counts, file paths, commits, documents. Anything here
  can be checked against the tree; where it can, the path is named.
- **The director's account.** First-person judgement about what worked, what it felt like,
  and why a decision was made. Marked *(director's account)*. These are not measurements
  and are not offered as such.

The project runs on an **honesty ladder** — *compiles ≠ works · PIE ≠ multiplayer · listen
server ≠ dedicated · editor ≠ packaged* — and every claim in the project names its rung.
That rule applies to this document too. Several passages below are less flattering than
they could have been written, for exactly that reason.

---

## 1. The thesis, and the honest answer

### The question

> Can a small-studio game project be built **completely, or almost completely, by AI
> agents** — with the human acting as director rather than author?

That was the goal from the start *(director's account)*. Not "can AI help me write code" —
that question is settled and uninteresting. The question was whether the **authorship**
could move: whether a single experienced engineer could direct a crew of AI agents the way
a technical director directs a team of engineers, and get a real, working, architecturally
sound game out the other end.

### It was answered twice, because the first answer was no

**Attempt one — `Source/Breachpoint/`.** Maximum ambition, minimum human input. Agents were
given the design and asked to build the game, with very little testing and very little
correction from me *(director's account)*. It grew large: **232 files, 48,641 lines of C++**
— the biggest single body of code in the repository. It also did not work. It was, in my
own assessment, **an unusable game** *(director's account)*.

That result is worth stating plainly rather than burying, because it is the finding. The
failure was not that the agents wrote bad code line-by-line. It was that nobody was holding
the shape of the thing. Code accumulated faster than judgement was applied to it, and by the
time the problems were visible they were structural.

**Attempt two — `Source/BreachpointNext/`.** Started **12 August 2026** as a deliberate
parallel rework, not an in-place cleanup. The founding commit says why, and the reasoning is
the method in miniature:

> *"A parallel tree rather than an in-place cleanup, so nothing arrives by inheritance: the
> current module carries 36 UE-template files nothing references and two disciplines spelled
> twice… Three rules produced the shape — one home per concept, a folder is an ownership
> boundary, and the shape encodes the laws."*

Attempt two is **153 files, 26,569 lines** — roughly *half* the code of attempt one — plus a
**21,715-line AI plugin** (`Plugins/AIBot/`) written as a separate, engine-only module so the
linker itself enforces the boundary. It is smaller, and it works.

### The answer

**Almost entirely by AI — yes. Without a lead engineer — no.**

What changed between attempts was not the tooling and not the models. It was that I stopped
asking for a game and started acting as **lead engineer and technical director**: writing
C++ myself to establish the idiom, showing the agents the correct pattern, then requiring
them to transcribe it rather than invent *(director's account)*. I have shipped shooters
using GAS before, so I knew what "correct" looked like here; the direction was specific
because the experience behind it was specific.

The cost is honest and worth naming: **attempt two took considerably longer than attempt one
would have, had it worked.** Direction is not free. What you buy with it is a codebase whose
quality is a property of the architecture rather than a property of luck — and a project
that actually runs.

---

## 2. The operating structure

### The human's role

One person, in three hats that never blurred:

| Hat | What it meant in practice |
|---|---|
| **Technical director** | Owns the architecture, the laws, and the design rulings. Decides *what correct means* before agents are asked to produce it. |
| **Lead engineer** | Writes the reference C++ for a pattern once, by hand, so agents have an in-repo idiom to transcribe instead of a description to interpret. |
| **Producer** | Cuts tickets, dispatches waves, merges results, and decides what blocks a landing. |

The explicit aim was **minimum involvement, maximum direction** — the human should be the
scarcest input, spent on judgement rather than typing *(director's account)*.

### The rule that made agent output trustworthy

**Transcription over invention.** An agent may only use an engine API, a pattern, or an
idiom it can point at in already-compiled code in this repository. If it cannot, it does not
guess — it writes the question down on a watch-list and stops. This single rule is why a
crew of language models produced code that compiles against a specific engine version
instead of code that reads plausibly and does not build.

Its companion rule: **an audit runs before a build.** The largest packets in the project open
with two to four *read-only* agents, each answering exactly one question, before any file is
written. A representative ticket header, verbatim from `docs/tickets/TICKET_BN22_FOUNDER_TAKEOVER.md`:

> *"Three read-only audits ran first (traversal, GAS wiring, team-play seams); every change
> below transcribes an idiom the audits located, none invents."*

---

## 3. The crew — nineteen agents, separated by power

Agent definitions live in `.claude/agents/`. There are **nineteen**: sixteen top-level roles
plus three content curators. The crew grew from an initial twelve as real ownership
boundaries emerged; specialists are minted, not assumed.

The organising principle is **separation of powers**, and it is the core of the whole method:

| Kind | Examples | Power | Constraint |
|---|---|---|---|
| **Builders** | `bn-builder`, `aib-builder`, `netcode-builder`, `ui-builder`, `anim-builder`, `sim-builder`, `services-builder`, `ai-builder` | Write code inside one scoped **owner path** | Never verify their own work. Never merge. |
| **Critics** | `critic`, `bn-critic`, `aib-critic` | Read-only, adversarial. Two modes: **JUDGE** (score competing designs) and **REFUTER** (try to *break* a finding with a concrete attack) | Cannot write anything, at all. |
| **Verifiers** | `verifier`, `aib-verifier` | Run acceptance checks exactly as written, report verbatim | **No write tools by capability** — "quietly fixed the test" is structurally impossible, not merely forbidden. |
| **Editors** | `bn-editor`, `aib-editor` | Drive the live Unreal editor | Permanently wave-exempt: editor state is global, so editor work is always serial. |
| **Curators** | `arena-architect`, `spotter`, `tuning-curator` | Content and tuning judgement | Convergent vs. divergent curation kept apart. |

Two details in that table are doing most of the work.

**The verifier has no write capability.** Not a policy — a capability boundary. The single
most common failure of an AI coding agent is making the test pass rather than making the
code correct, and the only reliable fix is to give the checking role no hands.

**The critic's REFUTER mode is adversarial by construction.** It is not asked "is this good?"
— agreement is a finding of last resort. It is asked to *write the attack*. For netcode the
instruction is literal: **write the cheat.** In practice this pass paid for itself repeatedly;
`docs/CREW_PLAYBOOK.md` §1 records that it killed an entire approach which reviewed as
plausible and would have shipped confidently wrong.

---

## 4. The law — eight rules, enforced by hooks rather than trust

Eight project laws bind every agent (`CLAUDE.md`): server authority · GAS purity · data is
not code · no gameplay `Tick` · **owner paths** · the honesty ladder · generated assets ·
closed design rulings.

Three of them are **enforced mechanically**. `.claude/hooks/guard_laws.py` is a
pre-tool-call hook: it inspects a write *before* it happens and blocks it. It refuses banned
engine APIs (the engine damage path, hard asset references, unseeded randomness in gameplay
code) and refuses any write outside the claimed packet's owner path.

The cultural point matters more than the mechanism: **a blocked write is the law firing, not
an obstacle to route around.** An agent that cannot make a change files a `contract_gap` in
the ticket and stops. It does not "just fix" a shared file to unblock itself — which is
precisely the behaviour that turned attempt one into a tangle.

**Design rulings are closed.** `docs/DESIGN-RULINGS.md` holds **46 numbered rulings**
(R1–R46). A review judges against the ledger and never re-litigates it. Without this, every
review cycle reopens settled questions and the project oscillates — a specific and expensive
failure mode when your reviewers are language models with no memory of yesterday's argument.

**Only `high`-severity findings block a landing.** Everything else lands in a risk register
alongside the artifact. A review process where any finding blocks is a review process that
never ships.

---

## 5. The unit of work — packets, tickets, waves, rounds

### Packets and tickets

A **packet** is one unit of work: a goal, an owner path, the contracts that bind it, named
acceptance checks, and its inputs. A builder receives exactly one. If the packet is wrong or
incomplete, the builder files a `contract_gap` and **stops that thread** — it never
improvises across the boundary.

Packets come from **tickets**, and tickets are files: `docs/tickets/` holds **50 active
tickets** with **30 more archived** — 80 units of tracked work. A ticket carries its status
line, its contracts, its acceptance checks, and — most importantly — its **Log**.

### The Log is the project's memory

Agent context does not persist. The repository does. So the rule is: **a decision that lives
only in a conversation is lost.** Measurements, refuted hypotheses, rejected approaches and
the reasons for them all go into the ticket's Log. This is why a hypothesis disproved in
August is not re-tested in September, and it is the single highest-leverage habit in the
whole method.

The corollary is **many readers, one writer**: any agent may read any artifact; every
artifact has exactly one agent authorised to write it.

### Waves

Larger efforts fan out as **waves** — one dispatch of several agents in parallel, ending at a
**barrier** where the lead merges before anything else moves (`docs/AIBOT-WAVES.md`, standing
doctrine since 25 August):

| Wave | Width | What runs | Barrier output |
|---|---|---|---|
| **W-AUDIT** | 2–4 | Read-only agents, **one question each** | One merged findings list, with contradictions named |
| **W-BUILD** | 2–4 | Builders on **pre-named, disjoint** file lists | Diffs merged serially; boundary check on the union |
| **W-REVIEW** | 4 | Four critic passes, one attack surface each | Findings ranked; a `high` from any pass blocks |
| **W-VERIFY** | 2–3 | Verifier protocol splits (specs ∥ measured log counts) | One verdict per protocol; no protocol half-run |

The safety rule is one line: **reads parallelize; writes serialize** — unless the packet
names disjoint file sets *in advance*. A collision discovered mid-wave aborts the wave and is
logged as a packet-authoring failure, because that is what it is.

The **merge is a real step**, not a formality. The lead de-duplicates findings, names
contradictions explicitly (*two auditors disagreeing is itself a finding*), and re-runs the
mechanical checks on the merged whole.

### Rounds and roadmaps

Work is organised into numbered **rounds** — the commit history carries `feat(R1…)` through
`feat(R10…)` — each with its own roadmap document stating a one-line goal and the specific
defects it closes. Round 9's goal, for example, is simply *"the bots feel like people"*, and
it enumerates six named behavioural failures with the evidence for each.

A roadmap in this project is not a wish list. It is a list of **things currently wrong, with
how we will know they are fixed.**

There is no `ROADMAP-8.md`, and the gap is deliberate rather than an oversight. **R8 was
TEAMS: it landed, and was then reverted at my call** because the measurement said team play
had made the match worse (12 kills and 461 ambition switches under teams, against 38 and
1,329 in free-for-all). The number is recorded, the round is not renumbered to hide it, and
teams were later brought back *as a fix for that specific collapse* under
`TICKET_BN22_FOUNDER_TAKEOVER.md` rather than by re-asserting the original decision. A
method that renumbers its failures loses the only evidence that would stop it repeating
them.

---

## 6. The two-session topology — and tickets as the protocol between them

This is the piece of the method I consider most transferable *(director's account)*.

Two Claude sessions ran the project, with different capabilities:

- **The cloud session** (Claude Code, connected to GitHub) — the lead. Reads the whole
  repository, writes code and documents, cuts tickets, runs waves, merges, pushes. It has no
  Unreal Engine, so it can never compile or run anything.
- **The terminal session** (Claude Code, local) — the hands. Has the engine, the editor, the
  build toolchain, the packaged build, and the gamepad. It compiles, runs headless matches,
  drives the editor, and reports measurements.

They never talk directly. **They communicate through tickets in git.** The cloud writes a
ticket naming exactly what the terminal must do, which tool calls to make, and what receipts
are owed; the terminal executes, writes results into the ticket's Log, and pushes. The cloud
reads the Log on its next pull.

This solves a genuinely hard problem — the agent that can reason about the whole codebase is
not the agent that can run it — and it does so with no bespoke infrastructure. The protocol
is a Markdown file under version control. It is auditable, it survives both sessions being
restarted, and a human can read it.

It also enforces the honesty ladder structurally. The cloud session *cannot* claim something
compiles, because it has no compiler. Work leaves the cloud stamped **"WRITTEN, NOT
COMPILED"** and only the terminal can raise its rung. Roughly a third of the entries in the
recent ticket Logs are exactly that stamp.

---

## 7. MCP-first — driving the editor instead of clicking in it

### The policy

Unreal's editor is where most game work traditionally happens by hand — and hand work is
invisible to review, unreproducible, and impossible to delegate to an agent. So the standing
policy became: **editor jobs are driven through the Unreal MCP tools, directly, from the
editor session.** My involvement in the editor was to be as close to none as possible
*(director's account)*.

This is written down as **ruling R46** (1 September 2026), which fixed three rules:

1. **A ticket precedes the work.** Every editor job is a ticket naming the toolsets, the
   call sequence, and the receipts owed — never an instruction living only in chat or only
   inside a script.
2. **The primary path is `list_toolsets` → `describe_toolset` → `call_tool`**, with **every
   write read back**. An unverified write is not a write.
3. **A Python script is the fallback, not the default** — lawful only where no toolset
   exposes the operation, or for headless validation that never touches the editor. Using the
   fallback is *recorded in the ticket Log along with the toolset listing that forced it.*

That third rule is the interesting one. The fallback is not banned — it is made **expensive
to use silently.** You may take the shortcut; you must leave evidence that you did and why.

### What MCP was used for

| Surface | Use |
|---|---|
| **Unreal MCP** | Spawning and inspecting actors, editing assets, data tables, StateTree assets, materials, running PIE, capturing viewports, reading logs |
| **Figma MCP** | Building the design system, generating and reading UI screens, extracting measured layout values, exporting assets |

### What it cost, honestly

It was **a work in progress throughout, and UI in particular took a long time to get right**
*(director's account)*. Much of the effort was not building the feature — it was learning the
tool surface well enough to teach the agents to use it correctly, and then getting them to do
so consistently. The repository carries the scar tissue: `docs/ui/ue-frontend/` alone holds
**twenty** working documents, including a `TERMINAL-VS-EDITOR.md` whose entire subject is
which of the two execution contexts a given job belongs in.

I regard the outcome as good — nearly all of it ended up driven from MCP tools *(director's
account)* — but the path was not short, and pretending otherwise would misrepresent the
method's cost.

---

## 8. The UI pipeline — Figma to Unreal, without hand-placement

The front end was built to **Halo Infinite as its reference**, then generalised into an
actual design system rather than a set of copied screens.

The pipeline:

1. **Design in Figma**, via the Figma MCP tools — screens, components, and a design system
   with real tokens. Some UI was authored directly by me; the agents generated the rest
   against the system *(director's account)*.
2. **Measure, don't eyeball.** Screens are reduced to *measured referee documents* checked
   into the repo — e.g. `Source/BreachpointNext/UI/Content/BN/UI/Assets/01-MENU-MEASURED.md`
   — recording absolute layout values on a 1280×720 canvas. The engine's DPI curve does the
   scaling; nothing is multiplied by a fudge factor.
3. **Export only what UMG cannot draw.** The governing rule in
   `docs/ui/ue-frontend/ASSET-PIPELINE.md` is one line: *"Export nothing that UMG can draw."*
   The design language is flat panels, sharp corners, 1px strokes and uppercase type — all of
   which UMG renders natively. So the exported asset set is small and deliberate: icons and
   glyphs, rank marks, scene plates, weapon silhouettes, a handful of materials, fonts.
4. **Import into Unreal via MCP**, with the widget structure built in **CommonUI** — the
   four-layer activatable widget stack (Game / GameMenu / Menu / Modal) with MVVM view models
   behind it, and the screens themselves written in **C++**, not Blueprint.
5. **Verify by contract.** A headless self-test (`Tools/bn/bn41_selftest.py`) checks that the
   C++ bind contract and the built widget tree still agree, so a screen cannot silently drift
   from the code that drives it.

The through-line is the same as everywhere else in this project: **the artifact is not the
source of truth — the measured document and the C++ are, and the artifact is checked against
them.**

---

## 9. The AI opponent — where the method was pushed hardest

The bots are the project's centrepiece, and they are built as a **separate engine-module
plugin** (`Plugins/AIBot/`, 21,715 lines) that cannot see the game's code. The dependency
direction is enforced by the linker: the brain talks to the game only through interfaces.

### The architecture, and why StateTree won

The design is a three-layer brain: **GOAP-style ambitions → utility scoring → a StateTree
spine → GAS as the hand.** Perception enters through a *sensorium* that imposes a reaction
clock and matures stimuli before the brain may see them; the brain then scores **ambitions**
(Roam / Engage / Retreat / Search / Rally / Objective) and **tactics** (Push / Flank / Hold /
Explore) by utility, and a StateTree executes the chosen verb.

I tried to run a **mix of Behavior Tree and StateTree, and StateTree won** *(director's
account)*. Two reasons, and the second is a method finding rather than a design one:

- StateTree's data-oriented model fits a utility-scored brain better than a Behavior Tree's
  reactive traversal does.
- **The agents were markedly better at building StateTree assets through MCP than Behavior
  Trees** — the asset surface is more amenable to being driven programmatically and read
  back. A tool that an agent can verify its own writes against is a tool the agent uses
  correctly.

That is worth generalising: when your workforce is AI agents, *"which technology can the
crew author and verify reliably"* becomes a legitimate architectural criterion, sitting
alongside the traditional ones.

### FAIRPLAY — a constitution for the bot

The bots run under nine written laws (F1–F9) that make fairness structural rather than
tuned: a reaction floor no bot may beat · stimuli mature, never teleport · the brain sees
only what the sensorium admits · aim drifts, never snaps · memory decays · verbs only (the
bot presses the same inputs a player does) · **failure is visible** · raw perception is
quarantined · and **motion is the default — a bot standing still must name why.**

The bots are, deliberately, **players**: they hold weapons through the same ability system,
spend the same verbs, and are bound by the same rules. There is no bot-only shortcut.

### Instrumented, not vibed

The bot work is measured, not judged by feel. `Tools/aib/80_aib_metrics.py` parses match
logs into per-bot counters — stall seconds, idle seconds, path refusals, abandoned goals,
flank counts — and committed **baselines** (5 runs × 300 s per map, seeded) let a change be
compared against its predecessor rather than against an impression.

This is what let a real regression be found and correctly attributed: a 4–6× jump in "stuck
seconds" was traced to the release in which **crowd avoidance was first actually switched
on**, meaning the metric was counting bots politely waiting for teammates as bots being
stuck. That finding was only possible because the numbers existed and were versioned.

---

## 10. Assets and provenance

The project is **content from licensed sources under this project's architecture.** The rule
(`docs/BREACHPOINT-NEXT-ASSET-RULES.md`, 13 August) is: **reuse first, never author what
already exists**, with a written search order:

1. `/Game/FPSTemplate/` — the primary source, ~2,102 assets: weapons, meshes, montages,
   effects, sounds.
2. `/Game/MigrateLyra/` — the Lyra migration, ~826 assets, chiefly mannequin animation and
   material functions.
3. The rest of `Content/` — legitimate, but check *why* a thing is only there.
4. `/Game/BN/` — ours, and only for what steps 1–3 genuinely lack.

Animation is the case where content was kept but **logic was ported**: `docs/ANIM-PORT-LEDGER.md`
audits the template's Blueprints asset by asset with three verdicts — **PORT** (logic
rewritten as C++), **KEEP** (stays an asset), **DROP** (not ours) — with the rule that *a
verdict with no evidence from the inventory is not a verdict.*

That ledger also contains one of the better illustrations of the method working. Its original
headline claim was **wrong**, the error was caught on challenge, and rather than being quietly
edited the document now opens with a boxed correction naming exactly what was wrong and why,
with the original left intact beneath it as the dated record. A project where being wrong is
cheap to admit finds its errors faster than one where it is expensive.

---

## 11. Where the method failed: level design

**Level design is the honest failure of the MCP-first approach** *(director's account)*.

Building the arenas through the MCP surface turned out to be very difficult, and after
sustained effort I put my own level-design skills into it directly rather than continue.
Some effects work went the same way.

The reasons are visible in the tickets and are instructive:

- **Geometry is spatial judgement**, and the loop of "propose → look → adjust" is exactly the
  loop that a text-driven tool surface serves worst.
- **World Partition's one-file-per-actor model** requires an editor save that no MCP tool
  exposed — so a proven, working change could exist only in an editor session that then
  closed. `TICKET_BN22` records precisely this: a stairs fix that demonstrably worked (16
  mid-flight pawns versus 1) but survived only as *"terminal, one action: run the script
  against a live editor and save the level by hand once."*

The blockout kits and generators (`Tools/blockout/`) remain script-driven and reviewable —
law 7 holds, nothing was hand-placed without a committed generator — but the **judgement
about what makes a good arena stayed human**, and I no longer expect it to move.

---

## 12. What the method cost, and what it bought

### It bought

- **A working game with an architecture that holds.** Half the code of the failed attempt,
  doing more.
- **Reviewability.** Every decision has a location. The reason a thing is the way it is can
  be found, and it names its evidence.
- **Error containment.** Owner paths plus mechanical hooks mean a wrong agent damages one
  folder, not the project.
- **Real parallelism.** Audit and review waves genuinely run four-wide, because reads cannot
  collide.
- **Compounding memory.** The ticket Logs mean the project stopped re-making the same
  mistakes, which is the failure mode that killed attempt one.

### It cost

- **Time.** Substantially more than an undirected run would have taken — and the undirected
  run produced nothing usable, so the comparison flatters the method, but the cost is real.
- **A large documentation surface.** Dozens of design documents, 50 live tickets, 46 rulings.
  This is genuine overhead and it must be maintained or it rots.
- **Tool-learning time**, especially for UI, which was measured in weeks rather than days.
- **A hard human dependency.** This method requires someone who already knows what correct
  looks like in this domain. It does not remove the need for a senior engineer — it
  **multiplies one.** That is the finding, and I would not overstate it into anything more.

### The polish caveat, stated plainly

The game is architecturally complete and mechanically complete; it is **not polished at the
asset level**, and that was a deliberate allocation. Effort went into programming and systems
because those were the subject of the experiment. A reviewer should judge the systems, and
should not read rough art as an accident.

---

## 13. The project at a glance

| | |
|---|---|
| Duration | 21 July – 4 September 2026 (~6.5 weeks) |
| Commits | 1,175 |
| Attempts | 2 — `Source/Breachpoint/` (232 files, 48,641 lines, abandoned) and `Source/BreachpointNext/` (153 files, 26,569 lines, shipped) |
| AI plugin | `Plugins/AIBot/` — 100 files, 21,715 lines, engine-module only |
| Agents | 19 (16 roles + 3 curators), `.claude/agents/` |
| Skills | 8 reusable procedure packs, `.claude/skills/` |
| Tickets | 50 active + 30 archived |
| Design rulings | 46, closed and non-re-litigable |
| Project laws | 8, three of them hook-enforced |
| Rounds | R1–R10, each with its own roadmap document (no R8 doc — R8 landed and was reverted, on the record) |
| Tooling | 79 Python tools under `Tools/` |
| Blueprint classes | **Zero** (R18), with one narrow defaults-only exception (R26) |
| MCP surfaces | Unreal MCP (editor), Figma MCP (design) |

---

## 14. What I would do differently

*(director's account throughout.)*

1. **Start with the lead-engineer posture.** Attempt one's lesson is not that AI agents can't
   build a game — it is that nobody was holding the architecture, and no amount of agent
   quality substitutes for that. I would write the reference idiom first, every time.
2. **Instrument before optimising.** The bot work only became tractable once metrics and
   committed baselines existed. Everything before that was argument.
3. **Decide the MCP-vs-hand boundary early, in writing.** R46 arrived at the start of
   September and should have arrived in early August; the weeks before it contain work that
   drifted between execution contexts for no recorded reason.
4. **Concede level design sooner.** The evidence that the MCP surface was the wrong tool for
   spatial judgement arrived well before I accepted it.
5. **Keep the ticket Log discipline exactly as it is.** It is the highest-value habit in the
   entire method and the cheapest one to adopt.

---

## 15. Where to look

| Subject | Path |
|---|---|
| The game itself | `docs/PROJECT-OVERVIEW.md` |
| Controls | `docs/HOW-TO-PLAY.md` |
| The laws | `CLAUDE.md` |
| The method in full | `docs/CREW_PLAYBOOK.md` (16 sections) |
| Agent definitions | `.claude/agents/` |
| Law enforcement hook | `.claude/hooks/guard_laws.py` |
| Design rulings ledger | `docs/DESIGN-RULINGS.md` |
| The ticket board | `docs/tickets/` |
| Wave dispatch doctrine | `docs/AIBOT-WAVES.md` |
| Round roadmaps | `docs/BREACHPOINT-NEXT-ROADMAP-{1..7,9,10}.md` |
| Asset rules and provenance | `docs/BREACHPOINT-NEXT-ASSET-RULES.md`, `docs/ANIM-PORT-LEDGER.md` |
| UI pipeline | `docs/ui/ue-frontend/` |
| Bot metrics harness | `Tools/aib/80_aib_metrics.py`, `Tools/aib/baselines/` |
