# WORK ROUTING — how to run BREACHPOINT across parallel sessions

**Written 1 Aug 2026.** Companion to `BREACHPOINT-AUTHORING-MATRIX.md` §5, which defines the four
execution contexts. **This file answers the next question: given the contexts, what runs at the
same time, and what is forced to queue?** It is a scheduling document, not a ruling — nothing
here overrides `DESIGN-RULINGS.md` or a ticket's `requires:` line.

Derived from the board on 1 Aug. **Re-derive it rather than trusting it** once tickets move:
`grep -m1 "requires:" docs/tickets/TICKET_*.md` and the STATUS lines are the source.

---

## 1. The scarce resources — this is the whole document

Parallelism on this project is not limited by how many sessions you can open. It is limited by
**three exclusive resources**, and every scheduling decision is about them:

| Resource | Rule | Who is excluded |
|---|---|---|
| **The project lock** | UE takes it exclusively (R29.1) | Everything else that wants it |
| **The build mutex** | One build at a time (R21); an open editor is a party to it (R29.3) | Every other build, and the editor |
| **A second machine** | Required for the Steam rung (BP11) | Not owned today — see §6 |

**The reframing that matters:** the project lock has **two mutually exclusive modes**, and almost
everyone gets this wrong because both are loosely called "editor work":

| Mode | The editor is | Work that runs in it |
|---|---|---|
| **OPEN** | running, interactive, hosts the MCP | BP16 (MCP), BP08 asset authoring, BP10 WBP layout, PIE |
| **CLOSED** | not running; tools take the lock themselves | Every `-run=pythonscript` commandlet **and every build** |

The three highest-priority items on the board — the input generator, the R26 rename, the CSV
reimport — are **all CLOSED-mode**, because all three are `UnrealEditor-Cmd -run=pythonscript`.
They are not "editor tasks that need the editor." They need the editor **gone**.

> **Every mode switch costs an editor restart.** So the unit of scheduling is not the ticket —
> it is the **mode window**. Batch all CLOSED work, then all OPEN work. A board run
> ticket-by-ticket pays a restart per ticket for no reason.

---

## 2. The routing table

`FILES` = no engine (unbounded parallelism) · `CLOSED` = commandlet/build, lock held, serialized ·
`OPEN` = live editor, one driver (R29.2)

| Ticket | Mode | State / what it is waiting on |
|---|---|---|
| **BP15** architect, steps 1–3 | **FILES** | **Ready. Gate green as of 1 Aug.** Best parallel work on the board |
| BP15 steps 4–6 | CLOSED | after steps 1–3 |
| **BP16** UE MCP, step 1 | **OPEN** | **Ready — and the editor is open now.** Read-only; safe before step 2's ruling |
| BP16 steps 2–3 | **FILES** | The law-7 ruling + contract fill-in. Needs step 1's output, then no engine |
| BP16 step 4 | CLOSED | retrofit `gen_input` to the MCP path |
| **BP01** step 3b (input assets) | **CLOSED** | **Ready.** Nothing moves in PIE without it |
| BP01 step 5 (verify + PIE) | OPEN | after 3b |
| **R26 rename** (BP01-adjacent) | **CLOSED** | **Ready.** `Tools/rename_r26/`, never run |
| **BP13** step 6 (CSV reimport) | **CLOSED** | **Ready.** 7 CSVs exist, none imported — no number is read at runtime yet |
| **BP07** arena `.umap` | **CLOSED** | Manifest v3 validates PASS; blockout generator never run |
| **BP00** step 1 | done | — |
| BP00 steps 2–3 | CLOSED | **BLOCKED** — engine C# libs fail NuGet/SDK resolution (§6) |
| **BP03** step 2 (fire path) | CLOSED | **Ready — was in flight and stopped. Restart it.** Also owes `AbilitySet` column + 3 cue tags |
| BP02 / BP04 | CLOSED | steps landed; remainder needs rung 1 |
| BP05 / BP06 / BP09 | CLOSED | gated: BP05←BP02, BP06←BP05, BP09←BP05 |
| **BP08** bots (assets) | **OPEN** | `ST_Bot` + behaviour tree assets. C++ brain landed |
| **BP10** HUD (WBP) | **OPEN** | C++ + ViewModels landed; no WBP assets exist |
| BP11 online / Steam | CLOSED + **2nd machine** | gated by BP08; §6 |
| BP12 ship | CLOSED | gated by BP11 |
| BP14 steps 2–5 | CLOSED | code job type, real ladder, deliberate failures |

