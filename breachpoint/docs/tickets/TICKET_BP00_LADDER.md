# TICKET — BP00: Bootstrap the validation ladder (rungs 1, 2, 4)

> STATUS: open — cut by lead session, 29 Jul 2026. Needs: UE 5.8 install path in
> `Tools/env.local`; Breachpoint project skeleton (BP01 step 1) for targets to exist.

Founder directive: before any feature lands, the crew must be able to PROVE things — clean
compile on all three targets, pinned headless sim specs, and a real dedicated-server + 2-client
Gauntlet smoke. Founder laws: server-authoritative only; numbers in DataTables; no feature
work rides along.

**Reference skill:** `gauntlet-testing` for step 3 — load it before writing the test node.
**It is an UNVERIFIED draft** (written from docs, never run): this ticket is its first use, so
correcting it is part of step 3, and the delta goes in the Log.

**Ordering law:** Step 1 gates all. Steps 2 and 3 may then run in parallel (different owners).
BP01 step 1 (project + targets exist) gates this ticket's step 1.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- `Tools/env.local` exists and its `ENGINE_ROOT` points at a **source-built UE 5.8**
  (launcher installs cannot build the server target — this ticket builds all three)
- Ticket BP01 step 1 is landed: `Breachpoint.uproject` plus all three `*.Target.cs`
  files exist (there is nothing to compile otherwise)
- owner_path: `Tools/`, `Source/Breachpoint/Tests/`

## Steps (in order)

1. Author `Tools/env.local.example`, `Tools/run-ubt.ps1` (all three targets: BreachpointEditor,
   Breachpoint, BreachpointServer), `Tools/run-specs.ps1`, `Tools/run-gauntlet.ps1`,
   `Tools/reimport-tables.ps1`. Owner: **builder**. Contracts: `testing.md`.
2. First pinned suite `Breachpoint.Sim.Combat` (`Source/Breachpoint/Tests/BRCombatSpec.cpp`):
   AR TTK vs 100/100, Magnum headshot math, shields-before-health order, invariants (health
   never regens, damage never negative) — all asserted AGAINST `DT_Weapons`/`CT_Combat`
   values, zero literals. Prove red-then-green with a deliberate broken value. Owner:
   **sim-builder**. (Depends on BP02's attribute set compiling — coordinate in Log.)
3. Gauntlet skeleton + smoke `BRGauntlet.SmokeTS2C`: dedicated server + 2 clients join
   `BR_Arena01` (blockout box is fine), client A damages client B, assert in threes (server
   truth, A's view, B's view), rerun under `-PktLag=120 -PktLoss=5`. Owner: **builder**,
   **netcode-builder** consults assertions.
4. Verifier runs rungs 1/2/4 from clean state, reports verbatim; rung 3 reported NOT WIRED
   (honest gap). Owner: **verifier**.
5. Critic prompt-hole review (REFUTER): can a spec pass asserting nothing? Can the smoke pass
   with replication broken (server-only assertions)? Owner: **critic**.

## Done when

- [ ] Rung 1: real pass/fail artifact from clean state, all three targets
- [ ] Rung 2: `Breachpoint.Sim.Combat` runs `-nullrhi -unattended`, proven red-then-green
- [ ] Rung 4: `BRGauntlet.SmokeTS2C` green incl. emulation variant; a deliberately broken
      replication change makes it fail (proven once)
- [ ] Critic findings addressed or explicitly waived in the Log
- [ ] Findings + decisions in this ticket's Log

## Notes

- Crew: builder (wrappers, Gauntlet) · sim-builder (specs) · netcode-builder (assertion
  consult) · verifier (runs) · critic (refutes wiring)
- Binary files owned: none
- Out of scope: CI wiring, rung 3 functional tests, any gameplay feature

## Log

(append findings here, dated, newest last)

**31 Jul 2026 — Kickoff condition 1 confirmed necessary, and currently UNMET.**

The Kickoff's requirement that `ENGINE_ROOT` point at a *source-built* UE 5.8 was verified
against this machine rather than taken on faith. It holds: the launcher 5.8.1 at
`D:\Program Files\UE_5.8` ships precompiled `UnrealEditor` and `UnrealGame` targets only —
no `UnrealServer` under `Engine/Intermediate/Build/Win64/x64/`, no `UnrealServer.exe`.
Three targets cannot be built from it. Full evidence in BP01's Log.

Machine inventory at time of writing — no source-built 5.8 existed:
`C:\UnrealEngine` is a source build but **5.5.1**; every other root (`D:\UE_5.5`,
`C:\Program Files\Epic Games\UE_5.0`–`5.6`, `D:\Program Files\UE_5.4`) is launcher-installed.
Source build of `5.8.1-release` started 31 Jul 2026 → `D:\UnrealEngine_5.8`.

This ticket stays BLOCKED on that build plus BP01 step 1. Rung 1 is not runnable before then,
and per the honesty law it is reported BLOCKED with this reason, never skipped.
