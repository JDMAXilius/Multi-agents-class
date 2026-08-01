# TICKET — BP15: The Architect — score what to build next, then build it

> STATUS: **steps 1-3 DONE 1 Aug 2026 (terminal, Windows) — steps 4-6 OPEN, claim released.**
> `Tools/architect/` holds architect.py + test_selfcheck.py + state + one blackboard; 3 of 10
> Done-when boxes checked. Steps 4-6 are `engine-installed` and were NOT started. See the Log's
> 1 Aug entry — the score was wrong twice before it was right, and both are recorded.

> STATUS: **open — claim RELEASED 1 Aug 2026, nothing landed.** Claimed by a cloud session for
> steps 1–3, then released un-started when the founder moved to the terminal. `Tools/architect/`
> is empty and `.claude/active-packet.json` is deleted. **The Kickoff now passes** — pick it up
> clean. See the Log for the two spec corrections made before the release; they are the useful
> part of that attempt.

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
   > - **The generic GE classes** in `AbilitySystem/Effects/BRGameplayEffects.h`. They are a
   >   named library under R18, not numbered units — exclude them and say so in the run log.
   >   **Corrected 1 Aug 2026: there are SEVEN, not six, and they share ONE header.**
   >   `UBRGE_RecentDamage`, `UBRGE_Damage`, `UBRGE_Regen`, `UBRGE_Cooldown`, `UBRGE_InitStats`,
   >   `UBRGE_Death`, `UBRGE_ShieldsBroken` (`BRGameplayEffects.h:77,103,157,179,211,239,263`).
   >   The old wording also said "counting files on disk finds 17 in `AbilitySystem/`" — the
   >   folder holds **8** headers. **So the scanner must count CLASSES, not files**: one header
   >   declaring seven classes is why a file-count premise was wrong twice over, and it is the
   >   single most useful thing step 1 can learn before writing a line of the parser.
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

- [x] `python3 Tools/architect/architect.py --scan` reproduces every per-folder count from §3
      (43 in-slice units) plus the one Phase-2 reserved unit = 44, matching §4's composition
      table; exits nonzero on any mismatch. The **seven** generic GE classes and
      `BRGameLiftLifecycle` are excluded/expected-MISSING per §4, and the run log says so
- [x] Ranked table prints all four score terms; the same input produces the same order twice
      (run twice, diff is empty)
- [x] Steps 1–2 make **zero API calls**, proven by the run log
- [ ] The blackboard file exists and its mtime **predates** the generated source file's
- [ ] The chosen unit compiles on rung 1 (all three targets) and its spec passes rung 2, both
      verbatim in this Log; rung 4 either green or BLOCKED with a reason
- [ ] The feature is visible in PIE (screenshot or spec assertion named in the Log)
- [x] `docs/BUILD-STATE.md` regenerates byte-identically from a clean checkout
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
- The **seven generic GE classes** live in `AbilitySystem/Effects/BRGameplayEffects.h` under
  R18, but §3.3's count of 11 covers only its file table. They are a named library, not
  numbered units. *Corrected 1 Aug 2026 — this read "six … a scanner counting files on disk
  finds 17"; both numbers were wrong. Seven classes, and `AbilitySystem/` holds 8 headers, not
  17 files. The exclusion is by class name, not by file.*
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

**1 Aug 2026 — Kickoff is now satisfiable, and the GE exclusion rested on two wrong numbers.**
(Cloud session, Context A. Not a claim — this is the lead correcting the packet's premises
before a builder spends its first hour on them.)

*The gate.* This ticket's Kickoff requires `python3 Tools/data-crew/run_crew.py` (replay) to
exit 0 from the game repo, citing "(BP14 step 1)". **That was unsatisfiable until today** —
BP14's own Kickoff gated BP14 on the same line, so the condition depended on work that was
itself gated behind the condition. BP14 step 1 has now been executed (see its Log): the crew
lives at `Tools/data-crew/`, `find_agents_dir()` resolves the game repo's `.claude/agents/`,
and replay exits 0. **This gate now passes.**

*The premise defect.* Step 1's self-check excludes "the six generic GE classes" from the unit
count and justifies it with *"counting files on disk finds 17 in `AbilitySystem/` against an
expected 11."* Both numbers are wrong against HEAD:

- There are **seven** GE classes, not six — the sixth-and-seventh are `UBRGE_Death` **and
  `UBRGE_ShieldsBroken`** (`AbilitySystem/Effects/BRGameplayEffects.h:77,103,157,179,211,239,263`).
- `AbilitySystem/` holds **8** headers, not 17 files. All seven GE classes are declared in a
  **single** header.

*Why this is worth a Log entry and not a silent edit:* the scanner "exits nonzero on any
mismatch," so it is a machine that will confidently report a defect that isn't there. And the
two errors have the same root — **the exclusion was conceived as a file-count adjustment when
it is a class-count adjustment.** A UE header routinely declares several `UCLASS`es; any unit
scanner for this codebase must parse declarations, not `ls`. That is the reusable finding.

Corrected in the Kickoff self-check, the Done-when line, and Defect 3 above. `BRCombatCurves`
is a second, separate case the audit flagged (present on disk, absent from `ARCHITECTURE §3`) —
**left for the packet**, because deciding whether it is a numbered unit is the ticket's own job,
not a correction the lead should make from outside it.

*Still true:* steps 1–3 need no engine. With the gate now passing, this is **the most
claimable ticket on the board from a cloud or phone session.**

**1 Aug 2026 — claimed, then released with nothing landed. Recorded so the next session does
not wonder.** A cloud session claimed steps 1–3 and dispatched a builder; the founder moved to
the terminal before it wrote a file, so the agent was stopped and the claim released. **No
partial work exists** — `Tools/architect/` was never created, and `.claude/active-packet.json`
is deleted, so law-5 confinement is off and the next claim starts clean.

What survives from the attempt is in the entry above: the Kickoff gate now passes (BP14 step 1
ran), and the GE-class exclusion was corrected from six-classes/17-files to seven-classes in
one header. **Steps 1–3 still need no engine, so this remains the cheapest ticket on the board**
— but running it on the terminal is strictly better, because step 4 continues straight on from
step 3 there instead of handing off across machines.

---