---

## 3. The three lanes, running right now

**Lane A — OPEN (T2, the MCP terminal).** Owns the editor under R29.2. **BP16 step 1**, per
`Tools/ue_mcp/STEP1-PLAN.md`. It is the only OPEN work that is both ready and unblocked, and it
is read-only, so it cannot land an asset before step 2's ruling exists.

**Lane B — FILES (T1, this terminal / any session).** **BP15 steps 1–3.** Zero contention with
Lane A: no engine, no lock, no build. This is the genuine parallelism win — the architect's
perception + scoring pass is the largest ready chunk of work on the board that touches no
exclusive resource.

**Lane C — FILES, low priority (T3, cloud).** Doc reconciliation: `SURFACE.md` vs `RESEARCH.md`,
and `ARCHITECTURE §3`'s known staleness (`UI/` 7 vs 4, `BRCombatCurves` unlisted, `Tests/` empty
vs 3). **Do not hand ARCHITECTURE §3 to a human editor** — BP15 step 1 surfaces all of it
mechanically, which is the better fix.

**Lane D — CLOSED. Empty on purpose, and this is the point.** Nothing may build or run a
commandlet while Lane A holds the editor. The moment T2 closes it, Lane D becomes the critical
path and everything in §4 runs back to back.

---

## 4. The CLOSED batch — run these in one window, in this order

When the MCP terminal releases the editor, this sequence is one uninterrupted window. **The
order is load-bearing, not preference:**

1. **`Tools/run-ubt.ps1 -Targets BreachpointEditor`** — R19 timestamp proof. First because it is
   BP16's own Kickoff condition (close → build → open → claim) and because everything below runs
   against the built editor.
2. **R26 rename** (`Tools/rename_r26/rename-r26.ps1`, `-PlanOnly` first). Early, because it
   renames five assets and repoints `DefaultEngine.ini`. Every asset created after it is created
   under the correct names; every asset created before it is one more referencer to fix up.
   `git lfs unlock` the five paths afterwards, then re-run the R26 audit.
3. **Input generator** (`Tools/gen_input/build-input.ps1`, `-PlanOnly` first). Creates the 8
   `IA_*`, `DA_InputConfig`, and 10 `IMC_Default` mappings. **Until this runs,
   `BRPlayerController` binds nothing and you cannot move in PIE** — so it gates every
   OPEN-mode verification that follows.
4. **CSV reimport** (`Tools/reimport-tables.ps1`, BP13 step 6). Seven CSVs, none imported. Note
   its commandlet invocation **has never completed successfully here** — budget debugging time,
   and do not schedule it ahead of items 2–3, which are known-good scripts.
5. **BP03 step 2** — the fire path. The largest *code* item that is ready, and it needs rung 1.
6. **BP07 blockout** — builds the `.umap` from the PASS manifest, if the window still has room.

Then reopen the editor for **BP01 step 5 (PIE)**, which is the first honest test that 3 and 4
actually worked.

