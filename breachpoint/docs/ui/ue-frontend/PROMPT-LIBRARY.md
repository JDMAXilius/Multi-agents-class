# BREACHPOINT — Front-end prompt library

**Status:** v1, 2 Aug 2026. For the UE 5.8 front-end/HUD build (BP10 and the four gap tickets
BP21–BP24). This is the **prompt** layer: how to instruct a Claude agent or drive an MCP so the
result lands under the laws instead of being cleaned up afterwards.

**What this is not.** It is not a second design system (`docs/UI-DESIGN-SYSTEM.md` +
`.claude/skills/ui-presentation/SKILL.md` own that), not a second crew (`docs/CREW_MAP.md` §3
owns who wakes when), and not the art-generation library (`docs/ui/ART-PROMPT-LIBRARY.md` owns
image prompts — nothing here overlaps it). Every template below **dispatches an existing crew
agent from `.claude/agents/` against an existing ticket**. If a prompt here ever conflicts with
`CLAUDE.md` or `docs/DESIGN-RULINGS.md`, they win and the conflict is a finding against this file.

**Sibling docs a prompt will cite constantly:** `docs/ui/COMPONENT-SPECS.md` (measured component
geometry) · `docs/ui/SCREEN-BUILD-SPEC.md` (screen invariants, §1) · `docs/ui/MOTION-MEASURED.md`
(durations and curves) · `docs/ui/REFERENCE-EXTRACTION.md` (screen/component inventory + node ids)
· `docs/ui/HUD-REFERENCE.md` §3b (the measured-vs-unverified HUD ledger) · `docs/ui/HUD-AUDIT.md`
(what is currently wrong in Figma) · `Tools/ue_mcp/SURFACE.md` (the verified UE MCP surface).

---

# 1. Prompt design principles for this project

Seven rules. Each one exists because its absence cost this project real work; the anti-pattern in
§6 that paid for it is named in the last column.

| # | Principle | Paid for by |
|---|---|---|
| P1 | Name the contracts up front, by path | R29's three miscitations |
| P2 | Force the honesty ladder into the output shape | `CLAUDE.md:29` law 6 |
| P3 | Demand `file:line` for every factual claim | AP-2, AP-5 |
| P4 | Separate **verified-from-repo** from **proposed** in the output itself | AP-1 |
| P5 | Refuse invented capabilities — the tool list is the capability list | AP-6 |
| P6 | State the scope, and require absence claims to be scoped to it | AP-2 |
| P7 | Preflight the exact call you are about to make, not a similar one | AP-3 |

## P1 — Name the contracts up front, by path

A prompt that says "follow the laws" gets law-shaped prose back. A prompt that says
"`CLAUDE.md:23` law 3 binds this step: any threshold you need is a `Content/Data/*.csv` row, and
if the row does not exist you file a gap instead of typing the number" gets a CSV row or a gap.

Cite **file and line**, not rule numbers alone. `docs/DESIGN-RULINGS.md:359` records that three
documents cited the one-editor rule as "R21" when R21 says nothing of the kind — *"a miscitation
is worse than a missing rule: it reads as settled, so nobody checks."* A prompt that hands over
`docs/DESIGN-RULINGS.md:359` cannot be miscited; a prompt that says "per R21" can.

The minimum contract list for any front-end job:

```
CLAUDE.md:23  law 3 — numbers in Content/Data/*.csv, soft asset refs only
CLAUDE.md:26  law 4 — no gameplay Tick; timers, delegates, FieldNotify
CLAUDE.md:27  law 5 — write only inside owner_path; blocked ⇒ contract_gap and STOP
CLAUDE.md:29  law 6 — every "works" names its rung; MP claims come in threes
CLAUDE.md:32  law 7 — zero Blueprint classes (R18) except R26's five conditions; one owner per binary
docs/DESIGN-RULINGS.md:125 (R18) · :283 (R26) · :172 (R21) · :359 (R29) · :561 (R36) · :574 (R37)
.claude/skills/ui-presentation/SKILL.md   §5 grid · §8 pipeline · §10 prohibitions · §11 self-check
.claude/skills/ue5-ui-architecture/SKILL.md §3 ViewModels · §7 join-in-progress · §8 grep gate
```

## P2 — Force the honesty ladder into the output shape

Law 6 is not satisfied by an agent that *knows* the ladder; it is satisfied by an output that
cannot be written without naming a rung. Make the report schema carry it:

```
rung_evidence[]:  one entry per claim  { claim, rung, command_or_view, verbatim_output_tail }
```

Rungs, per `docs/CREW_PLAYBOOK.md:35-56`: 1 compile · 2 headless specs · 3 functional ·
4 networked (dedicated + 2 clients) · packaged. For UI specifically,
`.claude/skills/ue5-ui-architecture/SKILL.md:120` binds: a HUD claim is only true when read on
**server, acting client, and observing client**. And `ui-presentation` §10 (line 216) adds the UI
form of it — *"PIE is single-process"* — so a screenshot from PIE is a rung-3 claim about one
process, never a rung-4 claim about a HUD.

An agent that has no rung for a claim must write `rung: none — opinion` and keep the claim. That
is the wanted behaviour: labelled opinions are cheap, unlabelled ones cost a HUD rebuild (AP-1).

## P3 — Demand `file:line` for every factual claim

Any sentence of the form "X exists / X is Y / there is no X" carries a citation or is deleted.
This is the single highest-yield line in any prompt here, because the failure it prevents —
plausible assertion about code nobody opened — is invisible in review.

Two forms are acceptable and no third is:

- `Source/Breachpoint/UI/BRViewModels.h:41` — a repo file and line.
- `Figma yznvnVdOFDADaugZSeomfP · node 62:29 · HUD / Elements` — a node id from a call actually
  made this session.

`docs/tickets/TICKET_BP22_RETICLE_STATE.md:95-98` is the model: *"`UBRVM_Combat`
(`UI/BRViewModels.h`, 160 lines) has no target-state field of any kind… Grep for `Reticle` across
`Source/` returns only `UBRReticleWidget` as an unbuilt row"*. That paragraph is checkable in
fifteen seconds. Aim every prompt at that register.

## P4 — Separate verified-from-repo and proposed, in the output

Two headed sections, never interleaved:

```
## VERIFIED (read this session)      — each line ends in file:line or node id
## PROPOSED (mine, not in the repo)  — each line ends in "no source" + what would settle it
```

`docs/ui/HUD-AUDIT.md:257` does this properly under the heading *"Ours by necessity — no
reference exists, must not be presented as 1:1"*. The opposite is `docs/ui/HUD-REFERENCE.md:3-10`:
a whole HUD *"invented rather than extracted"* and deleted. The difference between those two
outcomes is a heading.

Corollary for the UI work specifically: `docs/ui/HUD-REFERENCE.md:100-102` lists what is still
unmeasured (reticle geometry per weapon class, hitmarker art, low-ammo colour, medal-feed anchor,
health-bar height, fill opacity vs colour). Anything an agent produces in those six areas is
PROPOSED by definition, no matter how confident it looks.

## P5 — Refuse invented capabilities

State it as a rule the agent must obey, not a hope:

> If a tool you need is not in this session's tool list, it does not exist. Do not write code
> against it, do not plan a step around it, and do not describe it as "available". Report
> `blocked: <capability> has no tool` and stop that thread.