**1 Aug 2026 (terminal, Windows) — steps 1–3 LANDED. Three of ten boxes checked. The score was
wrong twice before it was right, and both wrongnesses are the useful part.**

`Tools/architect/` now holds `architect.py` (`--scan` / `--rank` / `--blackboard` / `--all`),
`test_selfcheck.py`, `state/perception.json`, `state/ranking.json`, and one blackboard.

*Step 1 result.* **The self-check PASSES** — all twelve §3 folders are internally consistent,
summing to 43 in-slice + 1 reserved = **44**, matching §4's composition table. 7 `UBRGE_*`
classes found in one header (asserted, not assumed — a drift there now fails the run).
Disk: **BUILT 31 · STUB 3 · MISSING 10**.

> **A distinction worth keeping, because HANDOFF predicted this ticket would fail here.** The
> handoff said *"§3 is stale against disk (`UI/` 7 vs 4, `BRCombatCurves` unlisted, `Tests/`
> empty vs 3)"* and expected step 1 to surface it as a **count mismatch**. It is not one. §3 is
> *self*-consistent; it is *disk* that disagrees with it. So the staleness surfaces in the
> **UNDECLARED** report — `AbilitySystem/: BRCombatCurves` · `UI/: BRRootLayout, BRUISettings,
> BRUITypes` (4 declared + 3 undeclared = the 7 the handoff counted) — and `Tests/`'s three
> specs come back **MISSING**, which is a *state*, not a manifest defect. Conflating "the doc
> disagrees with itself" and "the doc disagrees with disk" would have made the scanner exit
> nonzero on a healthy manifest. They are separate outputs for that reason.

*Step 2 — the score was wrong twice.* Both drafts ran, printed, and looked authoritative:

1. **It ranked a BUILT unit first.** Terms were depth + blockers + tier + state with state at
   2/1/0; the blocker term reaches 35, so it swamped state entirely and `BRCharacter` (BUILT)
   came top. **A "what to build next" scorer selected something already built.** Fixed by making
   state a *gate* rather than a nudge (MISSING 100 · STUB 50 · BUILT −1000), the same mechanism
   the tier term already used to push Phase-2 last.
2. **Then it picked `BRGA_Grapple`.** Units were mapped to tickets by *folder*, but tickets cut
   across folders: all six abilities live in `AbilitySystem/`, so all six were attributed to
   BP02. `BRGA_Grapple` is **BP06**'s — gated behind BP05, gated behind BP02 — so the score was
   ranking a unit that **cannot legally be started**, and it picked the single hardest one in
   the codebase (THE netcode packet, critic REFUTER gate). Fixed with a `UNIT_TICKET` override
   map, each entry justified by the ticket whose *title* names the unit's subject.

*Step 2 result, after both fixes:* **`BRGA_WeaponFire` (BP03, MISSING, total 106)** —
depth 2 · blockers 4 · tier 0 · state 100. Ties break on lowest ticket number, then unit name;
`BRGA_WeaponUtility` ties it at 106 and is the same BP03 packet.

> **The result worth reporting to the founder:** the ticket predicted the first unit would be
> `BRGA_Sprint` + `BRAttributeSet` and said *"if the score picks otherwise, the score wins and
> the Log records the surprise."* Both of those are now **BUILT**, so neither is selectable —
> and the score independently landed on **the fire path, which is exactly what HANDOFF's
> "restart BP03 step 2" says to do next.** A deterministic scorer with no knowledge of the
> handoff reproduced the human board's next move. That is the strongest evidence this ticket
> can produce that the scoring is measuring something real, and it cost zero API calls.

*On `BRGameLiftLifecycle`:* it ranks **last among selectable candidates** (total 4, below all
three STUBs). It sorts above the BUILT units only because BUILT is a *stronger* exclusion
(−1000); neither class is selectable. The ledger's intent — never selected to build — holds.
Flagged rather than tuned: if the founder wants it literally last, the tier weight moves.

*Step 3.* `blackboard/2026-08-01-BRGA_WeaponFire.md` — full ranked table with every term, the
verbatim builder prompt, target paths, `owner_path`, and the rungs owed (incl. R30's 4a/4b
split). **Two earlier blackboards were deleted, deliberately and recorded here**
(`BRCharacter`, `BRGA_Grapple`): a blackboard is an *authorisation* record, and those two
authorised units the corrected score does not select. Keeping them would have left the
directory claiming three units were authorised. The Log is the record; the blackboard is the
authorisation.

*Verification (this session, this machine):*

| Check | Result |
|---|---|
| Self-check on the real manifest | **PASS** — 43 + 1 = 44 |
| `test_selfcheck.py` red-then-green | **5/5** — 1 control ACCEPTs, 4 corruptions REJECT (exit 2) |
| Determinism (run twice, diff) | `perception.json`, `ranking.json` **and the full run log** byte-identical |
| Zero API calls | no network import — `grep -E "^\s*(import\|from)\s+(requests\|urllib\|http\|socket\|anthropic\|openai)"` returns nothing |
| Law-5 confinement | `git status` after the packet shows **only** `Tools/architect/` + the claim file |

*Why `test_selfcheck.py` exists at all:* the Done-when says the scanner "exits nonzero on any
mismatch," and this session had already found three mechanisms that read as enforced and were
not. A self-check only ever observed **passing** is not evidence. Driving the real parser over
four corrupted manifests **found a real defect in `architect.py`** — `Path.relative_to` raises
on a non-subpath, so a cosmetic log line was load-bearing and crashed the control case. A
formatting helper that can fail a run is exactly the kind of thing only a rejecting case finds.

*Two things NOT done, said plainly:*
- **The blackboard-predates-source box stays unchecked.** No source has been generated (step 4
  is `engine-installed`), so half the box is unprovable today. Half a box is not a box.
- **Steps 4–6 untouched.** They need the engine, and R29.3 forbids building while the MCP
  session holds the editor.

