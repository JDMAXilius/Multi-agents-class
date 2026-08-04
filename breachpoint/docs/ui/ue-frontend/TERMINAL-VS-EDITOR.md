# TERMINAL vs EDITOR — the front-end division of labour

**Status: operating manual. Not a ruling.** Nothing here overrides `docs/DESIGN-RULINGS.md`,
`CLAUDE.md`, or a ticket's `requires:` line — where this file and those disagree, they win and the
conflict is a finding against this file. Everything labelled **PROPOSAL** is new and unbuilt.

Scope: building the BREACHPOINT front end (HUD, menus, death overlay, carnage report) across two
execution contexts — **(a) Claude in a terminal with the editor CLOSED**, and **(b) the UE 5.8
editor driven live via MCP / Python Editor Script Plugin / Remote Control**.

Companions, all of which this file assumes you have read:
`BREACHPOINT-AUTHORING-MATRIX.md` §5 (the four execution contexts) · the routing notes (removed)
(mode windows) · `.claude/skills/ue-editor` (the bridge) · `.claude/skills/ue5-ui-architecture`
(CommonUI/MVVM) · `.claude/skills/ui-presentation` (the design system and the Figma→WBP handoff).

---

## 1. The hard rule

### R21, verbatim on the point (`docs/DESIGN-RULINGS.md:172`)

> **R21. Stopping an agent does not stop the processes it spawned, and UE's build lock is global.
> One build agent at a time — always.** `TaskStop` kills the agent, not its children: a
> `Build.bat` it launched keeps compiling, keeps holding `Build.bat`'s mutex, and keeps writing
> binaries long after the agent is gone.

Its four consequences (`DESIGN-RULINGS.md:186-197`), compressed: never dispatch a second
build-running agent while one is live, and check for live `UnrealBuildTool`/`cl.exe`/`link.exe`
**before** dispatching, not after · before stopping a build agent, decide what happens to its
build · parallel worktrees give disjoint *files*, never disjoint *build locks* · a wait for a
build must exit on **failure as well as success** — poll for the processes disappearing, not for
the success artifact appearing.

### R29, verbatim (`docs/DESIGN-RULINGS.md:359`, three parts at `:367-379`)

> **R29. One editor, one driver — and an open editor and a build must not overlap.**
>
> 1. **One editor instance per project, ever.** UE takes an exclusive lock on the project; a
>    second instance either refuses to start or opens read-only. This is the engine's behaviour,
>    not our policy.
> 2. **One driver per editor.** A running editor is a single mutable world. Two agents issuing
>    MCP calls into it interleave with no transaction boundary — asset A half-created while asset
>    B saves the level. The MCP exposes no locking, so "don't" is the whole mechanism. A session
>    claiming an `editor-live` ticket owns that editor until it releases it.
> 3. **An editor session and a build must not overlap.** The open editor holds `Binaries/Win64/`
>    DLLs; UBT cannot replace a loaded module, so the build fails late and confusingly, or
>    produces binaries the running editor will never load. **A session using the MCP must not
>    dispatch a builder that compiles, and vice versa.**

And R29 is **not** the whole rule, because R29.3 was written too narrowly. R36
(`DESIGN-RULINGS.md:561`):

> **R36. R29.3 is widened: an editor session must not overlap ANYTHING THAT TAKES THE PROJECT
> LOCK, not merely "a build".** R29.3 named the operation this project rarely runs and missed the
> one it runs constantly — every editor-driving tool we own is a `-run=pythonscript` commandlet
> that takes the lock without being a build.

**So the operative rule for front-end work is R36's, not R29.3's:** an interactive editor and
*any* project-lock operation are mutually exclusive. That includes `run-ubt.ps1`, `run-specs.ps1`,
`run-gauntlet.ps1`, `reimport-tables.ps1`, and every `Tools/*/build-*.ps1` generator.

### What it forbids, and what breaks

| Forbidden | The failure, as observed |
|---|---|
| Build while the editor is open | **Demonstrated 1 Aug 2026** (`DESIGN-RULINGS.md:564-567`): `run-ubt.ps1 -Targets BreachpointEditor` compiled every TU and linked the `.lib`, then failed `LNK1104: cannot open file … UnrealEditor-Breachpoint.dll` because the editor held it. *The compile was fine; the lock was not.* You lose the whole build's wall-clock and learn nothing. |
| Commandlet while the editor is open | Same lock, worse diagnosis — the commandlet may open the project **read-only** or refuse, and a generator that half-runs leaves `Content/` in a state no one planned. |
| Two sessions driving one editor | R29.2: interleaved MCP calls with **no transaction boundary**. Asset A half-created while asset B saves the level. There is no rollback and no diff — a `.uasset` is binary. |
| Editor open while another session runs PIE via MCP | `SURFACE.md:437-440` (§4g): `StartPIE`/`StopPIE` mean an MCP session can start a play session in an editor a human is also using. Same conflict class as a build. |
| Stopping an agent mid-build and dispatching a replacement | R21's origin case: orphaned build completes at 22:35, replacement hits the mutex at 22:39, reports BLOCKED, and its second target comes back INCONCLUSIVE because the orphan already made it up to date. **Two agents, three reports, zero usable rung results.** |

