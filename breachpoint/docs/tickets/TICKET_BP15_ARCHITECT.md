# TICKET — BP15: The Architect — score what to build next, then build it

> STATUS: open — cut 31 Jul 2026 from Class 07 (Autonomous Agency). **Mixed-machine:**
> steps 1–3 and 7 run anywhere (no engine); steps 4–6 need the Windows box with UE 5.8,
> a `.uproject`, and build targets.

The crew executes tickets it is handed. **Nobody scores what to build next — you do, by hand.**
This ticket adds that layer: a deterministic perception + scoring pass in front of the existing
gates, so the crew can be pointed at the architecture instead of at a ticket. Founder laws bind
it unchanged — the builder still writes only inside `owner_path`, the critic still refutes, the
verifier still cannot write.

**The law this ticket adds: an LLM never chooses the unit.** Scoring is deterministic Python
with printed terms; the model is only ever handed a unit the score already picked. A ranking
that came out of a model is a ranking nobody can audit — and deterministic code is smaller than
the prompt that would replace it.

**The second law: the architect never writes game code.** It writes inside `Tools/architect/`
and regenerates `docs/BUILD-STATE.md`. Everything under `Source/` goes through a builder packet
and the gates, exactly as today.

**Ordering law:** step 1 gates 2, step 2 gates 3, steps 1–3 gate 4. BP01 step 1 (project
skeleton) and BP00 step 1 (ladder wrappers) gate steps 4–6. BP14 steps 1–2 supply the code-job
path; if BP14 has not run, step 4 carries a minimal path of its own and says so in the Log.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: **files-only** for steps 1–3 (the ticket states they need no engine) → claimable from Context A or B; step 4+ is engine-installed
- `python3 Tools/data-crew/run_crew.py` (replay) exits 0 from the game repo (BP14 step 1)
- `BREACHPOINT-ARCHITECTURE.md` §3 is present and its per-folder counts parse
- owner_path: `Tools/architect/` (generated output `docs/BUILD-STATE.md` is the one exception,
  declared in the packet)

## Steps (in order)

1. **Perception — deterministic, zero API calls.** `architect.py --scan`. Parse ARCHITECTURE
   §3's tables into the expected unit list (name, folder, one-line spec, owning discipline from
   §9's owner-path map). Scan `Source/Breachpoint/` and classify each unit **BUILT / STUB /
   MISSING** (STUB = header exists with no `.cpp` or an empty body). Emit
   `Tools/architect/state/perception.json`.
   *Self-check that must pass:* the per-folder counts match §3's printed numbers
   (Core 2 · Input 2 · AbilitySystem 11 · Character 2 · Weapons 3 · Match 4 · AI 6 ·
   Online 3 · UI 4 · Telemetry 2 · Data 1 · Tests 3) — these sum to **43** — plus the one
   Phase-2 reserved unit (`BRGameLiftLifecycle`, §3.8) for the **44** of §4's budget.
   A mismatch exits nonzero — the doc and the scanner disagree, and that is a finding, not a
   rounding error.

   > **Corrected 31 Jul 2026.** This self-check previously demanded 44 units *and* per-folder
   > counts that sum to 43, without saying where the 44th came from — it could not pass as
   > written. §3's composition table now states it. Two exclusions the parser must handle,
   > both real C++ under `Source/` that the per-folder counts do not cover:
   > - **The six generic GE classes** (`GE_Damage`, `GE_Regen`, `GE_Cooldown`, `GE_InitStats`,
   >   `GE_RecentDamage`, `GE_Death`) in `AbilitySystem/Effects/`. Counting files on disk
   >   finds 17 in `AbilitySystem/` against an expected 11. They are a named library under
   >   R18, not numbered units — exclude them and say so in the run log.
   > - **`BRGameLiftLifecycle`** is expected MISSING for the entire slice. Its GDD-tier term
   >   must rank it last: a perpetually-MISSING unit that scored high would be selected to
   >   build, which inverts the ledger's intent.
   >
   > Also fixed: §3's ASCII tree printed `AI/ (4)`, stale from v1 — §3.7 lists six units and
   > says so (*"v2: was 4 units — the GOAP layer adds `BRBotBrain` + `BRBotFacts`"*). A parser
   > reading the tree and a parser reading the section headers would have disagreed by two.
   Owner: **builder**. Contracts: `data-and-assets.md`.
