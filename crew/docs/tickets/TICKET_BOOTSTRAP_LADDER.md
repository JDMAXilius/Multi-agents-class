# TICKET — Bootstrap the validation ladder (rungs 1, 2, 4)

> STATUS: open — cut by lead session, 2026-07-22. Needs: UE 5.8 install path in
> `Tools/env.local`, project targets compiling, Steam SDK configured for two test accounts.

Founder directive: before any feature packet lands, the crew must be able to PROVE things —
compile clean, pin sim math headless, and demonstrate replication with a real dedicated
server + 2 clients. Founder laws binding this ticket: server-authoritative only; numbers in
DataTables; no feature work rides along.

**Ordering law:** Step 1 (fill-in verification + wrapper scripts) gates all later steps.
Rung 2 (step 2) and rung 4 (step 3) may then proceed in parallel — different owners.

## Steps (in order)

1. Verify the SLASH ROLLER fill-ins in `docs/contracts/testing.md` against the real machine:
   create `Tools/env.local` (ENGINE_ROOT), author `Tools/run-ubt.ps1`, `Tools/run-specs.ps1`,
   `Tools/run-gauntlet.ps1` as thin UBT/UAT wrappers. Owner: **builder**. Contracts:
   `testing.md`.
2. One pinned headless suite for an existing sim rule: `SlashRoller.Sim.Stamina` — exact
   cases (light=15, heavy=30, dodge=20 drain; winded at 0 until 30%; regen 20/s after 1.0 s)
   plus invariants (stamina never negative; winded never permanent). Owner: **sim-builder**.
   Contracts: `testing.md`, `data-and-assets.md` (values read FROM `DT_CombatTuning.csv`,
   asserted against the table, not literals).
3. Gauntlet skeleton + the first smoke scenario **`SRGauntlet.SmokeDM2C`**: dedicated server
   + 2 clients join `SR_Arena01`, client A lands a light attack on client B, assert in threes
   (server truth, A's view, B's view), then repeat under `-PktLag=120 -PktLoss=5`.
   Owner: **builder**, **netcode-builder** consults on the assertion points. Contracts:
   `testing.md`, `netcode.md`.
4. Verifier runs all three rungs from clean state and reports verbatim (rung 3 reported
   as NOT WIRED for now — honest gap, tracked here). Owner: **verifier**.
5. Critic prompt-hole review of the wiring (REFUTER): can a suite pass while asserting
   nothing? Can the Gauntlet scenario pass with replication actually broken (e.g. asserting
   only server state)? Owner: **critic**.

## Done when

- [ ] Rung 1 produces a real pass/fail artifact from clean state (all three targets:
      Editor, Game, Server)
- [ ] Rung 2: `SlashRoller.Sim.Stamina` runs headless (`-nullrhi -unattended`) and FAILS if
      a pinned value or invariant is broken (proven by a deliberate red run, then green)
- [ ] Rung 4: `SRGauntlet.SmokeDM2C` passes on dedicated + 2 clients, and its emulation
      variant runs; a deliberately-broken replication change makes it fail (proven once)
- [ ] Critic findings addressed or explicitly waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder (wrappers, Gauntlet skeleton) · sim-builder (stamina suite) ·
  netcode-builder (assertion consults) · verifier (runs) · critic (refutes wiring)
- Binary files this ticket OWNS: none (scripts + C++ + config only; `SR_Arena01` blockout is
  a separate arena ticket)
- Out of scope: any gameplay feature, arena art, bot logic, CI wiring (next ticket), fixing
  rung 3 functional tests

## Log

(append findings here, dated, newest last — this is what the next session reads)
