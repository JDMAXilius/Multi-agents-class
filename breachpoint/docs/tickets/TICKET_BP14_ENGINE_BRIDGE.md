# TICKET — BP14: Wire the crew orchestrator to the engine ladder

> STATUS: open — cut 29 Jul 2026 after the data crew ran to completion. **Terminal-only:
> nothing in this ticket can be done in a cloud container — it needs UE 5.8 installed, a
> `.uproject`, and build targets.** Needs BP00 (ladder wrappers) and BP01 step 1 (project
> skeleton) to exist first.

The data crew works: `run_crew.py` runs four agents through deterministic gates and lands
game-ready **data**. It cannot yet land **code**, because the verifier's real rungs need an
engine. In the 29 Jul run the verifier reported five of nine checks BLOCKED with the honest
reason — *"no .uproject, Source/, or build targets found."* That gap is this ticket. Founder
laws bind it unchanged: the verifier gets **no write tools**, compiles ≠ works, and a rung
that cannot run is BLOCKED, never silently skipped.

**Ordering law:** BP00 step 1 (`Tools/run-ubt.ps1`, `run-specs.ps1`, `run-gauntlet.ps1`)
gates everything here — the orchestrator shells out to those wrappers, it does not reinvent
them. Step 1 below gates steps 2–4.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed **for steps 2–5**. **Step 1 is `files-only`** and was executed
  separately from a cloud session on 1 Aug 2026 (see Log) precisely because two other tickets
  gate on its output.
- Ticket BP00 is DONE: `Tools/run-ubt.ps1`, `run-specs.ps1`, `run-gauntlet.ps1` exist
  and each produces a real pass/fail artifact (this ticket shells out to them; it does
  not reinvent them)
- Ticket BP01 step 1 is landed (a `.uproject` and build targets exist to compile)
- `python3 Tools/data-crew/run_crew.py` (replay) exits 0 from the game repo
  <!-- DEFECT FIXED 1 Aug 2026: this line gated the ticket on an artifact that only this
       ticket's OWN step 1 creates — unsatisfiable by construction, and BP15's Kickoff
       inherited it. It now reads as a gate on steps 2–5 only, and step 1 has been run. -->
- owner_path: `Tools/data-crew/`

## Steps (in order)

1. **Relocate the crew into the game repo.** Copy `../assignments/03-agent-crew/run_crew.py`
   (planning repo) to `Tools/data-crew/` in the game repo. It already resolves agents from
   `.claude/agents/` when dropped at the root — confirm `find_agents_dir()` picks up the
   real crew, and that `python3 run_crew.py` (replay) still exits 0 from the new location.
   Owner: **builder**. Contracts: `data-and-assets.md`.
2. **Add a `code` job type.** Generalize the pipeline so a job's producer is a *builder*
   writing files inside an `owner_path` rather than a curator returning records. Same gates,
   same order: producer → gate A (**diff confined to `owner_path`** — a diff outside it is
   auto-rejected, never "just a tiny fix") → critic REFUTER → gate B → verifier → land.
   The ticket file itself is the job spec: parse `owner_path`, contracts, and Done-when out
   of `docs/tickets/TICKET_*.md`. Owner: **builder**. Contracts: `testing.md`.
3. **Replace the verifier's simulated checks with the real ladder.** The verifier stage
   shells out and reports verbatim — exit code, and the actual failing output, never a
   summary:
   - rung 1 `Tools/run-ubt.ps1` — clean compile, all three targets, from clean state
   - rung 2 `Tools/run-specs.ps1` — `-nullrhi -unattended`, packet suites + pinned sim suites
   - rung 3 functional tests for the packet's maps (single instance)
   - rung 4 `Tools/run-gauntlet.ps1` — dedicated server + 2 clients, `BRGauntlet.SmokeTS2C`
   A rung with no wrapper is reported **BLOCKED with the reason**. Landing requires PASS on
   every rung the ticket names. Owner: **builder**, **verifier** consults on report shape.
4. **Prove the gates actually bite.** Three deliberate failures, each run once and recorded:
   a build error must fail rung 1; a broken sim value must fail rung 2 red-then-green; a
   replication change that works in PIE but breaks a networked client must fail rung 4.
   A gate that has never failed is a gate nobody has tested. Owner: **verifier** runs,
   **netcode-builder** authors the replication break.