2. **Utility scoring — deterministic, zero API calls.** `priority_score(unit)` over four printed
   terms: **dependency depth** (ticket DAG BP00–BP14 + declared include edges), **blocker count**
   (how many units wait on this one), **GDD tier** (vertical-slice critical vs. Phase-2), and
   **current state** (MISSING > STUB > BUILT). Ties break on lowest ticket number — never on a
   model's preference. Print the ranked table with every term visible, not just the total.
   Owner: **builder**. Contracts: `data-and-assets.md`.
3. **Blackboard — written before anything else happens.** For the top-ranked unit, write
   `Tools/architect/blackboard/<UTC-date>-<unit>.md` containing: **what it scored** (the full
   ranked table with terms), **what it issued** (the verbatim prompt the builder will receive,
   including which contracts were attached), **what it will generate** (target paths +
   `owner_path`). If the blackboard write fails, the run aborts — nothing reaches the codebase
   unlogged. Owner: **builder**.
4. **Generate one unit through the real gates** *(Windows / UE 5.8)*. Hand the top-ranked unit
   to the **builder** as a packet: gate A (**diff confined to `owner_path`** — a diff outside is
   auto-rejected) → **critic** REFUTER on the generated code → **verifier** runs rung 1
   (`Tools/run-ubt.ps1`, all three targets, clean) and rung 2 (`Tools/run-specs.ps1`). Rung 4 is
   reported **BLOCKED — not applicable** with the reason if the unit has no networked surface;
   it is never silently skipped. Expected first unit: **`BRGA_Sprint` + `BRAttributeSet`** —
   GDD-sourced, visible in PIE, needs no art. If the score picks otherwise, the score wins and
   the Log records the surprise.
   Owner: **builder**, **verifier** runs the rungs, **sim-builder** consults if the unit is sim math.
5. **Regenerate `docs/BUILD-STATE.md`** from `perception.json` + this run: **BUILT** (units, with
   the commit that landed each) · **DECISIONS** (why this unit was picked — the score terms, not
   prose) · **NEXT** (the ranked remainder and known blockers). Header must read *generated — do
   not edit; edit the architecture or the scanner*. A hand-edited state file is a state file that
   lies. Owner: **builder**.
6. **Adversarial review of the architect itself** (REFUTER, `critic`). Each answer needs
   input → wrong outcome, not vibes:
   - Can a unit be landed that the score did not pick?
   - Can the blackboard be written *after* the generated file?
   - Can any score term be made to depend on model output?
   - Can `BUILD-STATE.md` report BUILT for a unit that is only a STUB?
   - Can `architect.py` be made to write inside `Source/`?
   Owner: **critic**.
7. **Assignment #5 packaging** (ELVTR S07 — no engine needed). A ReadMe answering the three
   questions the rubric asks: what feature did the agent build, **why did it pick that one**
   (paste the score terms — this is the whole point), and what did you change before accepting
   it into the game. Owner: **lead**.

## Done when

- [ ] `python3 Tools/architect/architect.py --scan` reproduces every per-folder count from §3
      (43 in-slice units) plus the one Phase-2 reserved unit = 44, matching §4's composition
      table; exits nonzero on any mismatch. The six generic GE classes and
      `BRGameLiftLifecycle` are excluded/expected-MISSING per §4, and the run log says so
- [ ] Ranked table prints all four score terms; the same input produces the same order twice
      (run twice, diff is empty)
- [ ] Steps 1–2 make **zero API calls**, proven by the run log
- [ ] The blackboard file exists and its mtime **predates** the generated source file's
- [ ] The chosen unit compiles on rung 1 (all three targets) and its spec passes rung 2, both
      verbatim in this Log; rung 4 either green or BLOCKED with a reason
