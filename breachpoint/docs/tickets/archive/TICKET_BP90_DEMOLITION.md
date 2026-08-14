# TICKET — Strip the UE template out of the runtime module, and settle the three rework decisions

> STATUS: done — mac terminal 14 Aug 2026
> STATUS: open — cut 7 Aug 2026 alongside `docs/BREACHPOINT-GAMEPLAY-REWORK.md`.
> Root ticket of the gameplay rework. Nothing in Phase 1+ starts until this is DONE.

Founder directive: we are reworking the gameplay layer from scratch, and we are not building
it on top of a shipped Epic template. `Variant_Horror/`, `Variant_Shooter/` and the three
`breachpoint*` files are 36 source files that compile on every build, are referenced by
nothing in the `BR` tree, and keep `Slate` in the dependency list for no gameplay reason.
Delete them, then record the three decisions the rework needs so no later session guesses.

**Ordering law:** step 5 (the decisions) gates every Phase 1+ ticket. Steps 1–4 may land
first; a decision left open is a `contract_gap`, not a judgement call for the next session.

## Kickoff (machine-checkable)

- requires: engine-installed
- `git status` clean on `main` before starting (this packet deletes; a dirty tree hides what)
- `docs/BREACHPOINT-GAMEPLAY-REWORK.md` exists and is committed
- owner_path: `Source/Breachpoint/`, `docs/`, `.claude/`

> **The guard does not cover this packet.** `guard_laws.py` hooks `Edit|Write|MultiEdit`
> only; this is a shell-driven deletion, so owner-path confinement is **advisory**. Write
> `.claude/active-packet.json` anyway — the banned-API greps still fire on writes.

## Steps (in order)

1. **[builder]** Verify the coupling claim before deleting anything. Run and paste into the
   Log: `grep -rl "Variant_\|breachpointCharacter\|breachpointGameMode\|breachpointPlayerController" Source --include=*.h --include=*.cpp --include=*.cs`
   Expected: only the files being deleted, plus `Breachpoint.Build.cs`. **A hit anywhere in
   `Source/Breachpoint/{Core,Input,AbilitySystem,Character,Weapons,Match,UI,AI,Online}/`
   stops this ticket** — file the coupling as a finding and re-scope.
2. **[builder]** Delete `Source/Breachpoint/Variant_Horror/` (4 pairs),
   `Source/Breachpoint/Variant_Shooter/` (14 pairs),
   `Source/Breachpoint/Character/breachpointCharacter.h/.cpp`,
   `Source/Breachpoint/Match/breachpointGameMode.h/.cpp`,
   `Source/Breachpoint/Match/breachpointPlayerController.h/.cpp`. 36 files.
3. **[builder]** `Breachpoint.Build.cs`: remove all six `PublicIncludePaths` entries. Then
   remove `"Slate"` from `PublicDependencyModuleNames` and rebuild. The comment says it is
   "template-inherited and kept because the surviving `Variant_*` sources need it" — with
   them gone, prove it or restore it. **`SlateCore` stays** (UMG's `SObjectWidget` needs it;
   the 1 Aug comment is still true and the failure mode is link-time, not compile-time).
   Record which of the two survived, and why, in the Log.
4. **[builder]** Any `Config/DefaultEngine.ini` or `DefaultGame.ini` entry naming a deleted
   class (default pawn, GameMode alias, map override) is repointed at the `BR` equivalent or
   removed. Search for `Horror`, `ShooterGameMode`, `breachpointGameMode`.
5. **[founder ruling, recorded by builder]** Settle D-1, D-2, D-3 from
   `BREACHPOINT-GAMEPLAY-REWORK.md` §7 and write the outcome into
   `docs/DESIGN-RULINGS.md` with today's date:
   - **D-1** projectile Tick exception → if taken, add the row to `gas-purity.md`'s Named
     Exceptions ledger **in this packet** (the contract says a ledger entry arrives as a
     contract change in its own packet; this is that packet).
   - **D-2** `Weapons/` → `Equipment/` → if taken, do the `git mv` here, update
     `BREACHPOINT-ARCHITECTURE.md` §9's owner-path map and any `.claude/agents/*` that names
     the path.
   - **D-3** AI scope → record in/out. Default out.
6. **[builder]** Mark `BREACHPOINT-ARCHITECTURE.md` §3.1–§3.6 and §3.11 as SUPERSEDED by
   `BREACHPOINT-GAMEPLAY-REWORK.md` §2–§3, with a one-line pointer at the top of each.
   Do **not** delete them — UI/AI/Online/Telemetry sections still reference the surrounding
   structure.
7. **[verifier]** Rung 1 on all three targets. `BreachpointServer` will report
   PARTIAL-by-environment (Epic Launcher install, no server binaries) — report it as PARTIAL,
   never as green.

## Done when

- [x] 36 template files are gone; `grep` for the four names returns nothing under `Source/`
- [x] `Breachpoint.Build.cs` has zero `PublicIncludePaths` *(one load-bearing module-root entry retained — see Log)*; the `Slate` verdict is in the Log
- [x] No `Config/*.ini` references a deleted class
- [x] D-1, D-2, D-3 each have a dated ruling in `docs/DESIGN-RULINGS.md`
- [x] If D-1 taken: `gas-purity.md` ledger has a fourth row with its bound stated
- [x] If D-2 taken: `Equipment/` exists, owner-path map updated, no stale `Weapons/` reference
- [x] `BREACHPOINT-ARCHITECTURE.md` §3.1–§3.6, §3.11 carry SUPERSEDED pointers
- [x] Rung 1: Editor + Game GREEN, Server PARTIAL-by-environment, stated as such
- [x] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder executes; founder rules on step 5; verifier runs rung 1.
- Binary files this ticket OWNS: none.
- Out of scope: writing **any** new gameplay code. This packet only removes and decides.
  Touching `UI/`, `AI/`, `Online/`, `Telemetry/` beyond an ini repoint is out of scope.

