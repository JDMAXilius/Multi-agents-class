# TICKET — Run the data crew to author DT_Weapons and the arena manifest

> STATUS: open — cut alongside Assignment #3, 29 Jul 2026. Runnable today; needs no
> engine, no compile, no UE install. Its outputs unblock BP03 (weapons) and BP07 (arena).

Every tuning number and every arena coordinate in Breachpoint must trace to a **named agent
output that a human reviewed** (GDD §"success criteria"). This ticket is that pipeline made
executable: four agents produce `Content/Data/DT_Weapons.csv` and the arena blockout spec
under adversarial review, and nothing lands until a write-disabled verifier proves it. Data
is not code — these files are the source of truth the C++ reads, never inline numbers.

**Ordering law:** the weapons job gates BP03's fire abilities (they read the table); the
arena job gates BP07's blockout (the builder executes the manifest, it does not invent
geometry). Both jobs are independent of each other and may run in parallel.

## Steps (in order)

1. **Run the crew.** From `assignments/03-agent-crew/` (planning repo) or the copied
   `Tools/data-crew/` (game repo):
   `python3 run_crew.py --live` — or `--job weapons` / `--job arena` to run one.
   No API key? `python3 run_crew.py` replays the recorded run through the same gates.
   The orchestrator is the manager: it enforces the gates, the agents do the thinking.
2. **Producer stage** — `tuning-curator` proposes all three weapon rows against the
   `FBRWeaponRow` schema with implied-TTK evidence vs 200 EHP; `arena-architect` returns
   `arena_manifest.json` (≥ 8 scored spawns, 35 m sightline cap, named callout landmarks).
   Binds: `contracts/data-and-assets.md` (soft refs, CSV-not-code).
3. **Adversarial stage** — `critic` in REFUTER mode attacks the design: a TTK that breaks
   strip-then-finish, a weapon that invalidates a slot, a mag that cannot finish its job,
   a spawn pair the stated cover does not separate, a rocket pad one team holds risk-free.
   Findings need input → wrong outcome in a live 4v4. Bounces to the producer, max 2, then
   the run exits 2 and escalates to the lead — a refusal to converge is a real signal, not
   a failure to work around.
4. **Landing stage** — `builder` formats the surviving records into the exact file content.
   **Values are frozen at this stage**; a changed number here is the silent-drift bug class
   this pipeline exists to kill, and is a finding against the builder.
5. **Verification stage** — `verifier` (no write tools by capability) independently
   recomputes TTK from the row values alone and the minimum pairwise spawn distance from
   the coordinates, audits schema/importability, and flags any claim that actually needs the
   editor rung. PASS is required before the orchestrator writes the file to disk.
6. **Human review + land in the game repo.** The lead reads `output/run_log.txt` — the
   critic's findings and the verifier's arithmetic — then copies the two artifacts to
   `Content/Data/`. Import DT_Weapons against `FBRWeaponRow`; hand the manifest to BP07's
   blockout owner (binary-asset law: the `.umap` has one owner, this file is its spec).

## Done when

- [ ] `output/DT_Weapons.csv` exists, verifier verdict PASS, and imports against
      `FBRWeaponRow` in-editor with zero row errors
- [ ] `output/arena_manifest.json` exists, verifier verdict PASS, ≥ 8 spawns, min pairwise
      spawn distance ≥ 8 m, max sightline ≤ 35 m
- [ ] Recomputed body-shot TTKs are in the Halo band and the AR strips 100 shields within
      one magazine — stated as numbers in the Log, not asserted
- [ ] `python3 run_crew.py` (replay, no key) completes exit 0 on a clean machine
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: `tuning-curator` / `arena-architect` propose · `critic` refutes · `builder` lands ·
  `verifier` proves. The orchestrator is a script, not an agent — it holds no opinions.
- Binary files this ticket OWNS: **none.** It authors data only; `.umap` work is BP07's.
- Out of scope: writing C++ that reads these tables (BP03), building the blockout (BP07),
  bot profiles (`DT_BotTuning` — same pipeline, add a third job when BP08 opens).
- Re-runnable: when telemetry arrives, re-run the weapons job — the curator's doctrine
  proposes only outside the 45–55% win-rate band, so a re-run on healthy data correctly
  proposes nothing. Restraint is a deliverable.

## Log

(append findings here, dated, newest last — this is what the next session reads)
