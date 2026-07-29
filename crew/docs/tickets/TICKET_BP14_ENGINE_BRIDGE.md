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

## Steps (in order)

1. **Relocate the crew into the game repo.** Copy `assignments/03-agent-crew/run_crew.py`
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

- [ ] `python3 Tools/data-crew/run_crew.py` (replay) exits 0 from the game repo
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
