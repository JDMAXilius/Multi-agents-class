# BP16 step 1 — execution plan and work division

**Written 1 Aug 2026 by the non-driver terminal, PRE-CLAIM.** This file is *input to* step 1,
not step 1's deliverable. The deliverable is `SURFACE.md`, and the session that claims BP16
**owns it and this folder** from the moment its claim lands. Nothing here is a finding; findings
go in the ticket Log.

Read `RESEARCH.md` first. It is desk research and **where it and a running editor disagree, the
editor is right.**

---

## 0. The two constraints that shape everything below

**A. One working tree, two terminals.** `git worktree list` returns a single tree. Both sessions
share the same files on disk — there is no branch isolation and no per-session staging area.

> **Therefore: never `git add -A` or `git commit -a` while another session is live.**
> Stage explicit paths only. The two sessions own disjoint paths precisely so this works.

**B. R29 — one editor, one driver.** The MCP terminal owns the editor for the duration. No other
session touches it, and **nobody builds while it is open** (R29.3). The board's three highest
-priority items — the input generator, the R26 rename, the CSV reimport — are all headless
`-run=pythonscript` and will refuse on their own R21 guard until the editor closes. That is
scheduling, not breakage.

---

## 1. Work division

| | **T2 — MCP terminal** (editor-live, the driver) | **T1 — this terminal** (no editor) | **T3 — cloud session** (files-only, low priority) |
|---|---|---|---|
| Crew role | **builder** | **lead + critic** | doc prep |
| Claims | **BP16** (owns `Tools/ue_mcp/`) | nothing — must not co-claim BP16 | nothing |
| Writes | `SURFACE.md`, `.mcp.json` | ticket Logs only | ticket Logs only |
| Touches the editor | **yes, exclusively** | **never** (R29.2) | impossible |
| Runs builds/commandlets | no (R29.3, its own editor is open) | **only after T2 closes the editor** | no |

**The division rule that matters more than the table: the builder does not grade its own
output.** T2 enumerates the surface; **T1 runs step 5's critic questions against it.** A single
session doing both produces a `SURFACE.md` that agrees with itself and with nothing else.

### T2's job (blocking — everything else waits on it)
Steps 1a–1d below, then `SURFACE.md`, then push.

### T1's job (parallel, non-blocking)
1. **Cross-check T2's output against disk** once `SURFACE.md` lands — the independent half of
   step 1. Concretely: if the MCP reports an asset inventory, diff it against
   `git ls-files "Content/**/*.uasset"`. A surface that cannot see an asset git tracks, or
   reports one git does not, is a finding.
2. **Draft step 2's ruling** — the (a)/(b)/(c) choice — as a proposal for the founder. It cannot
   be decided until `SURFACE.md` exists, but the argument for each option can be written now.
3. **Scope the `guard_laws.py` extension** proposed in `RESEARCH.md` §4 (match on tool *name*,
   `mcp__…__*`, gate mutating tools on the claim). **Scope only — do not implement it.**
   `.claude/hooks/` is outside BP16's `owner_path` (`Tools/ue_mcp/`, `docs/contracts/`), so
   implementing it is a **`contract_gap`**, filed in the ticket, not a "tiny fix" (law 5).

### T3's job (optional, genuinely low priority — do not block on it)
Reconcile `SURFACE.md` against `RESEARCH.md` §2 and mark every documented claim
**CONFIRMED / CONTRADICTED / UNTESTED**, then correct `RESEARCH.md`'s wrong lines in place. It is
low priority because it is bookkeeping: RESEARCH.md already says the editor wins, so nothing
downstream is blocked on the reconciliation. Worth doing only because §2's setup table will
otherwise be cited later by someone who does not know it was never verified.

---

## 2. T2's probe sequence

**Standing rule for this entire step: NO MUTATING CALL.** Step 2's law-7 ruling does not exist
yet, and BP16 exists precisely to stop an asset landing before it does. Mutating tools are
enumerated **from their schemas**, never by invocation. Their refusal behaviour is recorded as
`UNKNOWN — deferred to step 4`, which is honest; guessing is not.

### 1a. The inventory — and the one string the whole enforcement fix depends on

List every MCP tool actually resolvable **in the session's tool list**. Per this ticket's
Kickoff: *a tool that does not appear in the session's tool list does not exist*, regardless of
what the plugin claims to install.