5. **Adversarial review of the harness itself** (REFUTER, `critic`): can a builder land a
   diff outside `owner_path`? Can a job pass with every rung BLOCKED? Can the orchestrator
   be made to write a file before the verifier returns PASS? Can a `high` finding be
   downgraded to escape the block? Each answer needs input → wrong outcome, not vibes.
   Owner: **critic**.

## Done when

- [x] `python3 Tools/data-crew/run_crew.py` (replay) exits 0 from the game repo — **step 1
      only; done 1 Aug 2026, see Log.** No engine claim is attached to this box: it proves a
      Python pipeline runs, nothing about a rung.
- [ ] One real code ticket lands end-to-end through the pipeline with rungs 1/2/4 green,
      the transcript in that ticket's Log
- [ ] Rung outputs are verbatim in the log — command, exit code, failing output
- [ ] All three deliberate failures proven to fail the correct rung (step 4), recorded
- [ ] A diff outside `owner_path` is rejected by gate A — proven once with a real diff
- [ ] Critic findings from step 5 addressed or explicitly waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: `builder` (harness) · `netcode-builder` (the replication break) · `verifier` (runs
  the ladder, never fixes) · `critic` (refutes the harness). The orchestrator stays a
  deterministic script — it holds no opinions and makes no design calls.
- Binary files this ticket OWNS: **none.**
- Out of scope: CI/cloud runners (local ladder first — a green cloud badge over an unproven
  local ladder is the exact self-deception the honesty law exists to stop); replacing the
  ticket board with a database; letting an LLM anywhere near the simulation at runtime.
- Why terminal-only: rungs 1–4 need a UE install, `.uproject`, build targets, and multiple
  processes. The 29 Jul cloud run proved the data half works without any of that — this
  ticket closes the other half.

## Log

(append findings here, dated, newest last — this is what the next session reads)

**1 Aug 2026 — STEP 1 DONE (cloud session, Context A). The Kickoff gate was circular.**

*The defect first, because it is the reusable lesson.* This ticket's Kickoff required
`python3 Tools/data-crew/run_crew.py` to exit 0 **from the game repo** — but that path is
created by this ticket's own **step 1**. The gate could never pass before the work it gates.
Worse, it propagated: `TICKET_BP15_ARCHITECT.md`'s Kickoff cites the same line as "(BP14 step
1)", so the Architect packet was gated behind an unsatisfiable condition and nobody noticed,
because a gate that is never *reached* is indistinguishable from a gate that passes.

*Why a cloud session could close it:* step 1 is a file copy and a path fix. It needs no engine,
no editor, no build. The ticket was marked `engine-installed` as a whole, which is true of steps
2–5 and false of step 1 — the `requires:` line now says so per-step. **Generalisable:** a ticket
whose steps span two execution contexts should say which is which, or the cheap step waits on
the expensive machine for no reason.

*What landed* (all inside `owner_path: Tools/data-crew/`):
- `run_crew.py` and `recording.json` copied from `../assignments/03-agent-crew/`.
- **`find_agents_dir()` was wrong for this repo and the ticket's own text was wrong about it.**
  Step 1 asserts the script "already resolves agents from `.claude/agents/` when dropped at the
  root." It does not — it checked exactly two fixed paths, `./agents` (zip layout) and
  `../../crew/.claude/agents` (planning repo). From `Tools/data-crew/` the second resolves to
  `breachpoint/crew/.claude/agents`, which does not exist, so the script would have exited with
  "agent definitions not found." Fixed by walking `HERE.parents` for `.claude/agents/critic.md`
  — a walk, not a third fixed depth, so moving the script again cannot silently break it.
- `Tools/data-crew/output/` added to `.gitignore`. The replay re-derives `DT_Weapons.csv`
  byte-identical to `Content/Data/DT_Weapons.csv`; committing the scratch copy would put a
  **second, silently drifting** copy of the source of truth in the tree.

*Verification, stated at its real rung:* `python3 run_crew.py` (replay, no API key) from
`breachpoint/Tools/data-crew/` — **exit 0**, both jobs landed, agents resolved from
`breachpoint/.claude/agents/`. The weapons job ran its gates and the arena job bounced the critic
three times before converging, so the pipeline is genuinely executing, not short-circuiting.
**This proves a Python program runs. It is not a rung** — no engine was involved and none of
steps 2–5 (the `code` job type, the real ladder, the deliberate failures) is started.

*Still open on this ticket:* everything else. Steps 2–5 remain `engine-installed`.