**CONTRACT_GAP (filed, not fixed — `BREACHPOINT-ARCHITECTURE.md` is outside this packet's
`owner_path`).** §4's exclusion #1 still reads *"The six generic GE classes"* and *"a scanner
counting files on disk finds 17 in `AbilitySystem/`"*. Both numbers are wrong at HEAD — there
are **seven** classes and `AbilitySystem/` holds **8** headers — and §3.3's GE table likewise
prints *"(6 total"* and lists six, omitting `UBRGE_ShieldsBroken`. This ticket's Log corrected
the numbers on **1 Aug in the ticket**; the correction never reached the architecture document,
which is the file the scanner actually parses. `architect.py` does not read those prose numbers
(it asserts the class count against the header directly), so nothing is broken today — but the
manifest still tells the next reader six. Also unlisted in §3.9/§3.3: `BRCombatCurves`,
`BRRootLayout`, `BRUISettings`, `BRUITypes`. Whether those become numbered units is a founder
call, per this ticket's own out-of-scope line.

**Harness note.** This packet ran from a session rooted one level above the game repo, so
`guard_laws.py` was inert and law 5 was **not** mechanically enforced (see BP01's 1 Aug
amendment). Confinement was instead proven after the fact by `git status`, which shows only
`Tools/architect/` and the claim file. Stated because an unenforced law that happened to be
obeyed is not the same as an enforced one, and the next reader should know which this was.

**1 Aug 2026 — CONTRACT_GAP RESOLVED by founder direction: the GE count is seven, in five
documents.** The gap filed in the entry above was authorised and fixed.

Scope note, because it exceeded what was asked: the instruction named
`BREACHPOINT-ARCHITECTURE.md`, but the same wrong number was in **five** files. Fixing only the
one named would have left four documents contradicting it — which is the exact failure this
ticket's finding was about. All five:

| File | Was | Now |
|---|---|---|
| `BREACHPOINT-ARCHITECTURE.md` §3.3 | "(6 total" + a 6-row table | 7 total, `GE_ShieldsBroken` row added, one-header fact stated |
| `BREACHPOINT-ARCHITECTURE.md` §4 | "six generic GE classes" / "finds 17 in `AbilitySystem/`" | seven, named; **exclude by CLASS NAME, not by file**; 8 headers |
| `BREACHPOINT-AUTHORING-MATRIX.md` | "the six generic GEs … six C++ classes" | seven, + the one-header location |
| `docs/contracts/data-and-assets.md` | "the six generic GEs are C++ classes" | seven |
| `.claude/skills/gas-purity/SKILL.md` | "R18: the six generic GEs" | seven |

§3.3 now also records **why** there are seven, from the header's own comment: `GE_ShieldsBroken`
was added by BP02 steps 1–2 because `State.Shields.Broken` was declared in `BRGameplayTags` and
§3.1 with **nothing applying it**, and purity law 5 forbids applying a State tag by hand — so the
tag was either dead or it needed an effect. Recording the reason is the point: a bare "7" invites
the next reader to correct it back.

§4's paragraph was the more important fix. It carried a wrong *premise*, not just a wrong count:
it framed the exclusion as a file-count adjustment ("a scanner counting files on disk finds 17").
It is a **class-count** adjustment — a UE header routinely declares several `UCLASS`es, and all
seven live in one. A parser adjusting file counts has already lost.

*Regression check after editing the manifest the scanner parses:*

| Check | Result |
|---|---|
| `architect.py --all` | **exit 0**, self-check PASS, 43 + 1 = 44 (unchanged) |
| Top-ranked unit | `BRGA_WeaponFire` (BP03, MISSING, 106) — **unchanged** |
| `test_selfcheck.py` | **5/5** — control still ACCEPTs, 4 corruptions still REJECT |

Unchanged is the result worth stating: the scanner asserts the GE class count against the header
rather than reading §4's prose, so correcting the prose moved no number. That is the design
working — **the doc and the code now agree, and they agree because the code never trusted the
doc.**

---

**1 Aug 2026 — STEP 4 ATTEMPTED with `BRGA_WeaponFire`. STOPPED at law 5 before writing a line
of C++, and separately BLOCKED on the rungs. Zero Done-when boxes added.**

This is a real step-4 result, not a deferral — but it is **not** a completed step 4, and the
boxes stay unchecked. What follows is what the pipeline actually did.

*The architect's half worked.* Step 3's blackboard (`blackboard/2026-08-01-BRGA_WeaponFire.md`)
authorised the unit before anything else happened, exactly as designed. The builder packet then
opened `BREACHPOINT-ARCHITECTURE.md` §3.3's spec for the unit and stopped.

**CONTRACT_GAP — three of them, none resolvable inside the packet's `owner_path`.** Verified by
execution, not by reading:

| # | What is missing | Evidence | Owner |
|---|---|---|---|
| 1 | `Ability.Weapon.Fire` and `GameplayCue.Weapon.{AR,Magnum,Rocket}.Fire` are **not declared** | `BRGameplayTags.h` declares 31 tags; none of these. `InputTag.Fire`, `Damage.Kinetic`, `Damage.Headshot` **do** exist | **BP01** (`Core/`) |
| 2 | `FBRWeaponRow` has **no trace range and no spread** | Row has DamagePerShot, RPM, MagSize, ReserveMags, ReloadTime_s, HeadshotMult, ProjectileSpeed, SplashRadius_m, SplashDamage, EquipTime_s, MeshSoftPath. `grep -E "Range\|Spread"` over `BRDataRows.h` returns **one hit, inside an unrelated comment**. `CT_Combat` has 11 rows, none of them range or spread | **BP02** (`Data/`) + curator for values |
| 3 | `DT_Weapons.csv` has **no `AbilitySet` column** | header is `Name,DisplayName,FireMode,DamageDelivery,DamagePerShot,RPM,MagSize,ReserveMags,ReloadTime_s,HeadshotMult,ProjectileSpeed,SplashRadius_m,SplashDamage,EquipTime_s,MeshSoftPath,FireCueTag` | curator proposes → builder lands |

**Why each one stops the packet rather than slowing it:**

1. `BRGameplayTags.h`'s own header comment states the rule: *"Whoever needs `Ability.Weapon.Fire`
   or `GameplayCue.Weapon.Fire` must first get §3.1 amended with the enumeration."* The file
   anticipated this exact packet and pre-refused it. **`Core/` is BP01's `owner_path`, not this
   packet's.**
2. A hitscan ability with no range and no spread has to invent both. A literal cone angle or
   trace length in the ability body is a **law-3 violation** and is item four on `gas-purity`
   §9's self-check list (*"a literal gameplay number next to a gameplay noun"*) — it would be
   written specifically to be caught. There is no lawful value to read.