`Tools/ue_mcp/SURFACE.md:379-390` is the enumerated proof this matters: the UE MCP has **no
Slate/UMG/widget toolset**, **no automation/test toolset**, **no Niagara/MetaSound/AnimGraph/
StateTree/EQS toolset**, and **no console-command execution** — all four were listed in
`RESEARCH.md` from secondary coverage and none exists. The consequence is written into that same
line: *"BP10's WBP layout assets stay Tier-4 human work."* A prompt that lets an agent assume a
`create_widget` tool produces a confident plan for a build that cannot happen.

## P6 — State the scope, and scope every absence claim to it

`docs/ui/HUD-AUDIT.md:322-334` carries a correction in its own body: the audit scanned two Figma
pages, then asserted the scoreboard, death/respawn and pause screens *"do not exist"*. All four
existed on pages it never opened. The ruling it wrote for itself is the sentence every review
prompt must carry verbatim:

> An audit must not report absence outside the scope it actually read. Out of scope and not
> built are different findings, and only one of them is a defect.

So: the prompt names the scope, and the report opens with `scope_read[]` — the literal list of
files, pages or nodes opened. Anything outside it is `out_of_scope`, never `not_built`.

## P7 — Preflight the exact parameter set

`docs/ui/ASSET-METHODS.md:220-232`: cost was preflighted at `resolution: "1k"` (2.5 credits) and
generated at `2k` (10.0). Three sheets, 30 credits against a 7.5 estimate. The rule generalises
past credits to anything with a cost or a lock — a build, an editor session, a 100k-token metadata
dump: **preflight the call you are about to send, not a similar one.** For token cost specifically,
`ui-presentation` §6 (line 132) already warns that `get_metadata` on a real page is *">100k
tokens"* and gets written to a file — so the prompt says "parse it with a script, never Read it".

---

# 2. The standing preamble

Every template in §3 opens with the same line. It works because the preamble lives in the repo and
the agent can read it:

```
Read `docs/ui/ue-frontend/PROMPT-LIBRARY.md` §2.1 and obey it for the whole task.
```

## 2.1 STANDING RULES FOR EVERY FRONT-END PROMPT

1. **Contracts bind before preferences.** `CLAUDE.md` laws 1–8 and `docs/DESIGN-RULINGS.md`
   R18/R21/R26/R29/R36/R37. On any conflict with a skill or with this file, the law wins and the
   conflict is a finding you report.
2. **Cite or delete.** Every factual claim ends in `file:line` or a Figma node id from a call you
   made this session. No citation ⇒ the sentence does not ship.
3. **Two sections, never mixed:** `## VERIFIED (read this session)` and `## PROPOSED (mine, not in
   the repo)`. A proposal in the verified section is the worst defect you can produce here.
4. **Name your rung.** Every "works" carries `rung: 1|2|3|4|packaged` plus the command and its
   verbatim output tail. A UI claim about replicated data needs all three views (server, acting
   client, observing client) or it is rung 3 at best. No rung ⇒ write `rung: none — opinion`.
5. **Scope your absences.** Open your report with `scope_read[]`. Something you did not read is
   `out_of_scope`, never `not_built`.
6. **No invented capability.** A tool not in this session's tool list does not exist. Report
   `blocked: <capability> has no tool` and stop that thread.
7. **Numbers live in CSV** (`CLAUDE.md:23`). If a value you need is not in `Content/Data/*.csv`
   or on a ViewModel, it is a gap you FILE — never a literal you type into C++, a WBP, or a
   details panel.
8. **Zero Blueprint classes** (`CLAUDE.md:32`, R18/R26). A WBP is a layout asset parented to a
   `UBR` C++ class with **zero graph nodes and no new variables**. Anything else is a `high`
   finding, including one you author yourself.
9. **No Tick** (`CLAUDE.md:26`). Bind to `FieldNotify`. `NativeTick` in a widget and a UMG
   property binding in a graph are the same violation wearing different hats
   (`.claude/skills/ue5-ui-architecture/SKILL.md:112`).
10. **Owner path is a wall** (`CLAUDE.md:27`). Blocked by it ⇒ `contract_gap` in the ticket Log and
    STOP. The hook (`.claude/hooks/guard_laws.py`) firing is the law working, not an obstacle.
11. **Stop at the first honest answer.** If step 2 of your task is blocked, report steps 1 and 2
    and stop. Do not improvise across the boundary to produce a complete-looking result.

## 2.2 The report schema (all templates return this)

```json
{
  "scope_read": ["path or node id, one per thing actually opened"],
  "verified":   [{"claim": "", "citation": "file:line | node id"}],
  "proposed":   [{"claim": "", "why": "", "what_would_settle_it": ""}],
  "rung_evidence": [{"claim": "", "rung": "", "command_or_view": "", "output_tail": ""}],
  "contract_gaps": [{"need": "", "owner_agent": "", "ticket_to_file": "", "blocking": true}],
  "law_checks": {"law3_numbers_in_csv": "", "law4_no_tick": "", "law7_zero_bp_classes": ""},
  "blocked": ["capability with no tool, or gate not satisfiable"]
}
```

---

# 3. Templates, one per job type

Each is complete and copy-pasteable. Replace only the `<<…>>` slots; every slot says exactly what
goes in it.

## T1 — Author a C++ UI class (`UBR…`) from a Figma component spec

Dispatch to: **ui-builder**. Context: `files-only` (write with the editor CLOSED, then build —
R36 at `docs/DESIGN-RULINGS.md:561`).