> **Record the literal tool-name prefix, exactly, including separators** — e.g.
> `mcp__unreal__spawn_actor` vs `mcp__unreal-mcp__spawn_actor` vs something else entirely.
> `RESEARCH.md` §4's proposed hook fix matches on this prefix. **It is a guess right now.** If
> the real prefix differs by one character the fix silently matches nothing, which is this
> board's recurring failure mode — a protection that reads as enforced and is not. Copy it from
> the tool list; do not retype it from memory.

Also capture: editor version, plugin version, and whether the server was auto-started or started
by hand.

### 1b. Schemas
For each tool: full argument list (name, type, required/optional, default) and return shape.
Prefer the session's own schema dump over documentation prose.

### 1c. Read-only / mutating classification
Classify from the schema. **When ambiguous, mark `UNKNOWN` — do not resolve it by calling.**
An "inspect" tool with a `save` flag is mutating.

### 1d. Behaviour probes — read-only only

Each probe names a concrete target that is known-true on disk today, so a surprising answer is
unambiguous:

| # | Probe | Target | Why this one |
|---|---|---|---|
| P1 | Query an asset that **exists** | `/Game/Core/GM_BR` | Tracked at `Content/Core/GM_BR.uasset`, and `DefaultEngine.ini:30` points `GlobalDefaultGameMode` at `GM_BR.GM_BR_C`. Records the success shape. |
| P2 | Query an asset that **does not exist** | `/Game/Core/BP_BRGameMode` | The R26 *target* name — provably absent (the rename has never run). **This is the cheapest "what it refuses" datapoint on the board**: a real refusal, zero risk, and it doubles as proof the rename is still un-run. |
| P3 | Query a **C++** class, not an asset | `ABRGameMode` | Does the surface see native classes, or only content? Decides whether the MCP can ever verify an R26 parent-class condition. |
| P4 | Screenshot / viewport | whatever level is open | Note **which level is loaded** — the arena `.umap` has never been built, so it should not be one. |
| P5 | Any tool needing a **selection**, with nothing selected | — | Refusal shape for the empty-precondition case. |
| P6 | Anything reporting project path / engine version | — | T1 cross-checks against disk. Cheap independent-corroboration anchor. |

If a listed probe's tool does not exist, that is itself the finding — record
`no tool for this` rather than substituting a different probe.

### 1e. `.mcp.json`
Run `ModelContextProtocol.GenerateClientConfig ClaudeCode` and record what it wrote. **Do not
commit it yet** — commit-vs-gitignore is a lead call (it points at loopback, so committing is
harmless and saves the next machine a step, but that is the founder's to decide). Note it in the
Log and leave the file untracked.

---

## 3. `SURFACE.md` skeleton — copy this out when you claim

```markdown
# UE 5.8 MCP — the VERIFIED surface

Enumerated against a live editor on <date>, <machine>. Editor <version>, plugin <version>.
Supersedes RESEARCH.md §2 wherever they disagree.

Method: tools listed from the session's own tool list; schemas from the session; behaviour
from the read-only probes in STEP1-PLAN.md §1d. NO MUTATING TOOL WAS CALLED — see §4.

## 0. Connection
- Tool-name prefix (verbatim): `...`
- Endpoint: ... | Auto-start: yes/no | .mcp.json written to: ...

## 1. Tools
| Tool (verbatim) | Args | Returns | Read-only / Mutating / UNKNOWN | Refuses on |
|---|---|---|---|---|

## 2. Probe results
| # | Probe | Target | Result (verbatim) |
|---|---|---|---|

## 3. Documented-but-absent
Anything RESEARCH.md §2 listed that has no tool here. Absence is a result.

## 4. Deliberately untested
Every mutating tool, and why: step 2's law-7 ruling does not exist yet. Deferred to step 4.

## 5. Consequences for step 2
Which of (a) executor-only / (b) recorded transcript / (c) exploratory-only the surface makes
cheap, and which it makes impossible. Evidence, not preference — the lead decides, not this file.
```

---

## 4. Definition of done for step 1

- [ ] Every tool in the session's list appears in `SURFACE.md` §1 with a classification
- [ ] The tool-name prefix is recorded **verbatim** (§1a) — the hook fix depends on it
- [ ] All six probes in §1d have a result, or an explicit `no tool for this`
- [ ] `§3 Documented-but-absent` is filled in, even if empty — an empty section is a claim
- [ ] **No mutating tool was called**, and §4 says so
- [ ] T1's disk cross-check has run against the finished file (the independent half)