3. Even a perfect `BRGA_WeaponFire` would **never be granted to anyone**: equip has nothing to
   grant, so the step-4 Done-when *"the feature is visible in PIE"* is unreachable by
   construction, independent of the code.

> **`DT_Weapons.csv` already names three tags that no C++ declares** — its `FireCueTag` column
> holds `GameplayCue.Weapon.AR.Fire`, `.Magnum.Fire`, `.Rocket.Fire`. Data referencing symbols
> the code has never declared is the sharper half of gap 1, and it was **landed by the data crew
> on 29 Jul with verifier PASS** — because the verifier checked the CSV's *schema*, not whether
> the tags on the other end exist. A cross-artifact reference nothing validates.

*Rungs, reported per axis rather than skipped:*

| Rung | Status |
|---|---|
| 1 (UBT, three targets) | **BLOCKED** — a UE editor is live (`UnrealEditor` pid 44352, the MCP session). R29.3: an editor session and a build must not overlap |
| 2 (specs) | **BLOCKED** — same lock, and `Source/Breachpoint/Tests/` still holds only `.gitkeep` |
| 4a dedicated | **REQUIRED and not run.** This unit has a networked surface by definition — client TargetData that the server must re-validate |
| 4b listen + 1 remote | **REQUIRED** per R30 — a `LocalPredicted` fire ability is precisely a path that differs host-vs-remote. Also blocked upstream by BP00's Gauntlet/NuGet failure |

*Gate A:* trivially passed — **no diff was produced.** Nothing was written under `Source/`.
*Critic REFUTER:* not reached; there is no code to refute.

**The finding that matters for BP15 itself, and it is a good one.** The architect selected a
unit, authorised it on the blackboard, and the builder packet then **stopped at the law instead
of editing three other owners' files to unblock itself.** That is the behaviour law 5 exists to
produce, and step 6 asks *"can a unit be landed that the score did not pick?"* — the adjacent
question this run answers is *"can a unit the score DID pick force an out-of-owner-path edit?"*
**No.** It filed and stopped.

**The finding that matters for the board.** The score ranks by dependency depth, blocker count,
tier and state — **it has no term for whether a unit's inputs exist.** `BRGA_WeaponFire` is
correctly the most valuable next unit *and* is not startable today. Those are different
questions and the scorer only answers the first. A fifth term — *readiness*, computed from
declared-but-missing tags, row fields and columns — would have ranked it below
`BRGA_WeaponUtility` (which ties it at 106) and surfaced this before a builder was dispatched
rather than after. **Filed as a proposal, not implemented:** step 2's four terms are specified
by this ticket, and adding a fifth is a founder call, not a builder's.

*Unblocking sequence, cheapest first (all three are small):* BP01 declares the four tags →
curator proposes `Range_m` + `Spread_deg` + `AbilitySet` for `DT_Weapons.csv` and BP02 adds the
row fields → BP03 step 2 restarts with `BRGA_WeaponFire` and its inputs present. Then BP15
step 4 re-runs against a unit that can actually be built.

**1 Aug 2026 — CORRECTION to step 4's gap 1: it named the wrong owner, and one third of the
blocker was not a blocker.**