```
Read `docs/ui/ue-frontend/PROMPT-LIBRARY.md` §2.1 and obey it for the whole task.

You are ui-builder (`.claude/agents/ui-builder.md`). Packet: <<ticket file, e.g.
docs/tickets/TICKET_BP10_HUD_FRONTEND.md>>. owner_path: `Source/Breachpoint/UI/`.
Claim file `.claude/active-packet.json` must name this ticket before your first write.

GOAL: author the C++ class `<<UBRMenuRow>>` — header + cpp only, no asset, no editor.

RETRIEVAL SET (read these and nothing else; CREW_PLAYBOOK §14):
- `docs/ui/COMPONENT-SPECS.md` §<<2>> — the measured geometry and the state table for this
  component. Every number you need is there; do not re-derive any of it from a screenshot.
- `docs/ui/REFERENCE-EXTRACTION.md` §5 — the row naming this Figma component set and its UE class
  name. The two names are a pair (`ui-presentation` §2, "Naming law").
- `.claude/skills/ue5-ui-architecture/SKILL.md` §2 (the one widget base) and §3 (ViewModels).
- The current contents of every file you will touch under `Source/Breachpoint/UI/`.

WHAT THE CLASS OWNS AND WHAT IT MUST NOT:
- Owns: `BindWidget` slots, `UPROPERTY` declarations the layout needs, state enums, the setter
  API, delegate binding in `NativeOnActivated` and UNbinding in `NativeOnDeactivated`.
- Must not own: any hex colour (tokens only — `ui-presentation` §9), any gameplay number
  (`Content/Data/*.csv`, law 3), any hard asset or widget-class pointer (`TSoftClassPtr`, law 3),
  any `NativeTick`, any read of the pawn/ASC/GameState (ViewModel only).
- The variant axes in the spec become C++ state, not separate classes. `<<Main Button has 27
  variants across Status × Alignment × Type — COMPONENT-SPECS.md §2>>`; that is enums and a
  state setter, not 27 types.

STATE HONESTY (`ue5-ui-architecture` §7): every state enum you declare carries an explicit
`Unknown` as its FIRST value, matching `EBRUIDataState`. Zero is not an honest pre-data value —
a confident `0/100` on the first frame after join tells the player something false.

IF A FIELD YOU NEED DOES NOT EXIST on `UBRVM_Combat` / `UBRVM_Match`: that is a C++ gap. Do NOT
add it from here and do NOT work around it in the widget (`ui-presentation` §8 step 3). Use T4's
gap procedure: put it in `contract_gaps[]` and keep building the parts that do bind.

DONE WHEN:
- The header compiles conceptually against `Source/Breachpoint/UI/BRViewModels.h` as it exists
  today — you cite the getter each binding will use, by `file:line`.
- `law_checks` filled: no literal number, no hex, no Tick, no new BP class implied.
- Report per §2.2. rung: you may claim rung 1 ONLY if you ran `Tools/run-ubt.ps1` yourself and
  paste the timestamped tail (R19). Otherwise `rung: none — not built`.
```

## T2 — Author a WBP layout asset from a screen spec (editor-side, zero graph nodes)

Dispatch to: **ui-builder**, in a session with the editor open. Context: `editor-live`.
**Read this first:** `Tools/ue_mcp/SURFACE.md:381` — *there is no Slate/UMG toolset on the UE
MCP.* The widget hierarchy is authored **by hand in the UMG editor**; the MCP's role here is
narrow and is spelled out below. A prompt that implies otherwise is AP-6.

```
Read `docs/ui/ue-frontend/PROMPT-LIBRARY.md` §2.1 and obey it for the whole task.

CONTEXT GATE FIRST (tickets skill, `requires:` line). This packet is `editor-live`:
- One editor, one driver (R29, `docs/DESIGN-RULINGS.md:359`). You own it until you release it.
- The editor must NOT overlap a build or ANY `-run=pythonscript` commandlet — R36
  (`:561`) widened R29.3 from "a build" to "anything that takes the project lock". Do not
  dispatch a builder that compiles while you hold the editor.
- R37 (`:574`): a committed plan exists BEFORE the first mutating call, and a receipt is
  committed WITH the asset. An MCP call with no committed plan behind it is a `high` finding.
- Law 7: `git lfs lock` the `.uasset` before editing; you are its only owner for this ticket.

GOAL: author `<<Content/UI/Screens/WBP_HUDLayout.uasset>>`, reparented to `<<UBRHUDLayout>>`,
containing LAYOUT AND ANIMATION ONLY.

THE PLAN, COMMITTED FIRST (R37.1). Before touching the editor, write and commit
`<<Tools/ui/plan_wbp_hudlayout.md>>` containing, for every node: name, parent, widget type,
anchor, position, size, and the token name of every colour. Take every number from:
- `docs/ui/SCREEN-BUILD-SPEC.md` §1 — the invariants true on every 1280×720 frame (profile bar
  0,670 1280×50 · button prompts 74,685 · nav bar 44,45 666×30 · left column x=69/70 ·
  right band x>=650 reserved for the 3D subject).
- `.claude/skills/ui-presentation/SKILL.md` §5 — the measured grid (side margin 69, 3×349 or
  4×249.75, gutter 48 in BOTH, menu row h28 pitch 40, safe bottom 50).
- `docs/ui/COMPONENT-SPECS.md` for any component instance you place.
- For a HUD screen: `docs/ui/HUD-REFERENCE.md` §3b, and NOTHING from §3 where §3b supersedes it.
Any geometry NOT in those files goes in the plan under `PROPOSED` with the reason, per §2.1.3.

HARD PROHIBITIONS (each is a `high` finding, `ui-presentation` §10):
- Zero graph nodes. Zero new variables. R26's five conditions (`docs/DESIGN-RULINGS.md:283`)
  apply to a WBP exactly as to a `BP_BR*` container.
- No hex typed into any details panel — colour is a token read from the palette source
  (`ui-presentation` §9). Twelve widgets with hand-typed hex is twelve places a rebrand breaks
  silently and the critic cannot diff any of them.
- No gameplay number set in the asset (law 3). A threshold that turns ammo red is a `CT_Combat`
  row, not a value in a details panel.
- No branch, no arithmetic, no gameplay read. If the layout needs a computed value it belongs on
  the ViewModel — file it (T4).

WHAT THE MCP MAY DO HERE, and only this (`Tools/ue_mcp/SURFACE.md`):
- `AssetTools.exists` / `find_assets` / `get_asset_class` (`:201`) — confirm the asset path and
  confirm the parent resolves; a `_C` suffix from `get_asset_class` is the mechanical R18/R26
  audit primitive, so use it to PROVE the reparent landed.
- `ObjectTools.set_properties` (`:305`) — default values on properties C++ already declares.
- `EditorAppToolset.CaptureAssetImage` / `CaptureViewport` (`:170`) — the review screenshot.
- `AssetTools.save_assets`, `is_dirty`.
- FORBIDDEN in this packet, whatever the goal: everything in `BlueprintTools`. `write_graph_dsl`
  populates a graph AND compiles it in one call — the exact artifact R26 forbids, as a binary no
  critic can diff (`Tools/ue_mcp/SURFACE.md:409`).

THE RECEIPT (R37.2), committed with the asset as `<<Tools/ui/receipt_wbp_hudlayout.md>>`:
every call in order — toolset, tool, arguments, result — plus `git lfs lock` proof, the final
`get_asset_class` output, and the screenshot path. The critic cannot diff a `.uasset`; the
receipt is what it reviews instead. No receipt = the only control there is has been defeated.

DONE WHEN: plan committed before the first call · receipt committed with the asset · screenshot
attached · `law_checks.law7_zero_bp_classes` states the graph-node count you observed, as a
number · report per §2.2.
```

## T3 — Build a UMG animation from the motion spec

Dispatch to: **ui-builder** (`editor-live`, same R29/R36/R37 gates as T2 — a UMG animation lives
in the WBP).

```
Read `docs/ui/ue-frontend/PROMPT-LIBRARY.md` §2.1 and obey it for the whole task.
This packet is `editor-live`: re-read T2's CONTEXT GATE block and obey it in full. You are
editing a binary the ticket owns; lock it, plan first, receipt after.

GOAL: author the UMG animation `<<Anim_ObjectiveBanner_In>>` inside `<<WBP_ObjectiveBanner>>`.

EVERY TIMING AND CURVE COMES FROM `docs/ui/MOTION-MEASURED.md`. Do not invent a duration and do
not reach for a reflex default:
- The house curve is `cubic-bezier(0.45, 0.15, 0.10, 1.00)` — `Motion.Ease.Standard`
  (`MOTION-MEASURED.md:54-57`). It fits five independent measured series at mean RMSE 0.066
  against 0.191 for `ease-in-out`. `ease-in-out` is measurably the WRONG SHAPE here: the
  reference lands hard and flat, it does not decelerate symmetrically (`:253`).
- Per-phase durations and their own fitted curves are in §2–§4 of that file, phase by phase, with
  RMSE. Use the phase's own curve where one is given; use the house curve otherwise.
- `MOTION-MEASURED.md:13-17`: every source GIF holds its last frame for 1000 ms as an EXPORT
  setting. **Use the authored duration, never the encoded playback total.** Copying the encoded
  number ships a one-second dead tail.
- A centre-anchored panel animates height AND y together (`:250`, 330 ms in / 330 ms out, same
  curve both ways). Animating height alone makes it grow downward, which is not the reference.

STRUCTURAL AFFORDANCES ALREADY AUTHORED IN THE DESIGN — animate these, do not invent your own
(`docs/ui/SCREEN-BUILD-SPEC.md` §2): the selection caret is `Rectangle 278`, 3×65 at x=-4, and it
SLIDES to the focused row; panel reveal wipes originate from the `Rectangle 258`/`259` notches
(88×4.7), which is why the panel unzips rather than fades.

LAW CHECKS SPECIFIC TO ANIMATION:
- An animation is layout/art (Tier 4). It may not carry a gameplay number, a branch, or a state
  decision. What plays and when is C++/ViewModel; how it moves is the asset.
- No Tick-driven interpolation anywhere. If the animation must react to a value, the ViewModel
  pushes and C++ calls `PlayAnimation` — the widget does not poll (`ue5-ui-architecture` §3).
- Attribute presentation smoothing (a shield bar lerp) is a VISUAL interpolation on the ViewModel
  and never feeds back into simulation (`ue5-ui-architecture:65`).

DONE WHEN: every keyframe time traces to a `MOTION-MEASURED.md` line you cite · the curve is named
· plan + receipt committed (R37) · a capture of the end state attached · report per §2.2, with any
timing you could not source listed under PROPOSED with "no measured source".
```

## T4 — Wire a screen to MVVM, including how to file a C++ gap

Dispatch to: **ui-builder**. Context: `files-only` for the C++; `editor-live` only if bindings are
set in the asset.

```
Read `docs/ui/ue-frontend/PROMPT-LIBRARY.md` §2.1 and obey it for the whole task.

GOAL: bind `<<WBP_HUDLayout>>` / `<<UBRHUDLayout>>` to `UBRVM_Combat` and `UBRVM_Match`.

STEP 1 — INVENTORY BEFORE YOU BIND. Open `Source/Breachpoint/UI/BRViewModels.h` and list, with
line numbers, every field you intend to bind. Produce this table and produce it FIRST:

| Element on screen | ViewModel getter | Exists? file:line | If missing → gap id |

`docs/UI-DESIGN-SYSTEM.md:147-160` already records the answer for the HUD: everything except the
reticle target state binds today — vitals, ammo, stowed weapon, grenades, grapple ring, score,
clock, rocket countdown, killfeed. Four gaps are known and TICKETED, so if you land on one of
these, you have not found a new gap, you have found a blocked element:
  per-player stat block → BP21 · reticle target state → BP22 (`docs/tickets/TICKET_BP22_
  RETICLE_STATE.md`) · respawn countdown → BP23 · lobby ViewModel → BP24.

STEP 2 — HOW TO FILE A C++ GAP (this is the point of the template; the wrong move here is the
expensive one). `ui-presentation` §8 step 3: *"If a field does not exist on a ViewModel, that is a
C++ gap — file it, do not work around it in the widget."* Filing means, in order:
  a. Add to `contract_gaps[]`: `{need, owner_agent, ticket_to_file, blocking}`. Owner is decided
     by who owns the DATA, not who noticed: replicated state → netcode-builder; gameplay math →
     sim-builder; the trace behind a cosmetic read → whoever owns the ability (BP22 assigns its
     producer step to sim-builder because BP03 owns the trace).
  b. Write it into the ticket's `## Log`, dated. A decision that lives only in chat is lost
     (`CLAUDE.md:57`).
  c. If it blocks a step, STOP that step. Bind everything that is NOT blocked and report the
     blocked list. Partial + honest beats complete + invented.
  d. Never: add the field yourself outside your owner_path · read the pawn/ASC/GameState from the
     widget to get around it · compute it in the widget · hardcode a plausible value.

STEP 3 — BINDING RULES (`ue5-ui-architecture` §3, §7, §8):
- Fields are `UPROPERTY(FieldNotify, Setter, Getter)` and the setter calls
  `UE_MVVM_SET_PROPERTY_VALUE`. A plain assignment updates NOTHING and presents as a
  "UI doesn't refresh" bug (`:54`).
- Bind in `NativeOnActivated`, UNbind in `NativeOnDeactivated`. A leaked binding that outlives
  its widget is the crash the death cam finds (`:47`).
- The match clock is COMPUTED from one replicated `MatchEndServerTime`, never a ticking
  replicated countdown (`:61`).
- Join-in-progress is not an edge case, it is the normal path: `PlayerState` is null for frames,
  the ASC has not run `InitAbilityActorInfo`, GameState arrives before team assignment
  (`:97-108`).
  Render dashes/dimmed, never a confident zero.
- A widget NEVER calls a Server RPC or mutates replicated state. Intent goes through the owning
  PlayerController's interface (`.claude/agents/ui-builder.md:25`); a widget doing otherwise is a
  netcode finding filed to netcode-builder, not a UI convenience.

DONE WHEN: the inventory table is complete with citations · every unbound element has a gap id or
a ticket number · `law_checks.law4_no_tick` names the grep you ran · rung claims in threes
(server, acting client, observing client) or downgraded to rung 3 with the reason · §2.2 report.
```

## T5 — Extract geometry from Figma via MCP

Dispatch to: any read-capable agent. Context: `files-only`. **Read-only** — no write in this
template; writes are §4.3 and they do not parallelise.

```
Read `docs/ui/ue-frontend/PROMPT-LIBRARY.md` §2.1 and §4 (the Figma harness rules) and obey both.

GOAL: extract measured geometry for `<<component or screen name>>` from
`figma.com/design/yznvnVdOFDADaugZSeomfP` (BREACHPOINT — UI/UX System).
Node ids for reference screens are listed in `docs/ui/REFERENCE-EXTRACTION.md` §4; component sets
and their variant axes are in §5. Look the id up there before you go hunting.

THE CALL SEQUENCE, in this order, and stop as soon as you have what you need:
1. `get_metadata` with NO nodeId → the file's top-level pages. Cheap. Do this first even if you
   think you know the page name; `REFERENCE-EXTRACTION.md:284` records two pages that the MCP
   never listed from the community original, one of which (`Refences - Style Guide`) is the most
   authoritative page in the file.
2. `get_metadata` with the page id → the XML tree. **This is often >100k tokens and is written to
   a file instead of returned** (`ui-presentation` §6). PARSE IT WITH A SCRIPT. Do not `Read` it.
   The depth-limited walk that produced the measured grid is in `ui-presentation` §6 — copy it.
3. `get_screenshot` with `nodeId` + `maxDimension` → the rendered frame. **Set
   `enableBase64Response: true`.** This container's proxy denies direct fetches of `figma.com`
   asset URLs and the curl path in the tool's own response fails with exit 56.
4. `get_design_context` on a node when you want its properties as code.
5. `use_figma` READ-ONLY (a script that only reads and returns) when you need exact fills,
   strokes, stroke alignment, auto-layout padding, letter-spacing units or effect parameters —
   the XML dump does not carry them. `docs/ui/COMPONENT-SPECS.md:3-6` is the precedent: every
   number in that file came out of live nodes this way, "not from a screenshot and not from the
   XML metadata dump".

WHAT THE OUTPUT MUST BE: a table of `name · node id · x · y · w · h`, plus the exact call that
produced each row. Then reconcile against what we already measured:
- `.claude/skills/ui-presentation/SKILL.md` §5 and `docs/ui/REFERENCE-EXTRACTION.md` §3 for the
  grid; `docs/ui/COMPONENT-SPECS.md` for components; `docs/ui/HUD-REFERENCE.md` §3b for the HUD.
- A disagreement is a FINDING, reported with both numbers — not silently resolved in either
  direction. `REFERENCE-EXTRACTION.md` §7 is the model: five conflicts, each with a written
  resolution and a reason.

TRAPS THAT PRODUCE WRONG NUMBERS (all recorded, all real):
- **Rotation.** Figma reports the AXIS-ALIGNED BOUNDING BOX of a rotated node, not its intrinsic
  size (`docs/ui/HUD-AUDIT.md:16-19`). Diamonds are squares at 45°; the MP tracker is tilted
  -9.3°; the weapon silhouette carries 0.42°. Back-solve the AABB before you call it a delta.
- **Text.** A Figma text box includes leading; our measured numbers are ink bounding boxes from a
  pixel mask. A few px of y difference is not a finding (`HUD-AUDIT.md:19`).
- **Letter spacing is PERCENT, not px** (`COMPONENT-SPECS.md:16`). At 14px, 15% ≈ 2.1px.
- **Bloom.** Measured stroke weights off gameplay captures are FWHM of a glow profile and
  overstate a Figma stroke by 1.5–2×. Halve them (`HUD-REFERENCE.md:96-98`).
- **Source quality.** `HINF_HUD.png` is a rendered HTML mockup, not a capture. Do not measure from
  it (`HUD-REFERENCE.md:76-81`). Check what a source IS before you measure it.

Report per §2.2. Every geometry row is VERIFIED with its node id; anything you inferred is
PROPOSED. Absences are scoped to the pages you actually opened (§2.1.5).
```

## T6 — Review / critique a UI packet

Dispatch to: **critic** in REFUTER mode (`.claude/agents/critic.md`). Read-only. This is
`ui-presentation` §11 turned into a prompt, which is what it always was.

```
Read `docs/ui/ue-frontend/PROMPT-LIBRARY.md` §2.1 and obey it. You are the critic in REFUTER
mode: your job is to BREAK this packet with a concrete attack, not to agree with it. Agreement is
a finding of last resort (`docs/CREW_PLAYBOOK.md:12`).

TARGET: <<the diff, the WBP receipt, and the builder's report>>.
SCOPE: list in `scope_read[]` every file, page and node you actually open. You may not report
anything absent outside that list — `docs/ui/HUD-AUDIT.md:334`: "an audit must not report absence
outside the scope it actually read. Out of scope and not built are different findings, and only
one of them is a defect."

THE CHECKLIST — `ui-presentation` §11, one finding per failed line, each with file:line:
1. Every element on the screen traces to a NAMED component (`ui-presentation` §2 /
   `docs/ui/REFERENCE-EXTRACTION.md` §5), or a new one was added to `docs/UI-DESIGN-SYSTEM.md`
   with BOTH its Figma and its UE name. A component on one side only is a defect.
2. Every dynamic value traces to a ViewModel getter THAT EXISTS — cite the line — or is filed as
   a C++ gap with an owner.
3. Colours are token NAMES, not hex, and each is used for its §3 meaning only. Red is the threat
   channel; spending it on a non-lethal warning is a finding. Health is yellow, never green.
4. The WBP has ZERO graph nodes and declares no new variables. State the number you observed.
5. No gameplay number is set in an asset (law 3).
6. Geometry matches `ui-presentation` §5 / `COMPONENT-SPECS.md`, or the deviation is written down
   WITH a reason.
7. The screen was RENDERED AND LOOKED AT (§7 of that skill, or an editor capture), not described.

ADDITIONAL ATTACKS THIS PROJECT HAS PAID FOR — run each explicitly:
- **Rung inflation.** Does any claim about replicated data rest on PIE? PIE is single-process and
  replication bugs are invisible there (`CREW_PLAYBOOK.md:50`). Downgrade it and say so.
- **First-frame lie.** Force the join-in-progress case: null `PlayerState`, ASC not initialised,
  GameState before team assignment (`ue5-ui-architecture` §7). Does anything render a confident
  `0/100`? That is a finding, not a nit — it tells the player something false about a fight.
- **Invented-not-extracted.** For every geometry number: is it measured, or does it merely look
  measured? `docs/ui/HUD-REFERENCE.md:3-10` — an entire HUD was deleted for this.
- **Grep gate** (`ue5-ui-architecture:110-118`): `NativeTick` · a UMG property binding in a graph ·
  `GetPlayerState()`/`GetASC()` polled from a widget · a widget calling a Server RPC · a hard
  widget-class `UPROPERTY` or `ConstructorHelpers` · a gameplay literal in a widget ·
  `SetInputMode*` outside `GetDesiredInputConfig()` · an unbound delegate surviving deactivate.
- **R37 receipt** (`docs/DESIGN-RULINGS.md:574`): if an asset was landed by MCP, is there a
  committed plan from BEFORE and a receipt naming every call? No receipt ⇒ `high`.
- **Gamepad.** Is every interactive path reachable with the mouse unplugged
  (`ue5-ui-architecture` §6)? Hand-rolled focus math is a finding.

SEVERITY AND EXIT (`CREW_PLAYBOOK.md:114-128`, R13): only `high` blocks — a demonstrated exploit,
a broken hard constraint, or internally contradicting numbers. Medium/low land WITH the artifact
in the risk register. Bounded rounds (default 3); on the final round only hard violations may
block. Print your exit condition. You are judging against `docs/DESIGN-RULINGS.md`, never
re-litigating it (law 8).
```

## T7 — Write a UI ticket in the repo's format

Dispatch to: the lead session (or the founder). Output goes to `docs/tickets/TICKET_BPnn_<NAME>.md`
and must match `docs/tickets/TICKET_TEMPLATE.md` exactly.

```
Read `docs/ui/ue-frontend/PROMPT-LIBRARY.md` §2.1 and obey it. Write ONE ticket to
`docs/tickets/TICKET_<<BP26>>_<<SCOREBOARD>>.md`, in the exact structure of
`docs/tickets/TICKET_TEMPLATE.md`. `docs/tickets/TICKET_BP22_RETICLE_STATE.md` is the worked
example for a UI ticket — match its register.

BEFORE YOU WRITE A LINE, verify on disk and cite it. BP22's Log (`:93-98`) is the standard:
"`UBRVM_Combat` (`UI/BRViewModels.h`, 160 lines) has no target-state field of any kind… Grep for
`Reticle` across `Source/` returns only `UBRReticleWidget` as an unbuilt row". A ticket built on
an assumption is a packet that dies at kickoff.

REQUIRED SECTIONS, in order:
1. `# TICKET — BPnn: <short imperative title>` — say what is BROKEN or MISSING, not what to build.
2. `> STATUS: open — cut by <who> <date>. <what unblocks it>`
3. Founder/lead directive, 1–3 lines, plain words, naming any law that binds this ticket.
4. `**Ordering law:**` — what gates what. If nothing does, say so.
5. `## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)`
   - `requires:` FIRST — `files-only` | `engine-installed` | `editor-live`. This is the cheapest
     gate and the most expensive to discover late. A WBP or MCP step is `editor-live` and drags
     R29/R36 with it — and `docs/tickets/TICKET_BP16_UE_MCP_BRIDGE.md:22-29` records the trap:
     "fresh build proof" + "editor open" is unsatisfiable as a simultaneous checklist and
     satisfiable in ONE order (editor CLOSED → build → open → claim). If your ticket needs both,
     write the order into the Kickoff.
   - Each remaining condition must be checkable by a script, and must name the VALIDATOR, not just
     an output: "`Content/Data/DT_Weapons.csv` exists AND re-validates", not "the CSV exists".
   - `owner_path:` the exact folders. This is copied into `.claude/active-packet.json` and the
     hook enforces it on every write.
6. `## Steps (in order)` — each step names the exact file/class/command, the OWNING CREW AGENT
   (`ui-builder` / `sim-builder` / `netcode-builder` / `builder` / `verifier` / `critic`), and the
   contracts binding it. A UI ticket almost always splits across owners: BP22 is ui-builder step 1,
   sim-builder step 2, netcode-builder step 3 — because the data's owner owns the field.
   Include a verification step (which rungs) and, for any dangerous domain, a critic REFUTER step
   with the attack surface NAMED.
7. `## Done when` — observable binary outcomes only. "Rung 4 green on scenario X", never "the HUD
   works". At least one box states the rung. Last box is always "Findings + decisions written to
   this ticket's Log".
8. `## Notes` — Crew line · Contracts (by file) · **Binary files this ticket OWNS (lock before
   editing)** or "none" · Out of scope: what a well-meaning session must NOT do here.
9. `## Log` — "(append findings here, dated, newest last — this is what the next session reads)"
   plus your verification paragraph from step 0, dated, with citations, and an
   "Open questions — each needs a decision, none should be guessed during execution" list.

RULES:
- One ticket per file. A related workflow you notice gets NAMED in Out of scope, never bundled.
- Do not renumber or reference-shuffle existing tickets. Numbers are IDENTIFIERS, not an order
  (`docs/CREW_MAP.md:57`).
- If two tickets would touch one file, they are not two tickets — re-cut them
  (`CREW_PLAYBOOK.md:166`).
- Report per §2.2, with `verified[]` covering every on-disk claim in the ticket.
```

---

# 4. Figma MCP prompt patterns

The `use_figma` harness has a specific shape. An agent that is not told these will write code that
looks correct and fails on the first call. **Include §4.1 verbatim in any prompt that runs a
`use_figma` script.**

## 4.1 THE HARNESS RULES (paste this block)

```
FIGMA `use_figma` HARNESS — obey exactly; each of these fails silently or hard otherwise:

1. YOUR CODE IS AUTO-WRAPPED IN AN ASYNC FUNCTION. Write top-level `await` and a top-level
   `return`. Do NOT wrap your code in an IIFE, do NOT define and call your own async main(), and
   do NOT call `figma.closePlugin()` — the harness owns the lifecycle and closing it discards
   your result.
2. `return` IS THE ONLY OUTPUT CHANNEL. `console.log` goes nowhere you will see. Everything you
   want back — measurements, ids, counts, misses — must be in the returned object. Build a
   result object as you go and return it once.
3. COLOURS ARE 0–1 FLOATS AND THE COLOR OBJECT HAS NO ALPHA KEY. `{r, g, b}` only, each in
   0…1. Opacity is a SIBLING field on the paint (`{type:'SOLID', color:{r,g,b}, opacity:0.5}`),
   never `color.a`. `#35D0F2` is `{r:0.208, g:0.816, b:0.949}`. Divide by 255; do not pass 8-bit
   ints and do not pass a hex string.
4. `setCurrentPageAsync` MAY BE CALLED AT MOST ONCE PER SCRIPT. Two pages = two scripts.
   Usually you need neither: `page.loadAsync()` reads a page without switching to it, and that is
   what both committed HUD repair scripts do (`docs/ui/HUD-AUDIT.md:346-347`).
5. `fills` AND `strokes` ARE READ-ONLY ARRAYS. You cannot mutate an element in place. Clone,
   modify the clone, assign the whole array back:
       const fills = clone(node.fills); fills[0].color = {r,g,b}; node.fills = fills;
   In-place assignment appears to work in the console and changes nothing on the node.
6. VECTOR ORIGIN NORMALISATION — the trap that broke 41 of 42 components
   (`docs/ui/ASSET-METHODS.md:60-95`). Figma normalises every vector node's path data to that
   node's OWN bounding box. Build a multi-part icon as several VECTOR nodes, set each to
   x=0,y=0, and every part collapses to the top-left corner — irrecoverably, because the absolute
   coordinates are gone from the stored path data. THE FIX: ONE compound path per component,
   `windingRule: 'NONZERO'`, then centre the single vector in its box AFTER assigning the paths
   (`v.width` is meaningless until the node has geometry):
       const v = figma.createVector();
       v.vectorPaths = [{ windingRule: 'NONZERO', data: paths.join(' ') }];
       c.appendChild(v); v.x = (w - v.width)/2; v.y = (h - v.height)/2;
   NONZERO not EVENODD, because overlapping solid shapes must UNION; rings still cut their
   counters because a reversed contour cancels — a hole is just a reversed polygon.
7. FONTS: any script that touches text content or type metrics must `loadFontAsync` first. A
   script that only moves and resizes does not — say which yours is, in the return value.
8. NULL-GUARD EVERY LOOKUP AND COLLECT MISSES. Find nodes by name with a guard, push failures
   into `notFound`, and return it. One bad node must not abort the run
   (`docs/ui/HUD-AUDIT.md:358`, `docs/ui/ART-PASS-STAGE-1.md:24-26`).
9. IDEMPOTENCE: a second run must find nothing and report zero. State in your return value
   whether the script is re-runnable.
```

**Provenance note, kept because §2.1.2 applies to this file too.** Rules 4, 6, 7, 8 and 9 are
cited to committed repo files above. Rules 1, 2, 3 and 5 are harness/Plugin-API behaviour known
from use rather than written down anywhere in this repo before now — they are recorded here so the
next session does not rediscover them, and the first script that contradicts one of them is right
and this file is the bug.

## 4.2 Read-only extraction — see T5

Read-only Figma work parallelises. Multiple agents may run `get_metadata`, `get_screenshot` and
read-only `use_figma` scripts at the same time. See §7.

## 4.3 Writes — one at a time, always

`use_figma` **writes cannot be parallelised**: one shared connection and one global page state.
`setCurrentPageAsync` is per-script global (rule 4), so two write scripts racing can operate on
different pages than they think they are on. `docs/ui/HUD-AUDIT.md:3` records the correct
behaviour under contention — *"Nothing was written to Figma; another process holds the write
lock. Repairs are scripted, not applied"* — i.e. the reviewing agent produced scripts and
declined to run them. Copy that: **when a write lock is held, the deliverable is the script, not
the mutation.**

Additional write-side rule from `docs/ui/ART-PASS-STAGE-3.md:60-81`: **you cannot rename a layer
inside an INSTANCE**, and editing text in one creates a per-instance override that silently
diverges from its main forever. 227 of 290 instance-borne strings on our shipping screens resolve
to mains living on a *reference* page. So any prompt that asks for a rename or a text edit must
first ask: is this node inside an instance, and do we own its main? If not, the answer is
stage 3a (author/repoint the component), not the edit.

---

# 5. UE MCP editor prompt patterns

## 5.1 The three gates, in every `editor-live` prompt

```
1. CONTEXT (tickets skill): this packet is `editor-live`. Open the editor BEFORE the claim.
2. R29 (`docs/DESIGN-RULINGS.md:359`) — one editor per project, ONE DRIVER per editor, and an
   editor session must not overlap a build. The MCP exposes no locking; "don't" is the whole
   mechanism. R21 (`:172`) is the other half: one build agent at a time, and stopping an agent
   does not stop the build it spawned.
3. R36 (`:561`) widens R29.3 from "a build" to ANYTHING THAT TAKES THE PROJECT LOCK — including
   every `-run=pythonscript` commandlet we own. Demonstrated: with an editor open, a
   `BreachpointEditor` build compiled everything and then died on `LNK1104` because the editor
   held the DLL. The compile was fine; the lock was not.
```

Practical consequence for orchestration: **an MCP session and a builder that compiles are never
dispatched together.** Not "coordinated" — not dispatched.

## 5.2 R37: plan before, receipt after

`docs/DESIGN-RULINGS.md:574-600`, three obligations, all three in the prompt:

1. **A committed plan specifies the asset first.** The plan is a file in the repo. "The MCP
   replaces the `unreal`-importing half of a generator, not the deciding half. An MCP call with no
   committed plan behind it is hand-placing with a different hand and is a `high` finding."
2. **A receipt names every call and its result**, committed with the asset. The critic cannot
   diff a `.uasset`; the receipt is what it reviews instead.
3. **Law 7 is not repealed.** Answer the tier question before the first call: if it is not Tier 4
   of `BREACHPOINT-AUTHORING-MATRIX.md`, the answer is still C++ and the step is wrong.

And name the enforcement hole out loud in the prompt, because the ruling does: `guard_laws.py`
gates `Edit`/`Write` by `file_path`, and **an MCP tool call has neither**. Receipt discipline is
the only control there is. A session that lands an MCP asset without a receipt has defeated it.

## 5.3 What the surface actually is

`Tools/ue_mcp/SURFACE.md`, enumerated against a live editor 1 Aug 2026. Give the agent these
facts, not "the UE MCP can edit the editor":

- **It is a gateway, not a flat tool list** (`:33`). Three tools: `list_toolsets`,
  `describe_toolset`, `call_tool`. Everything else is
  `call_tool{toolset_name, tool_name, arguments}`. 19 toolsets, 255 tools.
- **`ProgrammaticToolset.execute_tool_script`** (`:341`) is the doctrine-shaped path: Python that
  must define `run() -> Dict[str, Any]`, batching calls to the other 253 tools. Its sandbox
  imports are the complete frozenset `{time, datetime, math, json, re, copy}` (`:348`) — **no
  `unreal`, no `os`, no `open()`**. A script cannot read `Content/Data/*.json` from disk; it goes
  through `AssetTools.read_file("/Game/…")` or not at all.
- **Absences to plan against** (`:379-390`): no Slate/UMG/widget toolset · no automation/test
  toolset · no Niagara/MetaSound/AnimGraph/StateTree/EQS toolset · no console-command execution ·
  no source-control toolset (`can_edit_asset` returns True whenever source control is disabled —
  it is NOT an lfs-lock check).
- **The forbidden toolset for UI packets: `BlueprintTools`.** `write_graph_dsl` populates a graph
  and compiles it in one call — R26's forbidden artifact, as a binary (`SURFACE.md` §4(b), line
  409). `set_variable_replication` creates a replicated property with no netcode packet and no
  REFUTER (§4(c)). Both are `high` findings by construction; name them as banned in the prompt.
- **`AssetTools.write_file` is confined** to `/Game/`, enabled plugins' `Content/` and `Saved/` —
  the confinement was fired and held (§5, line 449). But it CAN silently overwrite
  `Content/Data/DT_Weapons.csv` with law 5 never firing (§4(a), line 404), and the allowed roots
  include ~80 **engine-install** paths outside the repo that git cannot see (§5, line 452).
  Prompt rule: a UI packet never calls `write_file`.
- **Useful and safe:** `EditorAppToolset.CaptureViewport` / `CaptureAssetImage` (the evidence
  loop) · `AssetTools.get_asset_class` (`_C` suffix ⇒ Blueprint generated class: the R18/R26 audit
  primitive) · `ObjectTools.set_properties` (R26 default values) · `LogsToolset.GetLogEntries`
  (three-viewpoint assertions readable during PIE).
- **`StartPIE`/`StopPIE` are an R29 edge** (`SURFACE.md` §4(g)): an MCP session running PIE and a
  human playing in that editor are the same conflict as an MCP session and a build.

## 5.4 The exact-argument rule

`Tools/ue_mcp/SURFACE.md:274`: `DataTableTools.search_row_structs` filters by **exact name match,
not substring** — `"BR"` → `[]`, `"WeaponRow"` → `[]`, `"BRWeaponRow"` → hit, omitted → all 26.
*"A prefix search returns empty and reads exactly like 'the struct is missing'; it cost this
session a near-false-alarm."* Prompt rule, generalised: **an empty result is not evidence of
absence until you have enumerated with no filter.** Same shape as P7.

Also: `SceneTools.add_to_scene_from_class` and `add_to_scene_from_asset` **return nothing on
failure** (`SURFACE.md` §2, `SceneTools`) — a silent null. Every prompt driving them says "check
the return of every mutating call; a null is a failure, not a success with no data."

---

# 6. Anti-patterns — prompts that failed here, and the rule each one buys

Every entry is a recorded event in this repo, not a hypothetical.

## AP-1 — The invented HUD

**What happened.** `docs/ui/HUD-REFERENCE.md:3-10`: *"The first HUD attempt was invented rather
than extracted and has been deleted… I inferred the HUD from `UI-DESIGN-SYSTEM.md` §5 instead of
measuring it. Two of those inferences are now contradicted by official documentation."* The
contradictions were structural, not cosmetic — vitals top-left vs top-centre, score top-centre vs
bottom-middle (`:30-36`). A whole layout, wrong, thrown away.

**Rule the prompt must carry.** Name the measured source for every number, or mark the number
PROPOSED. "Inference is not extraction — if it isn't measured, it doesn't go in." When the
measured source does not exist (`HUD-REFERENCE.md:100-102` lists six such areas), the deliverable
is a *labelled* original design, never a 1:1 claim.

## AP-2 — The over-broad audit

**What happened.** `docs/ui/HUD-AUDIT.md:322-334`: the audit scanned `HUD / Elements` and
`HUD / Core`, then asserted the scoreboard, death/respawn and pause screens *"do not exist"*. All
four existed on pages it never opened.

**Rule.** `scope_read[]` opens every review report, and absence claims are scoped to it. "Out of
scope and not built are different findings, and only one of them is a defect." Bake it into T6.

## AP-3 — The preflight that measured a different call

**What happened.** `docs/ui/ASSET-METHODS.md:220-232`: cost preflighted at `1k` (2.5 credits),
generated at `2k` (10.0). Three sheets = 30 credits against a 7.5 estimate.

**Rule.** Preflight the exact parameter set you are about to send. Generalised for the front end:
before an expensive call, state the parameters and the expected cost in the same message as the
call. `get_metadata` on a page (>100k tokens), a full-page `use_figma` walk, a build, an editor
session — all take the same discipline.

## AP-4 — Measuring from a fan mockup

**What happened.** `docs/ui/HUD-REFERENCE.md:76-81`: `HINF_HUD.png` (3840×2160, "isolated HUD on
black") turned out to be a rendered HTML mockup — the directory contained matching `.html` files.
It agrees with real gameplay on the shield bar to within 0.7 px and diverges badly elsewhere:
tracker diameter +14.7%, rotation off by 14.6°, ticks at 45° instead of 12/3/6/9, no score bar,
no ammo block at all. A source that is right where you check it and wrong everywhere else.

**Rule.** Establish what a source IS before measuring it, and say so in the report. Spot-agreement
on one element is not validation of a source.

## AP-5 — The scoped count quoted as a total

**What happened.** `docs/ui/ART-PASS-STAGE-2.md:119-125` and `ART-PASS-STAGE-3.md:13-24`: a sweep
found 31 Halo-owned strings; the real number was 1,561. Three separate limits each hid work —
layer names never scanned (1,141 occurrences, 73% of the total), case-sensitive matching (71
instances of `HALO` alone), and 12 pages instead of 17. *"The 31 was not wrong about what it
measured. It was wrong to be quoted as the size of stage 3."*

**Rule.** A count carries its method inline: what was scanned, over what range, with what
matching. A number without its method is not reusable and will be quoted as a total.

**Companion trap, same file (`:29-31`):** substring matching on short terms produces false
positives — `reach` matches **B-reach-point**, our own wordmark and tagline. Any term shorter than
~6 characters needs a word-boundary test, not `indexOf`.

## AP-6 — Planning against tools that do not exist

**What happened.** `Tools/ue_mcp/SURFACE.md:44-48`: `RESEARCH.md` listed nine documented tool
categories from secondary coverage. Of those nine, *"inspecting Slate widgets"* and *"running
automation tests"* have **no toolset at all**. The desk-research plan for the UI half of the MCP
work described a capability that has never existed.

**Rule (P5).** The tool list is the capability list. Enumerate before you plan; when documentation
and the live surface disagree, the surface is right and the documentation is the bug. And the UI
consequence must be in every WBP prompt: **the UE MCP cannot author widget hierarchies; WBP layout
stays hand-authored Tier-4 work** (`SURFACE.md:381`).

## AP-7 — Building on components you do not own

**What happened.** `docs/ui/ART-PASS-STAGE-3.md:60-101`: the shipping screens were built with
`node.clone()`, which preserves the instance's pointer to the original main. 227 of 290
instance-borne strings resolve to mains on a *reference* page. Consequence: layers inside instances
cannot be renamed, text edits become 346 permanent overrides, and the only correct fix site is a
page the founder asked to preserve. *"Doing stage 3 before stage 3a means doing it twice."*

**Rule.** Before any bulk edit, prove ownership of the thing being edited — for Figma, whose page
the main lives on; for UE, who holds the lfs lock on the binary. Ownership check precedes edit
plan, always.

## AP-8 — The miscited ruling

**What happened.** `docs/DESIGN-RULINGS.md:359-361`: three documents cited the one-editor rule as
"R21". R21 says nothing about editors. The rule was real, load-bearing for every `editor-live`
packet, and had no ruling at all until R29 was cut. *"A miscitation is worse than a missing rule:
it reads as settled, so nobody checks."*

**Rule (P1).** Cite `file:line`, not a rule number alone. A rule number is a claim; a line is a
check. This applies to prompts citing this file too.

---

# 7. Chaining and orchestration

## 7.1 What parallelises

| Job | Parallel? | Why |
|---|---|---|
| T5 Figma extraction (read-only) | **Yes**, many | No shared mutable state. `docs/ui/SCREEN-BUILD-SPEC.md:3` was produced by "a parallel read-only pass" |
| T6 review / critique | **Yes**, many | Read-only by capability (`.claude/agents/critic.md`, verifier has no write tools at all) |
| T1 C++ authoring, different files | **Yes**, with disjoint owner_paths | `CREW_PLAYBOOK.md:158` — one git worktree per builder, hook enforces non-collision |
| Figma **writes** (`use_figma` mutations) | **NO — never** | One shared connection, global page state, `setCurrentPageAsync` at most once per script (§4.3) |
| UE MCP editor work | **NO — one driver** | R29.2: two agents issuing MCP calls interleave with no transaction boundary — asset A half-created while asset B saves the level |
| UE MCP + any compile | **NO — never together** | R29.3 + R36: the editor holds the DLL; the build dies on `LNK1104` |
| Two builders compiling | **NO — one build agent** | R21: the build lock is global, and killing the agent does not kill its build |
| Two tickets touching one `.uasset` | **NO** | Law 7, one owner per binary per ticket. Two tickets that would touch one file are not parallel tickets — re-cut them (`CREW_PLAYBOOK.md:166`) |

## 7.2 The standard front-end chain

Sequential where the arrow is solid; the bracketed stages fan out.

```
[T5 extract ×N in parallel]  →  reconcile into COMPONENT-SPECS / HUD-REFERENCE  (one writer)
        ↓
T1 author UBR C++ classes  (parallel across disjoint files, editor CLOSED — R36)
        ↓
BUILD  (one agent, R21; editor must be closed)
        ↓
open editor  →  T2 WBP layout  →  T3 UMG animation      ← one driver, R29, plan+receipt R37
        ↓                                                  (T2 and T3 on the SAME asset are
close editor                                                sequential; different assets are
        ↓                                                   still sequential — one driver)
T4 MVVM wiring  (C++ side: editor closed; asset side: back in the editor)
        ↓
[T6 critique ×N in parallel]  →  verifier rungs  →  land
        ↓
T7 tickets for every contract_gap the chain produced
```

## 7.3 Ordering rules that are not obvious

- **Extraction before authoring, always.** Every AP-1-class failure is an authoring step that ran
  before its measurement step.
- **C++ before asset** (`ui-presentation` §8): the class declares the `BindWidget` slots the WBP
  must contain. A WBP authored first has to be re-authored. And the class is written with the
  editor CLOSED, then built (R36) — which is why the whole C++ phase precedes the editor phase
  rather than interleaving with it.
- **Ownership before bulk edit** (AP-7). For a Figma rename pass: prove the mains are ours first.
  For a UE binary: `git lfs lock` first.
- **Gaps are filed, not waited on.** T4 files a `contract_gap` and keeps binding what is not
  blocked. `docs/UI-DESIGN-SYSTEM.md:161` — everything except the reticle state binds today, so a
  blocked field blocks one element, not the screen.
- **Smoke-test the wiring before you spend the pipeline** (`CREW_PLAYBOOK.md:205`): before a long
  run, one trivial round trip per distinct agent, then stop. A full live pipeline once burned real
  wall-clock discovering two prompt-formatting bugs a ten-second pass would have caught. For this
  library that means: one read-only `use_figma` returning `{ok:true}`, one `list_toolsets`, one
  one-line dispatch per agent — before the real chain.
- **A cost-ordered gate never runs after an expensive one** (`CREW_PLAYBOOK.md:130`): parse/schema
  → deterministic validators → critic → verifier → compile → specs → functional → Gauntlet → perf.
  For UI: grep for `NativeTick` before you dispatch a critic to reason about polling.

## 7.4 Routing a failure

`CREW_PLAYBOOK.md:140-142`, and do not collapse the two:

- **Form errors** (malformed output, missed schema, a bad `use_figma` call) go back to the agent
  that produced them, with the exact error, for ONE bounded self-correction.
- **Design disagreements** go through the critic loop and may escalate to the founder. An agent
  must never be allowed to "self-correct" past a design objection.

---

# 8. Self-check before using anything in this file

- The template names the agent from `.claude/agents/`, not a generic role.
- The template names the ticket and its `owner_path`, and the claim file is written.
- The `requires:` context is stated and matches where the session actually is.
- Every contract is cited `file:line`, and the citation was checked this session.
- The output schema (§2.2) is in the prompt, so the honesty ladder cannot be omitted.
- Every capability the prompt assumes appears in this session's tool list.
- The scope is stated, so absences can be scoped to it.
- For an `editor-live` job: plan committed BEFORE, receipt committed AFTER, no build dispatched
  in parallel, one driver.
- For a Figma write: nothing else is writing, and if something is, the deliverable is the script.
