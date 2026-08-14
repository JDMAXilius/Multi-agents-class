# TICKET — Strip the UE template out of the runtime module, and settle the three rework decisions

> STATUS: in-progress — mac terminal 14 Aug 2026 (4cf813f)
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

- [ ] 36 template files are gone; `grep` for the four names returns nothing under `Source/`
- [ ] `Breachpoint.Build.cs` has zero `PublicIncludePaths`; the `Slate` verdict is in the Log
- [ ] No `Config/*.ini` references a deleted class
- [ ] D-1, D-2, D-3 each have a dated ruling in `docs/DESIGN-RULINGS.md`
- [ ] If D-1 taken: `gas-purity.md` ledger has a fourth row with its bound stated
- [ ] If D-2 taken: `Equipment/` exists, owner-path map updated, no stale `Weapons/` reference
- [ ] `BREACHPOINT-ARCHITECTURE.md` §3.1–§3.6, §3.11 carry SUPERSEDED pointers
- [ ] Rung 1: Editor + Game GREEN, Server PARTIAL-by-environment, stated as such
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder executes; founder rules on step 5; verifier runs rung 1.
- Binary files this ticket OWNS: none.
- Out of scope: writing **any** new gameplay code. This packet only removes and decides.
  Touching `UI/`, `AI/`, `Online/`, `Telemetry/` beyond an ini repoint is out of scope.

## Log

(append findings here, dated, newest last)