Step 4 filed `Ability.Weapon.Fire` + the three cue tags as a `contract_gap` against **BP01 /
`Core/`**. **R23 says otherwise:** `Ability.*` and `GameplayCue.*` are **OPEN families**, and the
packet authoring the ability declares its own tag under an **exact-file grant** to
`BRGameplayTags.h`/`.cpp`. No BP01 packet, no §3.1 amendment. The tags have now been declared
under a BP03 claim (see that ticket's Log) — 34 EXTERN/34 DEFINE balanced, and all three
`FireCueTag` values resolve.

*Why the packet got it wrong, which is worth more than the correction:* it read
`BRGameplayTags.h`'s **file-level comment**, which said the opposite of the `Ability.*` block
twelve lines below it. The comment predated R23 and was never updated. **The builder read the
top of the file and stopped there** — and a stop is exactly what law 5 asks for, so nothing
misbehaved; the packet was correctly obedient to a stale instruction. Fixed at the source.

**The generalisable finding, and it sharpens step 6's adversarial list.** Step 4 proved the
pipeline *stops* rather than editing shared code. It did **not** prove the pipeline stops for
**true** reasons. A `contract_gap` filed against a rule that no longer exists is indistinguishable
from a real one at the moment it is filed — both look like a well-behaved packet halting at a
boundary. **Obedience is not correctness**, and a board that measures only "did it stop?" cannot
tell a real blocker from a stale comment. Suggested addition to step 6's REFUTER questions:
*can the architect halt on a constraint that the rulings ledger has already lifted?* Today: yes,
and it did.

*Net effect on step 4:* one of three blockers dissolved. **Two remain and are real** — no trace
range or spread in `FBRWeaponRow` or `CT_Combat`, and no `AbilitySet` column in `DT_Weapons.csv`.
`BRGA_WeaponFire` is still not startable, and step 4's boxes stay unchecked.

---

**1 Aug 2026 — step 6 — adversarial review of the architect itself (critic, REFUTER).**

All five of step 6's questions answer **yes**, and so does the sixth the ticket does not ask.
Every answer below is an executed input → wrong outcome; where I could not construct a failing
input I say so and the answer is no. Nothing under `Tools/architect/` was modified — the probes
copy `architect.py`/`build_state.py` byte-identically into throwaway repos under the OS temp dir
and drive the real code. No build, UBT or editor command was run (R29.3; the editor is live).

Step 5 landed while this review was running, so `build_state.py` and `docs/BUILD-STATE.md` are in
scope and Q4 is answered against the real generator rather than against a plan.

| # | Question | Answer | Severity |
|---|---|---|---|
| 1 | Can a unit be landed that the score did not pick? | **yes** | high |
| 2 | Can the blackboard be written *after* the generated file? | **yes** | medium |
| 3 | Can any score term be made to depend on model output? | **yes — two of the four** | high |
| 4 | Can `BUILD-STATE.md` report BUILT for a unit that is only a STUB? | **yes** | high |
| 5 | Can `architect.py` be made to write inside `Source/`? | **yes** | high |
| 6 | Any *other* missing term producing a confidently wrong ranking? | **yes — two** | high |

---

**F1 (high) — a unit the score did not pick lands, and `BUILD-STATE.md` attests it.**

*Input.* In a copy of this repo: run `architect.py --all`; the blackboard authorises exactly
`2026-08-01-BRGA_WeaponFire.md`. Then land a **different** unit — `BRGA_Grapple.h/.cpp`, ranked
**#6**, never authorised — inside the owner path the blackboard itself prints
(`Source/Breachpoint/AbilitySystem/`). Re-run `architect.py --all` and `build_state.py`.

*Outcome.* Both exit 0. `docs/BUILD-STATE.md` lists `BRGA_Grapple` in the **BUILT** table
(`| \`BRGA_Grapple\` | AbilitySystem/ | BP06 | … |`, tally `BUILT 32`). The blackboard directory
still contains one file and it names `BRGA_WeaponFire`. **No output anywhere names the
discrepancy.** `grep -rn blackboard` over the repo returns `architect.py`, the blackboard file,
docs, and `build_state.py` — where the only use is `run_date()` reading the *filename* for a date
string. Nothing compares a landing to an authorisation.

Gate A cannot catch it either: `guard_laws.py` tests `rel.startswith(owner + "/")` over
`.claude/active-packet.json`'s `owner_path`, which names **folders**. The blackboard's own
`owner_path` row is `Source/Breachpoint/AbilitySystem/` — a scope containing six abilities. Any
of them passes gate A. Worse, the architect then *erases the trace*: `--scan` reclassifies the
unauthorised unit BUILT, state scores −1000, and it drops out of `NEXT` entirely.

*Fix.* `build_state.py` already reads `blackboard/`. Have it diff the BUILT set against the union
of units any blackboard ever authorised and print an **UNAUTHORISED** section — the same shape as
the existing UNDECLARED section, and for the same reason. Nonzero exit optional; the report is the
point.

---

**F2 (medium) — the blackboard's "written BEFORE generation" is unverifiable and trivially false.**

*Input.* Write `Source/.../BRGA_WeaponFire.cpp` first, sleep 1.1s, then run the advertised entry
point `python architect.py --blackboard` (docstring line 6), which re-reads `state/ranking.json`
off disk.

*Outcome.* exit 0. source mtime `12:19:21.98`, blackboard mtime `12:19:23.17` — the authorisation
postdates the artefact it authorises, and the file it wrote still says *"Written by … BEFORE any
generation. If this file is absent, nothing was authorised."* The only timestamp **inside** the
file is `2026-08-01` — day granularity, so file content can never establish ordering within a day.
`architect.py` records no run time, no source-tree hash, no lock, and never checks whether the
target files already exist.

The Done-when box *"the blackboard file exists and its mtime predates the generated source file's"*
therefore rests on mtime alone, and mtime is **not append-only**: the filename is
`{date}-{unit}.md`, so a second `--blackboard` run the same day silently overwrites the first and
resets the mtime forward. This Log already records that two blackboards were deleted by hand — the
directory is fully mutable, unordered and unsigned.

Live and adjacent, now that BP17 (session bus) has landed: two terminals running `--blackboard` on
the same day for the same unit write the **same filename**, and the second erases the first
session's authorisation record with no trace.

*Fix.* Write the blackboard first-wins (`open(..., "x")` — refuse if it exists), stamp the file
with a full UTC timestamp and the `git rev-parse HEAD` it was computed against, and have
`--blackboard` refuse when the target `.h`/`.cpp` already exist. That converts the Done-when box
from an mtime assertion into something the file itself can prove.

---

**F3 (high) — two of the four score terms are computed from model-written source. One
non-compiling line moves the pick.**

The law this file exists to enforce is *"an LLM never chooses the unit."* Deterministic Python is
necessary and not sufficient: **`depth` and `blockers` are parsed out of `Source/`, and `Source/`
is what the model writes.** The scorer is in a feedback loop with its own output.

*Input.* Copy `Source/Breachpoint/` byte-identically; add **one line** to `Core/BRCore.h`:
`#include "AbilitySystem/Abilities/BRGA_Grenade.h"`. Recompute with the real
`include_edges` / `transitive_includers` / `dependents_of`.

*Outcome.*

| | before | after one line |
|---|---|---|
| 1 | `BRGA_WeaponFire` depth 2 block 4 **106** | `BRGA_Grenade` depth 2 block **28** **130** |
| 2 | `BRGA_WeaponUtility` **106** | `BRGA_WeaponFire` **106** |
| 3 | `BRSpotterSubsystem` **104** | `BRGA_WeaponUtility` **106** |
| 4 | `BRGA_Grenade` depth 2 block 1 **103** | `BRSpotterSubsystem` **104** |

The top-ranked unit changes, and `blackboard()` follows it without comment. `INCLUDE_RE` only
checks that the captured name is a **declared unit** — it never checks the header exists. In the
probe `BRGA_Grenade.h` does not exist, so that include **cannot compile**; rung 1 would reject it
and the scorer accepts it anyway. A stale include, a speculative forward include, or a builder
tidying headers is enough. No corruption, no bad faith, no API call — and the run log prints
`28 real #include edges` with the same confident tone either way.

*The compound case is worse, and it nearly happened in step 4.* `state` is also model-derived,
via `classify()` reading model-written files, and it is the **1100-point** term. A builder that
writes a header plus a boilerplate `.cpp` and then stops at a `contract_gap` flips its unit
MISSING (100) → BUILT (−1000): **106 → −994, permanently out of the queue.** Step 4 stopped
*before* writing a file, which is the only reason `BRGA_WeaponFire` is still rankable. Had it
written the shell it filed the gap about, the architect would never propose it again — and
`BUILD-STATE.md` would report the fire path as built (see F4).

*Fix.* Two separable changes. (a) Compute `depth`/`blockers` from the ticket DAG and the
**manifest's** declared relationships only, and demote the include graph to a *printed
cross-check* that flags disagreement instead of feeding the total — the ticket calls it "declared
include edges", and an include a model just wrote is not declared. (b) If the include graph stays
a term, require the included header to exist on disk, so a term can never be moved by a line that
does not compile.

---

**F4 (high) — `BUILD-STATE.md` reports BUILT for an empty shell, and reports three complete units
as STUB. Proven end-to-end, today.**

`build_state.py`'s docstring answers this question **no**: *"this file has no opinion about state;
it prints `perception.json`'s state field verbatim. Corrupt the scanner and this lies; edit this
and it cannot."* The copy-not-restate discipline is right and the conclusion does not follow —
**the scanner does not need corrupting. It is wrong as written**, and `build_state.py` faithfully
copies the wrong answer.

*Input (BUILT for a STUB).* Full repo copy; add the exact shell a builder leaves when it stops at
a `contract_gap` — `BRGA_WeaponFire.h` (UCLASS, empty body) and a `.cpp` containing an include, a
comment reading *"BLOCKED … Nothing implemented"*, an empty constructor and an empty
`ActivateAbility()`. Run the unmodified `architect.py --all` then `build_state.py`.

*Outcome.* exit 0, 0, and `docs/BUILD-STATE.md` reads:

> `| tally | **BUILT 32** · **STUB 3** · **MISSING 9** |`
> `## BUILT` — *"32 units the scanner classified BUILT (a header plus a `.cpp` with a real body)"*
> `| \`BRGA_WeaponFire\` | AbilitySystem/ | BP03 | … |`

`classify()`'s heuristic is `len(stripped) > 3` over lines that are non-blank and do not *start
with* `#include`, `//`, `/*`, `*`. Bare braces are lines. So an empty ctor plus one empty override
is six "implementation" lines. Three probes against the real function:

| probe `.cpp` | contains | `classify()` |
|---|---|---|
| empty ctor + empty `ActivateAbility()` | zero statements | **BUILT** |
| one block comment, 4 prose lines | zero code — interior lines start with letters, so the comment filter misses them | **BUILT** |
| three functions wrapped in `#if 0 … #endif` | nothing the compiler sees | **BUILT** |

*Input (the inverse, and it is live in the repo right now).* No probe needed — read
`docs/BUILD-STATE.md` lines 121–123:

> `| 10 | \`BRDataRows\` | BP02 | STUB | 1 | 28 | 0 | 50 | **79** |`
> `| 11 | \`BRBotFacts\` | BP08 | STUB | … | **64** |`
> `| 12 | \`BRServerLifecycle\` | BP11 | STUB | … | **57** |`

`BRDataRows.h` is **889 lines / 35,333 bytes / 8 `USTRUCT`s**, and its own header comment declares
it *"the ONE header for every `DT_` DataTable row struct in the project."* Pure `USTRUCT`s need no
translation unit, so it has no `.cpp` **by design**. `BRBotFacts.h` is 14.8 KB / 4 `USTRUCT`s;
`BRServerLifecycle.h` is 20.6 KB / 10 declarations (a `UINTERFACE` seam — also `.cpp`-less by
design). `classify()`'s `if has_h and not cpps: return "STUB"` calls all three unimplemented.
The state of record therefore tells the reader that the project's DataTable schema is a stub and
offers it as the tenth thing to build. All three are complete.

**Both errors are one root: `classify()` measures the presence and line count of a `.cpp`, which
is a fact about the C++ build model, not about whether the unit is implemented.** In UE, absence
of a `.cpp` is often *correct*, and presence of one proves nothing.

*Fix.* Header-only is not STUB: if a `.h` declares a `USTRUCT`/`UINTERFACE`/`UENUM` and no
`UCLASS` with out-of-line members, it is BUILT — or, cheaper and honest, add a fourth state
`HEADER_ONLY` and stop scoring it as buildable work. For the other direction, replace the
line-count threshold with a body test (a `{` … `}` containing at least one statement-terminating
`;` outside a comment, ignoring `#if 0` regions), and treat a `.cpp` whose functions are all empty
as STUB. And **`classify()` needs `test_selfcheck.py` cases** — see F7.

---

**F5 (high) — `architect.py --blackboard` writes inside `Source/`, and overwrites arbitrary `.md`
anywhere in the repo, including this ticket.**

`blackboard()` builds its output path as `BLACKBOARD_DIR / f"{stamp}-{top['unit']}.md"`, where
`top` comes from `state/ranking.json` **re-read off disk** on the `--blackboard`-only path. The
unit name is validated when the *manifest* is parsed (`UNIT_RE` + `.split("/")[-1]`) and **never
re-validated when it comes back out of JSON**. `ranking.json` is a committed file that lives
outside `Source/` and `Content/`, so gate A and every review habit on this board treat it as
generated noise.

*Input.* Unmodified `architect.py` in a fake repo; the only change is one string in
`ranking.json`: `ranked[0]["unit"] = "x/../../../../Source/Breachpoint/AbilitySystem/BRGA_WeaponFire"`.
Run `python architect.py --blackboard`.

*Outcome.* **exit 0**, and the run log prints
`wrote Tools/architect/blackboard/2026-08-01-x/../../../../Source/Breachpoint/AbilitySystem/BRGA_WeaponFire.md`.
A 2,821-byte file now exists at `Source/Breachpoint/AbilitySystem/BRGA_WeaponFire.md`. The
docstring's second law — *"Nothing under `Source/` is ever opened for writing here"* — is false.

*The escalation, also executed.* The extension is hardcoded `.md`, so a `.cpp` cannot be clobbered
— but every `.md` in the repo can. With
`ranked[0]["unit"] = "x/../../../../docs/tickets/TICKET_BP15_ARCHITECT"`, exit 0 and the ticket's
495 lines are replaced by a blackboard: `# Blackboard — x/../../../../docs/tickets/… — 2026-08-01`,
2,751 bytes. **The same one-line input destroys this Log, `CLAUDE.md`, `docs/contracts/*.md`, or
`BREACHPOINT-ARCHITECTURE.md` — the manifest the scanner parses.** `guard_laws.py` cannot help:
it is a tool-call hook on Edit/Write and is blind to a Python process writing through `pathlib`.

*Windows detail worth recording, because it makes the bug look absent.* The naive payload
`"../../../../Source/…"` **fails** with `FileNotFoundError` — the leading component becomes
`2026-08-01-..` and Win32 strips trailing dots, so it is read as a directory name, not a parent
ref. One character (`x/` in front) restores clean `..` components and Win32 canonicalises them
lexically before touching the filesystem. *A failed traversal attempt on this platform is not
evidence the traversal is impossible.*

*Fix.* Two lines in `blackboard()`: reject any `top["unit"]` not matching `^BR[A-Za-z0-9_]+$`, and
assert `path.resolve().is_relative_to(BLACKBOARD_DIR.resolve())` before the write. More generally,
`ranking.json` re-read from disk is **untrusted input** on the `--blackboard` and `--rank` paths
and should be schema-checked at load, not trusted because architect.py usually writes it.

---

**F6 (high) — the sixth question: two more absent terms, and one of them is the whole margin of
the current pick.**

The step-4 entry filed *readiness* as the missing fifth term. Two more are missing, and unlike
readiness they do not merely fail to demote — they actively produce the wrong order.

***6a. `blockers` counts units that are already BUILT as "waiting on this one."*** `dependents_of()`
returns downstream **tickets** and the loop adds *every* unit of those tickets regardless of state.

*Input.* Ask who is recorded as waiting on the selected unit. `BRGA_WeaponFire`'s four blockers
are, in full:

| waiter | state | ticket |
|---|---|---|
| `BRActivatableWidget` | **BUILT** | BP10 |
| `BRHUDLayout` | **BUILT** | BP10 |
| `BRUIManagerSubsystem` | **BUILT** | BP10 |
| `BRViewModels` | **BUILT** | BP10 |

**All four are already built. Nothing is waiting on `BRGA_WeaponFire`.** Its blocker term is 4/4
fictional, and 4 is exactly the margin that put it first.

*Outcome, recomputing blockers over non-BUILT waiters only and changing nothing else:*

| # | now | with the fix |
|---|---|---|
| 1 | `BRGA_WeaponFire` **106** | `BRSpotterSubsystem` **104** |
| 2 | `BRGA_WeaponUtility` **106** | `BRGA_Grenade` **103** |
| 3 | `BRSpotterSubsystem` **104** | `BRGA_Melee` **103** |
| 4 | `BRGA_Grenade` **103** | `BRGA_Grapple` **103** |
| 5 | `BRGA_Melee` **103** | `BRGA_WeaponFire` **102** |

The selection inverts. `BRDataRows`' 28 blockers become 9; `BRBotFacts`' 11 become 3. **The Log's
strongest claim — *"a deterministic scorer with no knowledge of the handoff reproduced the human
board's next move"* — does not survive this.** The agreement with HANDOFF is produced by counting
four finished UI widgets as blocked work; correct the term and the scorer picks
`BRSpotterSubsystem`, which the human board is not proposing. That does not make the scorer
useless, but the coincidence must not be cited as evidence it measures something real until 6a is
fixed and the agreement either survives or does not.

***6b. `depth` is added, so the score rewards being far from startable.*** `score = depth +
blockers + tier + state`. `ticket_depth` is *distance from the roots of the DAG* — how many
tickets must finish first. Adding it means **the more prerequisites a unit has, the higher it
scores.** For a "what to build next" scorer that is a sign error.

*Input → wrong outcome, live in `BUILD-STATE.md` right now.* The three BP00 test specs
(`BRCombatSpec`, `BRShieldSpec`, `BRBotDeterminismSpec`) have `depth 0` — BP00 gates on nothing,
they are startable today, and `Source/Breachpoint/Tests/` is empty. They rank **7, 8, 9** at 100.
`BRGA_Grapple` — BP06, gated behind BP05, gated behind BP02, *"THE netcode packet"* — ranks **6**
at 103, ahead of all three, **because it has three more prerequisites**. Meanwhile the same
generated file's own *Ladder blockers* table says rung 2 is `BLOCKED — Source/Breachpoint/Tests/
holds only .gitkeep`. **The scorer ranks the three units whose absence blocks rung 2 for the entire
project below the hardest unblockable unit in the codebase, and it does so as a reward for their
having no dependencies.** Combined with 6a, the current #1 has both an inflated blocker count and
a depth bonus.

*Fix.* Both are small and both are inside `architect.py`. 6a: filter `waiters` to
`state != "BUILT"` — one line, and the run log should print the waiter names, not just the count,
because a printed 4 that is four built widgets is exactly the kind of number a printed-terms
scorer is supposed to make impossible to hide. 6b: subtract depth, or drop it and let `blockers`
carry criticality — but **whichever way it goes, the sign is a founder call and belongs in
`DESIGN-RULINGS.md`, not in a builder's diff**, on the same reasoning that kept readiness a
proposal.

---

**F7 (medium) — the red-then-green evidence covers one of the three steps.**

`test_selfcheck.py`'s five cases all transform `BREACHPOINT-ARCHITECTURE.md` and all exercise
`parse_manifest()`. **`classify()`, `rank()` and `blackboard()` have zero cases.** The Log's own
justification for the file — *"an enforcement mechanism proves nothing until it is tested with a
case it should REJECT"* — applies hardest to `classify()`, which produces the term worth 1100
points and the central claim of `BUILD-STATE.md`, and which F4 shows to be wrong in **both**
directions. The four probes in F4 are the missing cases nearly verbatim: empty-shell `.cpp` → must
not be BUILT; comment-only `.cpp` → must not be BUILT; `#if 0` `.cpp` → must not be BUILT;
header-only `USTRUCT` → must not be STUB. F5's traversal string is a fifth. *Severity medium only
because it is the absence of a test, not a defect; the defects it would have caught are F4 and F5.*

---

**What I could not break, stated so the absence of a finding is not read as an absence of
looking.** Each of these I tried to construct a failing input for and could not:

- **Path injection through the manifest.** `SECTION_RE`'s folder group is `[A-Za-z]+` and
  `UNIT_RE` is `BR[A-Za-z0-9_]+` followed by `.split("/")[-1]`. No separator, dot or drive letter
  survives. The `--all` path is clean; **only the JSON re-read path (F5) is exploitable.**
- **Zero API calls.** `architect.py` imports `argparse, json, re, sys, datetime, pathlib` and
  nothing else; `build_state.py` adds `subprocess`, used only for `git`. There is no network
  import and no model consulted. **The claim is true.**
- **The GE exclusion.** `count_ge_classes()` asserts against the header itself and exits 2 on
  drift. It does not read §4's prose. Correcting five documents moved no number, as the Log said.
- **Tie-breaking.** `(-total, int(ticket digits), unit)` is a total order over the unit set with no
  model input and no dictionary-order dependence.
- **`build_state.py`'s own determinism.** No clock, no host, no cwd; SHAs truncated in Python,
  `--date=short` pinned, every list explicitly sorted, `cell()` escapes `|`, undeterminable
  commits print `-`. I could not make it fabricate. **The lie in F4 enters upstream of it, which
  is exactly what its docstring predicted and exactly why "corrupt the scanner and this lies" is
  not a defence — the scanner ships wrong.**

---

**What this means for the Done-when boxes.** No box moves on this entry. F1 and F2 bear directly
on *"the blackboard file exists and its mtime predates the generated source file's"* — that box
cannot be honestly checked while ordering is provable only by a resettable mtime. F4 bears on
*"`docs/BUILD-STATE.md` regenerates byte-identically"*: it does, and it regenerates a wrong number
byte-identically. **Byte-identical is a determinism property, not a truth property, and this
ticket should not let the first stand in for the second.**

*Recommended order to address, cheapest first, all inside `Tools/architect/`:* F5 (two lines,
stops the architect destroying this Log) → F6a (one line, changes the pick) → F4 (`classify()`
rewrite + F7's cases) → F1 (UNAUTHORISED section in `build_state.py`) → F2 (first-wins write +
in-file timestamp) → F3 (demote the include graph). **F6b's sign and F3's demotion are founder
calls**, per this ticket's own precedent for the readiness term.

**1 Aug 2026 — step 6's F5, F4 and F6a FIXED. The ranking changed, and a headline claim in this
Log is now RETRACTED.**

*The retraction first, because it was the most-repeated line in this ticket.* An earlier entry
said the score *"independently landed on the fire path, which is exactly what HANDOFF's 'restart
BP03 step 2' says to do next"* and called it **"the strongest evidence this ticket can produce
that the scoring is measuring something real."* **It does not survive F6a.** `BRGA_WeaponFire`'s
**entire** blocker score of 4 was four **already-BUILT** BP10 widgets — `BRActivatableWidget`,
`BRHUDLayout`, `BRUIManagerSubsystem`, `BRViewModels`. Nothing was waiting on it. Counting only
unbuilt waiters drops it 106 → 102 and it is no longer first. The agreement with the human board
was **an artifact of the defect**, and it was reported as corroboration. Recorded at length
because a number that flatters the thing that produced it is the one to distrust.

| Fix | Was | Now |
|---|---|---|
| **F5** path traversal (high) | `blackboard()` interpolated `ranked[0]["unit"]` into a path with no re-validation; a crafted name overwrote any `.md` in the repo, this ticket included | `safe_unit_name()` requires a bare `BR*` identifier; the probe payload now exits 1 with a refusal |
| **F4** BUILT-for-STUB, both directions (high) | `>3 non-include lines` called an empty ctor, a 100%-comment file and a file inside `#if 0` **BUILT**; and demanded a `.cpp` from units that owe none | statements counted by semicolons outside comments/dead-blocks/preprocessor; and the expected **form** is parsed from §3 (`BRDataRows.h` vs `BRGA_Sprint.h/.cpp`) |
| **F6a** blockers (high) | counted BUILT units as waiters | only unbuilt waiters count |

*F4's inverse mattered as much as F4.* `BRDataRows` (889 lines, 8 `USTRUCT`s, *"the ONE header
for every DT_ row struct"*), `BRBotFacts` and `BRServerLifecycle` were reported **STUB and offered
as work**. All three are header-only **by design**, and §3 says so in the only place that counts —
it writes `BRDataRows.h`, not `BRDataRows.h/.cpp`. The form was in the manifest the whole time;
the scanner inferred instead of parsing, against this ticket's own out-of-scope line. **Disk now
reads BUILT 34 · STUB 0 · MISSING 10**, and zero STUBs is the correct answer, not a suspicious one.

*The new ranking, and the reason it is worse rather than better:*

```
 1  BRSpotterSubsystem   BP11  MISSING  depth 4  block 0  tier 0  state 100  = 104
 2  BRGA_Grenade         BP05  MISSING  depth 2  block 1                     = 103
 5  BRGA_WeaponFire      BP03  MISSING  depth 2  block 0                     = 102
```

**The corrected score now picks the LEAST startable unit on the board.** `BRSpotterSubsystem` is
BP11 — gated by BP08, gated by BP02+BP04 — and it wins on **depth 4** alone. That is **F6b**,
which step 6 filed and this entry does not fix: `depth` is **added**, so the score rewards
distance from a root. The same generated `BUILD-STATE.md` reports rung 2 BLOCKED because
`Tests/` is empty, while ranking the three test specs (depth 0, genuinely startable) at 7–9.
**Flagged, not fixed: the sign of the depth term is a founder call**, on the same precedent as
the readiness term — step 2's four terms are this ticket's spec, and inverting one is not a
builder's call to make.

*Blackboard:* `2026-08-01-BRGA_WeaponFire.md` **deleted**, same rule as the two before it — a
blackboard is an *authorisation*, and the corrected score does not select that unit. The Log is
the record; the blackboard is the authorisation.

*Verification after the fixes:* `--all` exit 0 · `test_selfcheck.py` **5/5** · `build_state.py`
double-run **byte-identical** · F5 payload **refused, exit 1** · `BUILD-STATE.md` regenerated
(142 lines) and now carries the corrected states and ranking.

*Not fixed, still open from step 6:* **F1** (a unit the score did not pick can land, and
`BUILD-STATE.md` will attest it — nothing compares a landing to an authorisation), **F2**
(blackboard can be written after the source; the filename is day-granular so a re-run resets
mtime), **F3** (include edges are model-written, and a one-line `#include` of a **non-existent**
header moved a unit +27 and to #1), **F6b** above, and **F7** (`test_selfcheck.py` exercises only
`parse_manifest`; `classify`, `rank` and `blackboard` still have zero cases — F4 and F5 were both
found in exactly that gap). **Step 6's box stays unchecked**: findings are addressed only in part.