**Enforcement is honesty, not mechanism** (`DESIGN-RULINGS.md:384-386`): `guard_laws.py` gates
`Edit`/`Write` by `file_path`; an MCP tool call has neither, and nothing in the repo can see
whether an editor is open. The *scripts* carry the guard (§4.3 below); the *MCP path* carries
nothing but this document.

---

## 2. Terminal-only work — editor CLOSED

Everything in this section is `CLOSED` or `FILES` mode (the routing notes (removed)). None of it
should ever be done through the MCP, because all of it is diffable text.

### 2.1 What belongs here

| Work | Where it lands |
|---|---|
| Widget C++ classes + `BindWidget` slots | `Source/Breachpoint/UI/` — Tier 1 (`AUTHORING-MATRIX.md:57`) |
| ViewModels, `FieldNotify` fields, setters | `Source/Breachpoint/UI/BRViewModels.h/.cpp` |
| The layer stack, push/pop, input config | `BRUIManagerSubsystem`, `BRActivatableWidget` |
| **Wiring WBP classes to C++** | `Config/DefaultGame.ini` — **not** the editor. See 2.3 |
| UI tuning numbers (ammo-red threshold, killfeed lifetime) | `Content/Data/*.csv` (law 3) |
| Palette tokens | `DT_UIPalette` rows / `UBRUISettings` — never hex in a WBP (`ui-presentation` §9) |
| Automation specs for ViewModel logic | `Source/Breachpoint/Tests/` |
| **Generator and audit scripts** | `Tools/<name>/` — the plan half is plain CPython, no engine |
| Every compile and every rung | `Tools/run-*.ps1` |

### 2.2 The actual commands

```powershell
# Rung 1 — all three targets, R19 timestamp proof. THE gate before any editor opens.
.\Tools\run-ubt.ps1
.\Tools\run-ubt.ps1 -DryRun                     # print the commands, run nothing
.\Tools\run-ubt.ps1 -Targets BreachpointEditor  # PARTIAL — explicitly not a rung-1 result
.\Tools\run-ubt.ps1 -NoUnity                    # real recompile without -Rebuild (R20 way out
                                                # of an "up to date" INCONCLUSIVE)

# Rung 2 — headless specs. Zero discovered tests is BLOCKED here, never green.
.\Tools\run-specs.ps1

# Rung 4 — Gauntlet. Note R30: 4a dedicated is default, 4b listen is required for
# anything whose code path differs host-vs-remote.
.\Tools\run-gauntlet.ps1

# CSV -> DataTable/CurveTable. Editor must be gone; it is a commandlet (R36).
.\Tools\reimport-tables.ps1 -DryRun
.\Tools\reimport-tables.ps1 -Tables DT_Weapons

# Generator pattern — plan first, no engine, then the editor-less run.
.\Tools\gen_input\build-input.ps1 -PlanOnly
.\Tools\gen_input\build-input.ps1 -SelfTest
.\Tools\gen_input\build-input.ps1

# Raw commandlet shape (ue-editor skill §1) when no wrapper exists yet:
UnrealEditor-Cmd Breachpoint.uproject -run=pythonscript -script="Tools/<area>/<script>.py" `
  -stdout -unattended -nosplash
