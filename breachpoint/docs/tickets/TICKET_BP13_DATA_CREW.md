# TICKET — Run the data crew to author DT_Weapons and the arena manifest

> STATUS: **steps 1–5 DONE** (crew run live 29 Jul 2026, both artifacts landed with verifier
> PASS — see Log). **Step 6 remains and needs the terminal:** import into the UE project.
> Its outputs unblock BP03 (weapons) and BP07 (arena).

Every tuning number and every arena coordinate in Breachpoint must trace to a **named agent
output that a human reviewed** (GDD §"success criteria"). This ticket is that pipeline made
executable: four agents produce `Content/Data/DT_Weapons.csv` and the arena blockout spec
under adversarial review, and nothing lands until a write-disabled verifier proves it. Data
is not code — these files are the source of truth the C++ reads, never inline numbers.

**Ordering law:** the weapons job gates BP03's fire abilities (they read the table); the
arena job gates BP07's blockout (the builder executes the manifest, it does not invent
geometry). Both jobs are independent of each other and may run in parallel.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- Steps 1-5 are DONE (see Log). For the remaining **step 6** only:
- `Source/Breachpoint/Data/BRDataRows.h` defines `FBRWeaponRow` **including the
  `DamageDelivery` column** (the schema split the verifier forced — reimport fails without it)
- A UE 5.8 editor can open the project (this step is the import, and needs the engine)
- owner_path: `Content/Data/`

## Steps (in order)

1. **Run the crew.** From `../assignments/03-agent-crew/` (planning repo) or the copied
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
   Findings need input → wrong outcome in a live 4v4. **Only `high` severity blocks**; medium
   and low land in `output/open_risks_*.json` for the lead. Blocking findings bounce to the
   producer, max 3 rounds, then the run exits 2 and escalates — a refusal to converge is a
   real signal, not a failure to work around.
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

- [x] `output/DT_Weapons.csv` exists, verifier verdict PASS — **import against `FBRWeaponRow`
      in-editor with zero row errors is the remaining half of this box (step 6, terminal)**
- [x] `output/arena_manifest.json` exists, verifier verdict PASS, 8 spawns, min pairwise
      spawn distance 11.36 m (≥ 8 m), max sightline 35 m
- [x] Recomputed body-shot TTKs stated as numbers in the Log, and the AR strips 100 shields
      in 13 shots against a 32-round magazine
- [x] `python3 run_crew.py` (replay, no key) completes exit 0 on a clean machine
- [ ] `FBRWeaponRow` in `BRDataRows.h` carries the `DamageDelivery` column (schema split —
      see Log); reimport is clean after
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

**29 Jul 2026 — crew run live, both jobs landed (steps 1–5 done).**
Run recorded in `../assignments/03-agent-crew/recording.json` (18 exchanges); transcript in
`output/run_log.txt`. Model: claude-sonnet-5 via the `claude` CLI.

*Landed weapons table* (`output/DT_Weapons.csv`), verifier PASS:

| Name | FireMode | DamageDelivery | Dmg | RPM | Mag | HeadshotMult |
|---|---|---|---|---|---|---|
| AR | Automatic | Hitscan | 8 | 600 | 32 | 1.0 |
| Magnum | SemiAuto | Hitscan | 22 | 180 | 8 | 2.0 |
| Rocket | SemiAuto | Projectile | 120 | 30 | 2 | 1.0 (splash 4 m / 90, speed 30 m/s) |

Verifier arithmetic, recomputed from the row values alone vs 200 EHP:
AR body TTK **2.4 s** (25 shots), Magnum **3.0 s** (10 shots), Rocket **2.0 s** (2 shots).
AR strips 100 shields in **13 shots** — inside one 32-round mag.

*Landed arena manifest* (`output/arena_manifest.json`), verifier PASS: `breachpoint_vs01`,
bounds 40×40×12 m, **8 spawns** across three z-levels (0/4/8), 7 named callout landmarks
(The Core, Mezzanine Catwalks, The Gantry, N/S Barricade, W/E Stack), 8 cover volumes,
max sightline **35 m**, 4 recorded doubts. Verifier computed all C(8,2)=28 pairwise
distances: **minimum 11.36 m**, tied across SP5–SP7/SP5–SP8/SP6–SP7/SP6–SP8.

*Three defects the crew caught before landing:*
1. **Rocket one-shot.** First pass set the Rocket's `HeadshotMult=2.0` → 120×2 = 240 vs
   200 EHP, a guaranteed one-shot on a full-shield target. Destroys the two-shot
   power-weapon fantasy. Corrected to 1.0.
2. **AR headshot inversion.** `HeadshotMult=2.0` at 600 RPM let an accurate AR player
   out-finish the Magnum, inverting strip-then-finish. Crew converged on **1.0** — headshot
   bonuses belong to precision weapons. Matches Halo; reached by the pipeline, not handed to it.
3. **Schema defect, found by the verifier.** `FireMode` held `{Automatic, SemiAuto}` while
   also being the field meant to separate hitscan from projectile, so the
   "ProjectileSpeed is 0 for hitscan" invariant was **unverifiable at import**. Column split
   into `FireMode` (cadence) + `DamageDelivery` (Hitscan|Projectile); the invariant is now
   enforced deterministically in `run_crew.py`. **`FBRWeaponRow` must gain this column** —
   carried into Done-when above and into BP03.

*Accepted non-blocking risks:* 2 (weapons) + 3 (arena), in `output/open_risks_*.json`.
Chief among them: the Rocket's `ReserveMags=0` means "reload" is unreachable by design, and
the arena's mutual-visibility and 5 m LOS-break claims are **editor-rung** — the verifier
reported them BLOCKED, not passed. Confirm both at BP07's walkthrough.

*Method note:* the first pipeline build deadlocked — the critic found something every round
and nothing ever shipped. Fixed by splitting blocking from non-blocking severity. A reviewer
with no ship gate is a reviewer that never ships.
