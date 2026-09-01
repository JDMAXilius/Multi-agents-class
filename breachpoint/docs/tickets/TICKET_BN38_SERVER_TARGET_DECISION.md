# TICKET — Decide the dedicated-server target: source-build the engine, or retire rung 4a

> STATUS: open — cut by the mac terminal 31 Aug 2026 (during BN24), as a founder decision
> that BN24 was not allowed to make for itself. Needs a founder ruling on step 1 before any
> code or contract moves.

Founder directive: `BreachpointServer` has never compiled on this machine. During BN24's
rung-1 re-verify it failed again, and the question was raised out loud — *"BreachpointServer
is not even being used or working, could we remove it safely?"* The honest answer is that
removing it is not a build fix, it is a **contract amendment that deletes rung 4a**, and that
is a founder call. This ticket makes the call explicit and then executes whichever way it goes.

**Ordering law:** step 1 is a founder ruling and gates every other step. Nothing here touches
`Source/*.Target.cs` or `docs/contracts/` until step 1 is answered in this ticket's Log.

## The facts, so the decision is made on evidence and not on frustration

1. **The target is not broken; the engine distribution is.** `Tools/env.local` reads
   `ENGINE_ROOT=/Users/Shared/Epic Games/UE_5.8` — an Epic Launcher install. UBT's own words,
   31 Aug 2026: *"Server targets are not currently supported from this engine distribution."*
   Exit 6, 0.87s — it never reached a compiler. `run-ubt.sh` prints a LAUNCHER INSTALL warning
   before it starts and refuses to route around it, which is correct behaviour, not a bug.
2. **`docs/contracts/testing.md:114-115` already says exactly this** — the `BreachpointServer`
   target requires a **source-built** engine, not the launcher install. The contract predicted
   this failure. Nothing new was discovered; a known precondition is simply unmet.
3. **`~/UnrealEngine` exists but has no built Mac binaries** (`Engine/Binaries/Mac` is empty),
   so the source build is started-and-abandoned, not absent.
4. **Retiring the target deletes rung 4a, which is the DEFAULT netcode topology.**
   `testing.md:33` — *"4a — dedicated server + 2 clients. The default; every netcode packet
   runs it."* R30 (`DESIGN-RULINGS.md:388`) makes 4a and 4b a deliberate pair and states why
   4a stays the default: **on a dedicated server no player is ever the authority**, which is
   what keeps the Phase-2 dedicated move a config change instead of a rewrite. There is no
   listen-server substitute for that property — 4b cannot cover it *by construction*, which
   is the same argument R30 uses in the other direction for 4b.
5. **It is also the only target that compiles `WITH_SERVER_CODE` with no editor and no client
   rendering.** Editor+Game cannot catch editor-only or client-only code leaking into shared
   runtime; the server target is structurally the only build that can.
6. **Cost of keeping it: one `.Target.cs`.** Cost of deleting it: rung 4a, R30's 4a half, and
   `testing.md`'s "all three compile on every rung-1 run" (`:102`) all become false at once.

**Recommendation to the founder: option A.** The red is accurate and it is one environment
change from green. Deleting the target converts a true red into a silent green — the exact
failure mode the honesty ladder exists to prevent.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: **files-only for step 1** (a ruling written into this Log) ·
  **engine-installed for steps 2–4** (a source-built UE 5.8 is the thing being produced)
- `Source/BreachpointServer.Target.cs` exists (it does — this ticket may delete it, but only
  under option B and only after step 1)
- Step 1 answered in this ticket's `## Log`, signed, before any step 2+ work is claimed
- owner_path: `Source/` `Tools/` `docs/contracts/` `docs/tickets/`
  <!-- deliberately wide because option B edits contracts; under option A only Tools/ moves -->

## Steps (in order)

1. **Founder ruling — pick A or B, write it in the Log with one line of reasoning.**
   - **A · Keep the target, fix the engine.** Rung 4a stays real. Costs a source build.
   - **B · Retire the dedicated-server target.** Rung 4a goes with it, and every contract in
     step 4 must be amended in the same commit — a ruling that leaves `testing.md:102` and
     R30 standing while the target is gone is worse than either option.

   *Steps 2–3 are option A. Step 4 is option B. Do not do both.*