- [ ] The feature is visible in PIE (screenshot or spec assertion named in the Log)
- [ ] `docs/BUILD-STATE.md` regenerates byte-identically from a clean checkout
- [ ] A diff outside `owner_path` is rejected by gate A — proven once with a real diff
- [ ] Critic findings from step 6 addressed or explicitly waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: **builder** (architect.py + the generated unit) · **critic** (refutes the architect,
  then the code) · **verifier** (runs rungs, never fixes) · **sim-builder** (consults on sim math).
- Binary files this ticket OWNS: **none.**
- **Out of scope:** letting a model score, reorder, or break ties; a general-purpose
  codebase-perception engine (ARCHITECTURE §3 *is* our manifest — parse it, don't infer it);
  `architect.py` writing anything under `Source/`; replacing the ticket board; CI.
- Relationship to BP14: BP14 makes the *pipeline* able to land code. BP15 decides *what* it
  lands. BP14's steps 1–2 are the preferred substrate; BP15 does not wait on BP14's steps 3–5.
- **Course deadline note:** Assignment #5's slide states two dates — 13 August in the header,
  "Before S09 (6 August 11:59 ET)" in the table. Plan against **6 August** until the instructor
  confirms.

## Log

(append findings here, dated, newest last — this is what the next session reads)

**31 Jul 2026 — step 1's self-check could not pass as written. Fixed before claim.**

Found while reading the board, not while running it — no engine needed, and it would have
red-lighted this ticket's first step.

*Defect 1 — the self-check contradicted itself.* It demanded the parse yield **44** units
*and* that per-folder counts match §3's printed numbers. Those printed numbers
(2+2+11+2+3+4+6+3+4+2+1+3) sum to **43**. Nothing said where the 44th came from, so any
faithful implementation exits nonzero on its own acceptance criterion.

*Resolution:* 44 is correct as a **budget** — §4 reaches it as 42 after v2 consolidation plus
`BRBotBrain` + `BRBotFacts`, and the 44th is §3.8's *"reserved, Phase 2"* `BRGameLiftLifecycle`.
The number stays (it appears in §4, `README.md`, and `CLASS-07-AUTONOMOUS-AGENCY.md`); the
*composition* is now printed in §4 as a table, and step 1 cites it. Changing 44→43 would have
been the smaller edit and the wrong one — it would desync three other documents.

*Defect 2 — §3's ASCII tree was stale.* It printed `AI/ (4)` while §3.7's header printed 6 and
its body listed six units, with a parenthetical stating the change (*"v2: was 4 units — the
GOAP layer adds `BRBotBrain` + `BRBotFacts`"*). The tree was never updated. A parser reading
the tree and one reading the section headers disagree by two — and §3's tree is the more
obvious thing to parse. Fixed to `(6)`.

*Defect 3 — two unit-manifest ambiguities that would fail the per-folder assertion.* Both are
real C++ under `Source/` that §3's counts exclude:
- The **six generic GE classes** live in `AbilitySystem/Effects/` under R18, but §3.3's count
  of 11 covers only its file table. A scanner counting files on disk finds **17** in
  `AbilitySystem/` against an expected 11. They are a named library, not numbered units.
- **`BRGameLiftLifecycle`** is expected MISSING for the whole slice. It must carry Phase-2
  GDD tier so the score ranks it last — otherwise a permanently-MISSING unit scores high on
  the "current state" term and gets *selected to build*, which inverts the ledger.

Both are now stated in §4 and in step 1, and the run log must record the exclusions rather
than silently applying them (`game-lead`: a silent cap reads as "covered everything").

*Not changed, flagged for the builder:* §3's counts are the manifest per this ticket's own
out-of-scope line (*"ARCHITECTURE §3 is our manifest — parse it, don't infer it"*). Whether
the six GE classes should eventually become numbered units is a founder call, not a scanner
detail — if BP02 step 4 hits the `UGameplayEffectComponent` problem and files its
`contract_gap` against R18, that call gets made there, and this count moves with it.