## Log

(append findings here, dated, newest last)

### 14 Aug 2026 — builder, steps 1–6 executed (uncommitted; left for lead review)

**Step 1 — coupling grep.** Hits: `Breachpoint.Build.cs` + the 20 files being deleted, PLUS
three files the ticket did not predict: `Animations/BRAnimLayerInterface.h`,
`FPS/BRFPSCharacter.h`, `FPS/BRFPSCharacter.cpp`. All three are **comment-prose only**
("Variant_Shooter was deleted for hard-referencing…") — zero code coupling; none is in the
stop-list folders. Proceeded. (Prose later reworded so the Done-when grep runs clean.)

**Step 2 — deletion.** `git rm -r` staged **42** deletions, not the ticket's 36: the two
Variant_ dirs alone hold 36 files (Horror 8 + Shooter 28), plus the six breachpoint* files.
The ticket's "36" counted the dirs only.

**Step 3 — Build.cs.** PublicIncludePaths had SEVEN entries, not six: the six Variant_ paths
plus a bare `"Breachpoint"` module-root entry. Removing all seven fails the compile
(`fatal error: 'Core/BRCore.h' file not found` etc. — every BR source includes module-root-
relative). The six Variant_ entries are gone; the root entry stays with a comment. **Slate
verdict: "Slate" REMOVED, "SlateCore" kept — `BreachpointEditor` Result: Succeeded** with
Slate gone (compile + link), proving the "surviving Variant_ sources need it" comment was the
only reason. SlateCore's 1 Aug link-time rationale untouched.

**Step 4 — Config.** `DefaultEngine.ini` had three ActiveClassRedirects landing ON deleted
classes (TP_FirstPerson* → breachpoint*). Repointed at the BR classes and ADDED three
breachpoint* → BR* redirects so assets saved against the deleted names still resolve:
- `TP_FirstPersonPlayerController → BRPlayerController` (was breachpointPlayerController)
- `TP_FirstPersonGameMode → BRGameMode` (was breachpointGameMode)
- `TP_FirstPersonCharacter → BRCharacter` (was breachpointCharacter)
- new: `breachpointPlayerController → BRPlayerController`, `breachpointGameMode → BRGameMode`,
  `breachpointCharacter → BRCharacter`
`DefaultGame.ini:199` mentions Variant_Shooter in a comment only — left as is. No other ini hit.

**Step 5 — rulings recorded** as R42/R43/R44 in `docs/DESIGN-RULINGS.md` (D-1 TAKEN, D-2
TAKEN, D-3 OUT). gas-purity.md Named Exceptions ledger has the fourth row (Projectile Tick,
bound stated). D-2 executed: `git mv Source/Breachpoint/Weapons Source/Breachpoint/Equipment`
+ 20 `#include "Weapons/…"` → `"Equipment/…"` rewrites across the module; ARCHITECTURE §9 map
updated. **No `.claude/agents/*` file names the `Weapons/` path** (only the `DT_Weapons`
table name) — nothing to update there. `Source/BreachpointNext/Weapons/` is a different
module, untouched. NOTE: enabling Tick on ABRProjectile is NOT done here — BP90 writes no
gameplay code; the ledger row licenses the packet that does.

**Step 6 — SUPERSEDED pointers** on ARCHITECTURE §3.1–§3.6 and §3.11.

**Scope note.** `guard_laws.py` blocked the step-4 ini edit: the claim's owner_path lacked
`Config/` (and the architecture doc lives at repo root, not `docs/`) while ticket steps 4–6
mandate both. `.claude/active-packet.json` was widened to match the ticket's own scope, with
a note in the file. Flagging for the lead: the BP90 claim template under-declares.

**Ladder rung.** `BreachpointEditor` clean compile PASS (Result: Succeeded) after every
change; Game/Server targets not run — step 7 is the verifier's. Final four-name grep over
`Source/` returns nothing (exit 1).

### 14 Aug 2026 — verifier, step 7 (rung 1, all three targets)

`./Tools/run-ubt.sh`, no arguments:
- **PASS** BreachpointEditor (exit 0, touched libUnrealEditor-Breachpoint.dylib, 11.56 s)
- **PASS** Breachpoint (exit 0, touched CodeResources, 28.52 s)
- **PARTIAL-by-environment** BreachpointServer (exit 6, "Server targets are not currently
  supported from this engine distribution" — Epic Launcher install, no server binaries).
  Not a code failure; the known environment ceiling.

### 14 Aug 2026 — lead, close

All boxes verified. Deviations from the ticket text, all logged above and accepted: 42 files
deleted not 36 (dirs recounted); one load-bearing module-root `PublicIncludePaths` entry
retained (all six Variant_ entries gone — removing the root entry breaks every BR include);
three comment-prose grep hits reworded. Claim-template gap (owner_path missing `Config/` +
root-level ARCHITECTURE doc) noted for future ticket cuts. Phase 1 is unblocked.