2. **(A) Build UE 5.8 from source** at `~/UnrealEngine` — `Setup.sh`, `GenerateProjectFiles.sh`,
   then the `UnrealEditor` Mac target. Confirm `Engine/Build/SourceDistribution.txt` exists;
   that file is the exact thing `run-ubt.sh` looks for when it prints the LAUNCHER warning.
3. **(A) Repoint `Tools/env.local`** at the source build, then `./Tools/run-ubt.sh` and expect
   **three** PASS lines. This is the only step that proves the claim — a green Editor+Game with
   Server still failing is the state we are already in, and is not what this ticket produces.
   Then re-open BN24's "all three targets compile" box, which was left unchecked for this reason.
4. **(B) If and only if the ruling is B** — one commit, all of it or none:
   - delete `Source/BreachpointServer.Target.cs`
   - `Tools/run-ubt.sh` and `Tools/run-ubt.ps1`: drop `BreachpointServer` from `ALL_TARGETS`
     and delete the launcher-install warning block that exists only to explain its failure
   - `docs/contracts/testing.md:102` — "all three compile" becomes two, named
   - `docs/contracts/testing.md:114-115` — delete the source-build precondition it no longer has
   - `docs/contracts/testing.md:33-38` — rung 4a is struck, and the text must say what replaces
     it for proving *authority does not depend on a local player*, or say plainly that the
     project stops proving that
   - `docs/DESIGN-RULINGS.md` R30 — cannot be silently edited (law 8: rulings are closed).
     Retiring half of R30 needs a NEW dated ruling that supersedes it, written by the founder
   - `Tools/run-gauntlet.ps1` — `BRGauntlet.SmokeTS2C` is a dedicated-server scenario; it dies
     with the target and its replacement (or its removal) is named here
5. **Verifier** re-runs `./Tools/run-ubt.sh` and reports the summary block verbatim into the
   Log — three PASS under A, two PASS under B. Under B the critic reads step 4's commit as a
   REFUTER pass with one question: *does any surviving line in the repo still claim a
   dedicated-server property we no longer test?*

## Done when

- [ ] Step 1 ruling written in this Log, A or B, with reasoning
- [ ] (A) `~/UnrealEngine` has `Engine/Build/SourceDistribution.txt` and built Mac binaries
- [ ] (A) `Tools/env.local` points at it; `./Tools/run-ubt.sh` prints **three** PASS lines
- [ ] (A) BN24's "all three targets compile with the QA files in" box checked, citing this run
- [ ] (B) Every bullet in step 4 landed in ONE commit, including the superseding R30 ruling
- [ ] (B) No surviving line in `docs/` claims a dedicated-server property the repo no longer tests
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: founder rules step 1 (nobody else may) · builder executes 2–3 or 4 · verifier runs
  rung 1 and reports the block verbatim · critic REFUTES under option B only
- Binary files this ticket OWNS (lock before editing): none
- Out of scope: making `run-ubt.sh` treat the server failure as INCONCLUSIVE or SKIP to get a
  green. That is neither option — it is the silent-green failure mode both options exist to
  avoid, and the script's author refused it on purpose in the header comment.
- Honesty: under A, "three targets compile" is claimable only after the run in step 3 is seen.
  Until then the standing claim remains "Editor+Game compile; Server blocked by distribution".

## Log

**31 Aug 2026 — mac terminal, cut during BN24.** BN24's rung-1 re-verify produced
`BreachpointEditor PASS · Breachpoint PASS · BreachpointServer FAIL (exit 6)`. Filed as a
`contract_gap` there rather than routed around, because `Source/BreachpointServer.Target.cs`
is outside BN24's owner path (law 5) and because the fix is a founder decision, not a build fix.
Facts 1–6 above were gathered before the recommendation was written; fact 4 is the one that
changed the answer — the first read of this was "nothing uses the target today", which is true
of the *binary* and false of the *contract*: 4a is the declared default for every netcode
packet, and it is unrunnable rather than unused.