```

Exit codes are shared across the ladder and **only 0 is green**: `0 PASS · 1 FAIL ·
2 INCONCLUSIVE · 3 BLOCKED` (`Tools/run-ubt.ps1:40-53`).

### 2.3 The wiring that everyone reaches for the editor to do — and shouldn't

`UBRUISettings` is `UCLASS(config = Game, defaultconfig)`
(`Source/Breachpoint/UI/BRUISettings.h:13`) and every screen class on it is a
`TSoftClassPtr` (`:25`, `:28`, `:31`, `:34`, `:37`, `:40`). That means the entire
WBP→C++ binding is a **text edit in `Config/DefaultGame.ini`**, made with the editor closed,
diffable by the critic, and surviving a fresh clone with nobody opening the editor. This is
exactly R26's config corollary (`DESIGN-RULINGS.md:321-325`): *prefer ini for anything a script
can set.*

**Live gap, 2 Aug 2026, stated because it is the cleanest example in the repo:**
`Content/UI/` holds `WBP_RootLayout.uasset`, `WBP_HUDLayout.uasset`, `WBP_KillfeedEntry.uasset`
— and `grep -rn "BRUISettings" Config/*.ini` returns **nothing**. Three layout assets exist and
no `[/Script/Breachpoint.BRUISettings]` section points at them, so the subsystem resolves
nothing at runtime. Closing that gap is **terminal work with the editor shut**, not an
`editor-live` ticket.

---

## 3. Editor-only work — needs a live, interactive editor

The list is short on purpose. It comes from Tier 4 of `BREACHPOINT-AUTHORING-MATRIX.md:82-95`,
which is the complete list of what may exist as an asset at all.

| Work | Why C++ / a script cannot express it |
|---|---|
| **WBP widget hierarchy and layout** | Runtime widget trees *are* constructible in C++, so this is not an engine impossibility — it is `AUTHORING-MATRIX.md:92`'s verdict that C++ is **"genuinely worse"**: no designer surface, no live preview, worse results. The matrix's §3 argument is the authority; do not relitigate it. |
| **Anchors, slot padding, alignment, size rules** | Same class of thing: they are `UPanelSlot` properties on a serialized widget tree. Settable in principle, but authored against a viewport you can see. There is **no MCP tool that reaches a widget tree at all** (§5). |
| **UMG animations (`UWidgetAnimation` tracks/keys)** | Sequencer-backed track data. No C++ authoring path, no committed-script path, no MCP toolset. Purely Tier 4. |
| **Material / material-instance graphs** | Graph-only, full stop (`AUTHORING-MATRIX.md:89`). Note: material *instances* have an MCP surface (§5) — the *graph* does not. |
| **Niagara / MetaSounds** | Graph-only (`:90-91`). No MCP toolset at all (`SURFACE.md:386`). |
| **The visual judgment pass** | `ui-presentation` §11: *"the screen was **rendered and looked at**, not just described."* That is a human, not a tool. |
| **Texture import *settings*** | Import itself is scriptable (§5, §6). Whether compression/sRGB/LOD-group can be set through the MCP is **unverified** — see §5's probe. |

**The boundary, stated once:** C++ owns every binding, every value, and all state; the WBP owns
layout, anchors and animation and **nothing else**. `ui-presentation:183-186` and
`ue5-ui-architecture` §4 both say it, and R26's five conditions apply to a WBP exactly as to a
`BP_BR*` container — zero graph nodes, no new variables, no gameplay numbers.

---

## 4. The handoff protocol

### 4.0 The unit of scheduling is the mode window, not the ticket

the routing notes (removed): *every mode switch costs an editor restart, so batch all CLOSED
work, then all OPEN work.* A board run ticket-by-ticket pays a restart per ticket for no reason.
The bus already encodes the modes — `OPEN` (live editor) · `CLOSED` (commandlet/build, editor
must be gone) · `FILES` (no engine) · `ANY` (the message bus (removed)) — and `bus.py inbox` shows a
terminal only what its current mode can run.

### 4.1 CLOSED → OPEN (terminal work → editor work)

1. **Finish and commit the C++ side first.** The `BindWidget` names are the contract the WBP must
   satisfy; authoring layout against names that are about to change is rework. Today's contract:
   `GameLayerStack`, `GameMenuLayerStack`, `MenuLayerStack`, `ModalLayerStack`
   (`Source/Breachpoint/UI/BRRootLayout.h:35-45`) and `KillfeedContainer`
   (`Source/Breachpoint/UI/BRHUDLayout.h:60`, `BindWidgetOptional`).
2. **Run rung 1 to green with the R19 proof.** `.\Tools\run-ubt.ps1`. This is the last moment a
   build is legal. An editor opened on unbuilt code cannot load the module the WBP reparents to.
3. **Run every CLOSED-mode generator the OPEN window will depend on**, in the routing notes (removed)
   order. For UI that is at minimum `reimport-tables.ps1` (a HUD reading no table shows nothing)
   and any icon render (`Tools/render_weapons/render-weapons.ps1`).
4. **Commit and push.** The editor is about to write binaries; a clean tree is what makes
   "which of these `.uasset` changes are mine" answerable.
5. **Claim the editor on the bus, explicitly and by PID.** The precedent is
   a 1 Aug 2026 hand-off message: *"R29 BINDS NOW: I own UnrealEditor PID 43952 for the
   duration. NO SESSION MAY BUILD until I post the release."* Amend `.claude/active-packet.json`
   **additively** (R31) — append your paths, never overwrite another session's.
6. **`git lfs lock` every `.uasset` you will touch.** One owner per binary per ticket (law 7).
   Do not rely on `AssetTools.can_edit_asset` — `SURFACE.md:203`: it *"is always True when source
   control is disabled — it is NOT an lfs-lock check."*
7. **Then open the editor.** One instance (R29.1), one driver (R29.2).

### 4.2 OPEN → CLOSED (editor work → terminal work)

1. **Save everything explicitly.** `AssetTools.save_assets`, or `LevelEditorSubsystem.save_current_level`
   for maps. Never rely on a close prompt.
2. **Write the receipt and commit it with the asset** (R37 — §4.4).
3. **Close the editor. Then verify it is closed (§4.3).**
4. **`git lfs unlock` your paths; post the release on the bus** so the CLOSED lane can start.
5. **Rebuild before trusting anything.** An editor session may have applied Live Coding patches;
   §7 covers why that poisons every subsequent claim.

### 4.3 How to know the editor is *really* closed

Not "I clicked the X." The repo already owns the check —
`Tools/_BRLadderCommon.ps1:296`:

```powershell
function Get-BRLiveEditorProcesses {
    # A running editor holds UnrealEditor-<Project>.dll open: the editor target's link
    # step WILL fail, and any Live Coding patch it applied makes every binary suspect
    foreach ($name in @('UnrealEditor', 'UnrealEditor-Cmd', 'UnrealEditor-Win64-DebugGame')) { ... }
}
```

Three things to note, all load-bearing:

- It tests **any editor process is live**, which is the correct test — R36:567-570 says in as many
  words that this guard was already right when R29.3's *wording* was wrong, and that *"a ruling
  whose wording is narrower than the guards implementing it will eventually be read instead of the
  guards."*
- It includes `UnrealEditor-Cmd`, so a still-running commandlet counts. The two live callers are
  `Tools/gen_input/build-input.ps1:180` and the R26 rename script (removed), both of which
  print `BLOCKED (R21)` and refuse.
- The in-Python equivalent for a script that might be run from the wrong place is
  `Tools/render_weapons/render_weapons.py:151` — it inspects `argv` for `-run=` and **refuses**
  if it looks interactive, with `--allow-interactive` as the documented, must-be-justified escape.

One-liner for a session that has no wrapper to lean on:

```powershell
Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealEditor-Win64-DebugGame -ErrorAction SilentlyContinue |
  Select-Object Name,Id,StartTime
```

Empty output is the only green. R21.4 applies to *waiting*, too: poll for the **processes
disappearing**, never for a success artifact appearing — otherwise a crash is indistinguishable
from slow progress.

### 4.4 What a receipt is (R37)

R37 (`DESIGN-RULINGS.md:574`) permits the MCP to execute an asset step, and defines the price:

> **The MCP MAY execute an asset step. The committed plan plus a receipt is the reviewable
> artifact — never the asset alone.**
> 1. **A committed plan specifies it first.** The plan is a file in the repo. The MCP replaces the
>    `unreal`-importing half of a generator, **not** the deciding half. An MCP call with no
>    committed plan behind it is hand-placing with a different hand and is a `high` finding.
> 2. **A receipt names every call and its result**, and is committed with the asset. The critic
>    cannot diff a `.uasset`; the receipt is what it reviews instead.
> 3. **Law 7 is NOT repealed.** Tier is still answered before the first call.

And its closing warning, which is the reason this document exists at all
(`DESIGN-RULINGS.md:597-603`): `guard_laws.py` gates by `file_path`, **an MCP tool call has
neither**, so *"every mechanical protection this project owns is blind to asset authoring… the
only thing standing where the hook stands elsewhere is receipt discipline and review. A session
that lands an MCP asset without a receipt has defeated the only control there is."*

The implemented shape is `Tools/render_weapons/render_weapons.py:177` (`class Receipt`). Copy it
rather than inventing a format. Its properties, in the order they matter:

- **Written as the run proceeds and flushed per line** (`:203-205`) — *"a receipt reconstructed
  from memory afterwards is not a receipt."* A run killed halfway leaves a receipt saying where it
  died.
- **Header** — UTC start, the verbatim command line, the plan's digest, the source table's sha256,
  the plan's status (`:191-200`).
- **One line per call**: name, arguments, return value — **including failures**, marked
  `**FAILED**` (`:207-213`).
- **Findings** carry a severity and a code (`:219-222`).
- **A verdict plus a rung-honesty paragraph** (`:230-236`) naming what the PASS does *not* mean.
  *"A receipt showing only the calls that worked is an advertisement, not a record."*

Receipts live beside their generator (`Tools/<gen>/receipts/*.md`) and are committed in the same
commit as the asset. **PROPOSAL:** for UI specifically, `mcp-ui/gen_ui/receipts/` once §6's audit
script exists; until then put a UI receipt in the ticket's `## Log` — the ticket is committed too,
and an uncommitted receipt is not a receipt.

---

## 5. What the MCP editor can and cannot drive

Source: `.claude/skills/unreal-mcp/SKILL.md` — 19 toolsets, 255 tools, **enumerated against a live editor**
1 Aug 2026. Read its own honesty note first (`SURFACE.md:19-29`): the tools were reached over raw
HTTP and *"only read-only calls were made… the read-only/mutating marks are derived from each
tool's own declared description and schema, not from firing it."*
the routing notes (removed) sharpens this: **nearly every mark in `.claude/skills/unreal-mcp/SKILL.md` is
description-derived; only four tools have ever been fired at.** Treat any capability below that is
not marked *fired* as a plan, not a fact.

### 5.1 The four questions asked, answered

| Question | Answer |
|---|---|
| **Can it author a WBP widget hierarchy?** | **No.** `SURFACE.md:381-382`, from enumeration not assumption: *"**No Slate/UMG/widget toolset.** `RESEARCH.md` listed 'inspecting Slate widgets' from secondary coverage. It does not exist. **BP10's WBP layout assets stay Tier-4 human work.**"* There is no tool that adds a widget to a tree, reparents one, or reads the tree. |
| **Can it set anchors?** | **No tool exists for it.** Anchors are `UPanelSlot`/`UWidget` properties, so `ObjectTools.set_properties` (`SURFACE.md:305`, *"reaches any UObject"*) is the only conceivable route — and it needs an object handle for a slot inside a widget tree that no listed tool can enumerate. **Unverified and not planned against.** |
| **Can it create a UMG animation track?** | **No.** No UMG toolset, no Sequencer toolset. Not in the 19. |
| **Can it import a texture with settings?** | **Import: yes** — `TextureTools.import_file(folder_path, asset_name, source_file)` (`SURFACE.md:358-363`). **Settings: unknown.** The whole texture toolset is *two* tools, `import_file` and `get_size`; there is no compression/sRGB/LOD-group parameter. `ObjectTools.set_properties` on the imported `UTexture2D` is the plausible route and **has never been fired at a texture.** Say "unverified", not "yes". |

### 5.2 What it demonstrably does have that front-end work uses

| Toolset | Relevance to the front end |
|---|---|
| `AssetTools` (21) | `find_assets`, `exists`, `save_assets`, `move`, `get_dependencies`/`get_referencers` — *"exactly what a law-7 binary-owner check needs"*. **`get_asset_class` returns `_C`-suffixed generated-class names — the mechanical R18/R26 audit primitive the blueprint audit (removed) never got working** (`SURFACE.md:201`). |
| `BlueprintTools` (53) | ⚠️ **largest law exposure** (`SURFACE.md:62-67`). Also the only read path into a WBP's *graph*: `list_graphs`, `list_variables`, `read_graph_dsl`, `get_parent`. Use the **read** half for auditing; `write_graph_dsl` produces R26's forbidden artifact in one call (`SURFACE.md:409-412`). |
| `MaterialInstanceTools` (13) | HUD material instances — scalar/vector/texture params with no shader recompile. The cheap path for palette-driven materials. |
| `DataTableTools` (10) | `import_file`, `get_schema`, `search_row_structs` — makes *"schema declared ≠ schema live"* checkable. Note the fired gotcha at `SURFACE.md:274`: `search_row_structs` is **exact-name match, not substring**; `"BR"` returns `[]` and reads exactly like "the struct is missing". |
| `EditorAppToolset` (21) | `CaptureViewport`, `CaptureEditorImage`, `CaptureAssetImage` — the evidence loop. `StartPIE`/`StopPIE` (see R29's second edge, `SURFACE.md:437-440`). |
| `ProgrammaticToolset` (2) | ⭐ **the law-7 answer.** `execute_tool_script` runs committed Python defining `run() -> Dict`. Sandbox imports are the complete frozenset `{time, datetime, math, json, re, copy}` — **no `unreal`, no `os`, no `open()`**. A script reads its plan through `AssetTools.read_file("/Game/...")` or not at all (`SURFACE.md:336-356`). |

### 5.3 Absences to plan against (verified by enumeration, `SURFACE.md:373-393`)

No console-command execution and no CVar *setter* · no Slate/UMG/widget toolset · no automation
/test toolset (**the ladder stays headless and stays `Tools/run-*.ps1`**) · no Niagara, MetaSound,
AnimGraph, StateTree or EQS toolset · no source-control toolset · no `resources`, no `prompts`.

### 5.4 The three things still unknown, and exactly how to find out

Do not guess these. Each is one read-only or one cheap mutating call in a claimed OPEN window, and
the answer belongs in a receipt.

1. **Can `BlueprintTools.create` produce a `WidgetBlueprint` at all?** Fire
   `create(folder_path="/Game/UI/_Probe", asset_name="WBP_Probe", asset_type="WidgetBlueprint")`
   and then `AssetTools.get_asset_class` on the result. If it succeeds you get an *empty* WBP —
   which is still not layout, since no tool populates the tree. Value: it would let a script
   create-and-reparent the shell so the human only lays out.
2. **Does `ObjectTools.set_properties` reach a `UWidget` / `UPanelSlot`?** Start read-only:
   `ObjectTools.list_properties` on `/Game/UI/WBP_HUDLayout` and on its CDO
   (`BlueprintTools.get_default_object`). If `WidgetTree` appears and its children are addressable,
   §5.1's "no anchors" answer weakens. If they are not, it is settled.
3. **Do texture import settings survive `set_properties`?** Import one throwaway PNG with
   `TextureTools.import_file`, then `ObjectTools.list_properties` on it and look for
   `compression_settings`, `srgb`, `lod_group`, `mip_gen_settings`. Only then attempt a write.

There is also a standing Kickoff item nobody has ticked (`SURFACE.md:446`): **register `.mcp.json`
and confirm the tools resolve by name in a session's tool list.** `.mcp.json` is committed
(`http://127.0.0.1:8000/mcp`) but the surface was reached over raw HTTP. Until a session's tool
list actually contains them, *this is a transport, not a capability* — and a tool that does not
appear in the session's tool list does not exist.

---

## 6. Scripted vs hand-authored

Law 7's rule (`CLAUDE.md:40-42`): *"Blockouts, table reimports, and input assets are **generated by
committed scripts**, never hand-placed."* Applied to the front end:

### 6.1 Scripted (or ini) — do not open the editor for these

| Work | Mechanism |
|---|---|
| **WBP → C++ class wiring** | `Config/DefaultGame.ini`, `[/Script/Breachpoint.BRUISettings]`. Zero tooling. Today's gap (§2.3). |
| **Palette / UI tuning numbers** | CSV → `reimport-tables.ps1`. Existing wrapper, no new script. |
| **Weapon silhouette icons** | `Tools/render_weapons/` — plan (`render_plan.py`, no engine) + executor (`render_weapons.py`, imports `unreal`) + `selftest_no_editor.py` + R21-guarded wrapper. **This split is the model to copy for any new UI generator.** |
| **WBP conformance audit** | **PROPOSAL — the highest-value new script.** See 6.2. |
| **Screenshot evidence for a UI claim** | `EditorAppToolset.CaptureViewport` from a committed `execute_tool_script`, or `AutomationLibrary.take_high_res_screenshot` headless (`ue-editor` skill §3.7). |

### 6.2 PROPOSAL — the WBP audit (removed)

**The problem it solves is named in the rulings, not invented here.** R26:308-319: *"Enforcement is
owed, not assumed… An audit must assert conditions 1–3 mechanically (node count, added-member
count, parent chain) over every `BP_BR*` asset and run in rung 2, or condition 2 erodes to 'only a
little logic' within a month."* the blueprint audit (removed) exists, is **unreviewed, has
never been run, and is not wired into rung 2** — *"an audit nobody runs is an audit that does not
exist."* WBPs are the same hazard with a different extension, and `Content/UI/` now has three of
them.

Shape, mirroring `Tools/gen_input/`:

- `wbp_plan.py` — the committed expectation table, plain CPython, no engine: for each WBP, its
  required C++ parent and its required `BindWidget` names (derived from the headers, e.g.
  `WBP_RootLayout` → `UBRRootLayout` + the four stacks at `BRRootLayout.h:35-45`).
- `audit_wbp.py` — executor. Per WBP: `AssetTools.get_asset_class` (the `_C` primitive),
  `BlueprintTools.get_parent`, `list_variables` (must be empty), `list_graphs` +
  `read_graph_dsl` (**node count must be zero**). Emits an R37 receipt.
- `selftest_no_editor.py` — the plan's logic against a fake surface, provable with no engine.
- `audit-wbp.ps1` — the R21 guard + logging wrapper.

Wire it into rung 2, or it joins `audit_blueprints` in the drawer. **What it cannot check:** that
the layout is *good*. It checks that the WBP is lawful, which is the part a human staring at a
viewport is worst at.

### 6.3 Genuinely human, in the editor

Composing the widget hierarchy · anchors and slot rules against a visible viewport · UMG animation
curves and timing · material/Niagara graphs · the **Review gate** before crew output enters
`Content/` (`AUTHORING-MATRIX.md:153`, context D) · and every fun/feel call. `ui-presentation` §11
is the exit criterion: *the screen was rendered and looked at.*

---

## 7. Failure modes and mitigations

| # | Failure | Why it bites | Mitigation |
|---|---|---|---|
| 1 | **Stale hot-reload / Live Coding** | An editor session patches modules in memory. The DLL on disk and the code in the editor diverge; a HUD "works" against a binary nobody built. `CLAUDE.md` law 6 forbids reporting done on live coding. | `Get-BRLiveCodingArtifacts` (`Tools/_BRLadderCommon.ps1:308`) scans `Binaries\Win64` for `*.patch_*.dll`. **Any hit ⇒ every binary is suspect.** Close the editor, run `run-ubt.ps1` (or `-NoUnity`), re-open. Never claim a rung from a session that hot-reloaded. |
| 2 | **Asset lock contention** | `.uasset` is binary and LFS-tracked (`.gitattributes:2`). Two writers produce an unresolvable conflict, not a merge. | `git lfs lock` before opening; unlock on release. **Do not use `AssetTools.can_edit_asset`** — `SURFACE.md:203`: always True when source control is disabled. `SURFACE.md:389-391` states it plainly: *"nothing here checks an lfs lock. Law 7's one-owner-per-binary remains a human/hook obligation."* |
| 3 | **Binary `.uasset` merge conflict** | LFS pointer conflicts have no textual resolution. One side's work is lost, and which side is lost is arbitrary. | One owner per asset per ticket (law 7). BP18's pattern is the strong form: *"Binary files this ticket OWNS: **all of `Content/`** while claimed. That is the point of batching — one owner, one window, no cross-ticket binary contention."* If it happens: `git checkout --ours/--theirs` the whole file and **redo the other side's work**; never hand-merge. |
| 4 | **Two sessions, one editor** | R29.2. Interleaved MCP calls, no transaction boundary, no diff to reconstruct from. | Claim by PID on the bus before opening (`bus.py post --mode OPEN`), release explicitly. Amend `.claude/active-packet.json` **additively** (R31) — the harm R31 exists for is one session overwriting another's claim. |
| 5 | **Build during an OPEN window** | `LNK1104`, documented at R36:564-567. Whole compile wasted, confusing diagnosis. | The guards already refuse (`build-input.ps1:180`, `rename-r26.ps1:88`). For anything without a wrapper, run §4.3's check first. Lane D in the routing notes (removed) is **empty on purpose** during an OPEN window. |
| 6 | **`BindWidget` desync** | C++ renames a `BindWidget` property; the WBP still has the old name. The widget fails to compile *at asset load*, not at build — so rung 1 stays green and the HUD is empty in PIE. | The C++ header is the contract: land header changes first, then re-open the WBP in the same window. §6.2's audit turns this from "someone notices in PIE" into a rung-2 failure. `BindWidgetOptional` (`BRHUDLayout.h:60`) softens the crash into a silent null — softer, and therefore easier to miss. |
| 7 | **MCP writes outside the repo** | `SURFACE.md:448-461`, **fired and confirmed**: `write_file` confinement held, *but the refusal enumerated ~80 allowed roots and only two are ours.* Every other root is inside the **engine install** — outside the repo, invisible to git, no hook, no diff. Presents as "works on my machine." | Never point an MCP write at a path outside `<project>\Content` or `<project>\Saved`. The receipt records every write path, which is the only place this is visible. |
| 8 | **MCP silently overwrites tuning data** | `SURFACE.md:401-407`: `write_file` **can** overwrite `Content/Data/DT_Weapons.csv` — the data-crew's owner path — with law 5 never firing, because an MCP call has no `file_path` for `guard_laws.py` to see. | UI work never writes `Content/Data/`. If a UI packet needs a number, it files a data-crew request; the tuning-curator owns those rows. |
| 9 | **Silent-null actor/asset creation** | `SceneTools.add_to_scene_from_class`/`from_asset` *"return nothing if creation was unsuccessful"* (`SURFACE.md:212`). A script that does not check reports success over an empty level. | Check every return; record it in the receipt. `execute_tool` raises `RuntimeError` on failure, so the failure mode is specifically the tools that return `None` instead. |
| 10 | **A receipt written afterwards** | It is a reconstruction, and a run that died halfway leaves no trace of where. | Flush per line (`render_weapons.py:203-205`). Record refusals as carefully as successes. |

---

## 8. The daily loop — follow it literally

```
─────────────────────────── START OF SESSION (any mode) ───────────────────────────
 1. cd <repo root>. The game repo root IS the working root (CLAUDE.md:8-12).
 2. git pull --rebase
 4. /tickets list                        # claim with a STATUS line; commit + push the claim
 5. Prove the hook is live by firing a case it must REJECT (routing notes, removed).
    "Skills loaded" is not proof.
 6. Read the ticket's `requires:` line. OPEN, CLOSED or FILES? That decides everything below.

──────────────────────────── LANE: FILES (no engine) ─────────────────────────────
 F1. C++ headers/classes, ViewModels, ini wiring, CSV rows, plan scripts, docs.
 F2. Never dispatch anything that compiles if another session holds an OPEN window.
 F3. Commit small, push, write the decision to the ticket's `## Log`.
     A decision that lives only in chat is lost.

────────────────────── LANE: CLOSED (commandlet / build) ─────────────────────────
 C1. VERIFY NO EDITOR:
       Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealEditor-Win64-DebugGame `
         -ErrorAction SilentlyContinue
     Empty output is the only green. Non-empty ⇒ STOP, post on the bus, do not "just try".
 C2. Check for Live Coding poison: any Binaries\Win64\*.patch_*.dll ⇒ rebuild before trusting.
 C3. .\Tools\run-ubt.ps1                 # rung 1, all three targets, R19 proof
 C4. Generators, -PlanOnly / -SelfTest FIRST, then for real. Order per the routing notes (removed)
 C5. .\Tools\run-specs.ps1               # rung 2. Zero discovered tests is BLOCKED, not green.
 C6. Commit script + receipt + asset together. Push.

──────────────────────────── LANE: OPEN (live editor) ────────────────────────────
 O1. PRECONDITION: rung 1 green at this HEAD with an R19 timestamp proof. Re-prove if HEAD moved.
 O2. PRECONDITION: every CLOSED item the window depends on has RUN. A HUD over a game with no
     input and no tables verifies nothing.
 O3. git lfs lock <every .uasset you will touch>
 O4. Post the claim on the bus: ticket, editor PID, "NO SESSION MAY BUILD until I post release".
     Amend .claude/active-packet.json ADDITIVELY (R31).
 O5. Open the editor. ONE instance. YOU are the only driver.
 O6. Before the first MCP call: which Tier is this asset? Not Tier 4 ⇒ the answer is C++ and the
     step is wrong (R37.3).
 O7. Open the receipt file NOW, before the first call. Append per call, flush per line.
 O8. Author. Layout/anchors/animation only. Zero graph nodes. Zero new variables.
     Zero gameplay numbers. Colours are token names, not hex.
 O9. Render it and LOOK at it (ui-presentation §11). CaptureViewport / CaptureAssetImage.
 O10. Save explicitly. Close the receipt with a verdict AND a rung-honesty paragraph naming
      what the PASS does not mean.
 O11. Close the editor. Re-run C1 until it is empty.
 O12. git lfs unlock. Commit asset + receipt + screenshots together. Push.
 O13. Post the release on the bus so the CLOSED lane can start.

────────────────────────────── BEFORE ANY CLAIM ──────────────────────────────────
 X1. Name the rung (CLAUDE.md law 6): compiles ≠ works · PIE ≠ multiplayer ·
     listen ≠ dedicated · editor ≠ packaged.
 X2. Multiplayer claims come in THREES: server, acting client, observing client.
 X3. A UI claim from PIE alone is not a UI claim (ui-presentation §10). Join-in-progress
     needs a client that joined MID-MATCH.
 X4. Findings and numbers go in the ticket's `## Log`, or they did not happen.
 X5. The session that produced the artifact does not run the critic pass on it
     (routing notes, removed).
```

---

## 9. Open questions this file does not settle

- **`.mcp.json` is a transport, not a proven capability** (`SURFACE.md:446`). Until the tools
  resolve by name in a session's tool list, every OPEN-mode plan above has an untested first step.
- **The claim model is per-ticket; the editor is a per-machine resource spanning six tickets**
  (the routing notes (removed)). Two candidate resolutions are filed there **for the founder**. Until
  one is ruled, an OPEN window serving BP10 + BP08 + BP22 + BP25 has no clean claim shape, and this
  document's §4.1 step 5 (an additive amendment plus a bus post) is the workaround, not the answer.
- **§5.4's three probes are unfired.** The honest state of "can the MCP help with WBPs at all" is
  *no tool exists for the tree; two adjacent tools are unverified.* Fire them in a claimed window
  and correct §5 here, with the receipt.