> **A dependency worth seeing plainly:** items 3 and 4 are what make PIE meaningful — one gives
> you input, the other gives the game its numbers. Everything OPEN-mode downstream (BP08's bots,
> BP10's HUD, any PIE claim) is verifying against a game that cannot move and reads no tuning
> data until both have run. **That is why the CLOSED batch outranks more OPEN work.**

**Those two premises are no longer assumed.** A read-only MCP pass on 1 Aug checked them against
the running editor: `/Game/Input/DA_InputConfig` **does not exist** (only 4 of the needed `IA_*`
are present), and `/Game/Data` holds **zero assets** — the folder is there, the seven CSVs are
loose files, no DataTable or CurveTable has ever been imported. Items 3 and 4 are real. Evidence
and two concrete reimport risks are in BP16's Log.

### 4a. Hypothesis — items 2, 3 and 4 may not be CLOSED-mode at all

**Status: unproven. Do not reschedule on it.** Recorded here because it targets the exact
premise §4 is built on, and because BP16 step 4 is the measurement that settles it.

§1 classifies the input generator, the R26 rename, and the CSV reimport as CLOSED **because they
are `UnrealEditor-Cmd -run=pythonscript` today** — that is a fact about how the scripts are
invoked, not about what the work needs. BP16 step 1 (`Tools/ue_mcp/SURFACE.md`) enumerated 255
MCP tools against a live editor, and each of the three has an apparent OPEN-mode equivalent:

| CLOSED batch item | Today | Candidate OPEN-mode path |
|---|---|---|
| **2. R26 rename** | `-run=pythonscript` | `AssetTools.move` — **exists, but referencer fixup is UNPROVEN.** See the correction below |
| **3. Input generator** | `-run=pythonscript` | `DataAssetTools.create` + `ObjectTools.set_properties` |
| **4. CSV reimport** | `-run=pythonscript` | `DataTableTools.import_file` + `CurveTableTools.import_file` |

**Two structural facts make this more than a guess.** `UInputAction`, `UInputMappingContext`, and
`UBRInputConfig` are **all `UDataAsset` subclasses** (engine headers + `Input/BRInputConfig.h:92`),
so `DataAssetTools.create` is type-compatible with everything `Tools/gen_input/` produces. And all
six BR row structs **resolve live in the editor** via `DataTableTools.search_row_structs`, so the
reimport path is not blocked on schema availability.

> **CORRECTION, 1 Aug — this table overstated one cell, and the overstatement was load-bearing.**
> Row 2 originally read that `AssetTools.move` *"rewrites the package name and fixes up
> referencers, same as `EditorAssetLibrary.rename_asset`."* **That was inference from UE
> semantics, not from the tool.** Its complete description is: *"Moves or renames an asset or
> folder. Returns: True if the rename happened successfully. False otherwise."* It says nothing
> about package names or referencers. R26 needs exactly `rename_asset` semantics — a rename that
> does not rewrite the package name leaves the asset unloadable — so this is the one cell that
> decides whether row 2 is real, and it is **unproven**. Surfaced by T1 asking, correctly,
> whether each read/mutate mark came from a description or from evidence. Nearly every mark in
> `SURFACE.md` is description-derived; only four tools have been fired at.
>
> The same caveat applies to rows 3 and 4 one hop down: `DataAssetTools.create` accepting
> `UInputAction`, and `ObjectTools.set_properties` populating `UBRInputConfig`'s tag→action maps,
> are both **unverified** — each needs a mutating call, and BP16 step 2 has no R-number yet.

**If it holds, the scheduling consequence is large:** three of the six CLOSED items move into the
OPEN window that BP16/BP08/BP10 already need, and the board stops paying a mode switch to reach
its own top three priorities.

**Four reasons not to act on it yet:**

1. **Item 1 is CLOSED forever.** There is no build tool and no automation toolset on the MCP —
   `run-ubt.ps1` and the whole ladder can never move. **The CLOSED window never disappears**; at
   best it gets shorter, and R29.3 still forbids a build during any OPEN window.
2. **The MCP path loses the lfs-lock check.** `rename_r26.ps1` does a law-7 lock check;
   `AssetTools.can_edit_asset` **returns True whenever source control is disabled**, so it is not
   a substitute. Moving item 2 to OPEN mode without replacing that check trades a scheduling win
   for a law-7 hole.
3. **Law 7 still wants a committed script**, and the MCP does not repeal it. The lawful shape is
   `ProgrammaticToolset.execute_tool_script` running a committed Python script — its sandbox
   imports only `{time, datetime, math, json, re, copy}`, so it cannot read a manifest from disk
   and must go through `AssetTools.read_file("/Game/...")`. A port is a rewrite of the executor
   half, not a flag change.
4. **The MCP is not yet verified as a session capability.** `.mcp.json` is committed but the
   tools have never resolved by name in a session's tool list — the surface was reached over raw
   HTTP. Until a session restart proves otherwise, this is a transport, not a capability.

**The order in §4 stays load-bearing either way.** Rename before creating assets, input before
reimport, both before any PIE claim — that is a dependency argument, not a mode argument, and
running the three in OPEN mode does not reorder them.

**How this gets settled:** BP16 step 4 retrofits **one** generator (`Tools/gen_input/`) and
records wall-clock vs. the `UnrealEditor-Cmd` path, lines deleted, and any behaviour the MCP
could not reproduce. Until that number exists, §4 is the schedule.

---

## 5. The rules that make parallel sessions safe

1. **One working tree.** `git worktree list` returns one. Both terminals share the same files —
   no branch isolation, no per-session staging. **Never `git add -A` or `git commit -a` while
   another session is live.** Stage explicit paths; the lanes own disjoint paths so this works.
2. **Sync before every push.** `git pull --rebase`, fast-forward only, never force-push.
3. **One driver per editor (R29.2).** A non-owning session does not touch the editor — not to
   "just check something."
4. **No build while the editor is open (R29.3).** Lane B and C sessions must not dispatch
   anything that compiles during an OPEN window.
5. **The builder does not grade its own output.** The session that produces an artifact is not
   the session that runs the critic pass on it.
6. **A claim is per session, and the hook binds to the launch root.** Every session starts with
   its working directory at the game repo root, and proves the hook is live by firing a case it
   must reject (BP01 Log, 1 Aug amendment) — not by observing that skills loaded.

---

## 6. Standing blockers — not schedulable until someone acts

- **Rung 4 / Gauntlet (BP00 steps 2–3).** The engine's own C# libs fail NuGet/SDK resolution
  through MSBuild (`Microsoft.Extensions`, `MongoDB`, `Polly`, `OpenTracing`). **Workstation
  repair, not a packet** — likely `Setup.bat` or `dotnet restore` against the engine solution.
  Everything rung-4 waits on it, including R30's new 4b listen-server axis.
- **Rung 2 / specs.** `Source/Breachpoint/Tests/` holds only a `.gitkeep`. R25 settled ownership
  (one spec file per packet, exact-path grant) and nobody has written one. **Every rule landed
  so far is unpinned.** This is FILES-mode work and therefore schedulable into any lane — it is
  listed here only because it has been open long enough to read as blocked.
- **A second machine (BP11).** The Steam rung needs two. Not owned today; acquiring it is a
  founder action with lead time, so it is worth starting before BP11 is otherwise ready.
- **`Tools/audit_blueprints/`** — written, never run, agent stopped mid-packet. **Unreviewed; do
  not trust it.** Until it works, R26 is enforced by goodwill.

---

## 7. The mismatch this exercise exposed — filed, not fixed

**The claim model is per-ticket; the editor is a per-machine resource spanning six tickets.**
`.claude/active-packet.json` names one `ticket`. But an OPEN window naturally serves BP16, BP08,
BP10 and BP01 step 5 together, and a CLOSED window serves R26, BP01 3b, BP13 6, BP03 2 and BP07
together. Executed one claim at a time, each ticket pays its own editor restart, and the board's
own guidance ("claim, execute, release") schedules the machine badly.

Two candidate resolutions, **for the founder, not for a lead to pick**:

- **(a) Window claims.** A claim may name a *set* of tickets sharing one lock window, with the
  union of their `owner_path`s. Cheapest — `guard_laws.py` already reads `owner_path` as a list,
  so only the `ticket` field's meaning changes. The handoff's mention of a past *"multi-ticket
  parallel pod"* suggests this was already reached for once.
- **(b) Keep one-ticket claims, add an explicit lock holder.** A separate `.claude/lock-mode.json`
  recording `OPEN`/`CLOSED` and the owning session, checked by the ladder scripts and by anything
  driving the editor. More honest about what is actually exclusive, and more moving parts.

Filed under law 8: rulings are closed and this needs one, so it is written here as a decision
owed rather than as a decision taken.
