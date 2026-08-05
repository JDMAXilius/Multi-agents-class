# TICKET — BP82: get rung 1 green again in `Source/Breachpoint/UI/`

> STATUS: in-progress — windows terminal 5 Aug 2026 (cead6d9). Rung 1 is red on a C4458 shadow in
> `BRButton.cpp`; every UI packet downstream is blocked behind it, including BP81's "no glyph has
> been rendered yet" box and BP79's editor work.

Cut from BP81's contract_gap (`cead6d9`). The button-module merge (`2078499`, "BRHighlightButton
merges too — button source is now ONE file pair") landed a parameter that shadows a CommonUI
member, and this project builds warnings-as-errors, so the whole module fails to compile:

```
BRButton.cpp(487,40): Error C4458 : declaration of 'bSelected' hides class member
CommonButtonBase.h(934,8): note: see declaration of 'UCommonButtonBase::bSelected'
```

No asset packet can claim a rung while this stands — an imported texture that cannot be compiled
into a running editor cannot be looked at, which is the box BP81 could not tick. This ticket owns
the C++ **only**; it changes no behaviour and no numbers.

**Ordering law:** this gates BP81's remaining render boxes and BP79's `run-ubt` kickoff condition.
Nothing gates it.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- `Source/Breachpoint/UI/Components/BRButton.cpp` exists and contains `ApplySelectedMark`
- `./Tools/run-ubt.ps1` currently FAILS with `C4458` (if it passes, this ticket is already moot —
  do not claim it, close it)
- owner_path: `Source/Breachpoint/UI/`, `docs/tickets/TICKET_BP82_UI_SOURCE_RUNG1_UNBLOCK.md`

## Steps (in order)

1. **Rename the shadowing parameter.** `ApplySelectedMark(bool bSelected)` →
   `bInSelected`, at `BRButton.cpp:487`, its single use at `:494`, and the declaration at
   `BRButton.h:338`. `bIn…` is the prefix this file already uses for exactly this reason —
   `ApplyInversionToSubtree(UWidget*, const FSlateColor& InTextColor, bool bInverted)` at
   `BRButton.h:335`. Behaviour-neutral: the parameter is read once, to pick `TypeCheckMark`'s
   visibility. Owner: `ui-builder`. Binds: laws 4 (no Tick) and 5 (owner path).
2. **Re-run rung 1 and fix ONLY what is the same class of defect.** `./Tools/run-ubt.ps1` for all
   three targets. BP80's log records 7 pre-existing compile failures; if more shadows or
   warnings-as-errors surface inside `Source/Breachpoint/UI/`, they are in scope. Anything
   outside that folder is NOT — file a contract_gap and stop, per law 5.
3. **Verifier runs the ladder** (`contracts/testing.md` rung 1: all three build targets, then
   `run-specs.ps1`). Reports verbatim. A compile is rung 1 and nothing more — no PIE, no
   multiplayer claim comes out of this ticket.

## Done when

- [ ] `./Tools/run-ubt.ps1` PASSES on Breachpoint, BreachpointEditor, BreachpointClient
- [ ] `run-specs.ps1` reports no NEW failures versus the pre-fix baseline
- [ ] No file outside `Source/Breachpoint/UI/` was modified (`git diff --stat` in the Log)
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: `ui-builder` writes step 1; `verifier` runs step 3. No critic — there is no attack
  surface in a parameter rename.
- Binary files this ticket OWNS (lock before editing): none. This is a text-only packet.
- Out of scope: **`BRHUDDirector`'s cross-collection `InitializeDependency`** — the `high` finding
  BP80 logged at its line 405-417. It lives in this owner_path but is a real behavioural
  decision (the ordering the comment wants may need re-establishing, not deleting), and mixing it
  into a compile unblock would hide it. It needs its own packet and a `ui-builder` ruling.
  Also out of scope: the 36 modified `.uasset` glyph textures sitting uncommitted in the tree.

## Log

(append findings here, dated, newest last — this is what the next session reads)

### 5 Aug 2026 — windows terminal — step 1 done, rung 1 GREEN, rung 2 has two pre-existing reds

**Step 1 landed.** The rename is 3 lines across 2 files and nothing else:

```
Source/Breachpoint/UI/Components/BRButton.cpp | 4 ++--   (:487 signature, :494 use)
Source/Breachpoint/UI/Components/BRButton.h   | 2 +-     (:338 declaration)
```

`bInSelected` matches the prefix the file already used for the same reason at `BRButton.h:335`.
Behaviour-neutral — the parameter is read once, to choose `TypeCheckMark`'s visibility.

**Step 2 — scanned for siblings, found none.** Grepped `Source/Breachpoint/UI/` for parameters
named after `UCommonButtonBase` bool members. `BRFeatureCard::ApplyHoverState(bool bHovered)`
(`BRFeatureCard.cpp:161`) looked like a candidate — `UBRFeatureCard` does derive from
`UCommonButtonBase` — but `bHovered` is **not** a member of that class (the bitfield block at
`CommonButtonBase.h:878-940` has no such field), so it does not shadow and was left alone.
`BRItemTile.h:70`'s `bool bLocked` is a member declaration, not a local; C4458 does not apply.

**Rung 1: PASS, all three targets** — run stamp `20260805-123143`, incremental.

| target | exit | artifact mtime | newer than start | verdict |
|---|---|---|---|---|
| BreachpointEditor | 0 | 2026-08-05T12:32:03.471 | YES | PASS |
| Breachpoint | 0 | 2026-08-05T12:33:21.813 | YES | PASS |
| BreachpointServer | 0 | 2026-08-05T12:34:25.964 | YES | PASS |

**Rung 2: 83 pass, 2 fail** (`Tools/Logs/specs-20260805-123441.log`). Both failures are the SAME
root cause and neither is reachable from this packet's diff:

- `Breachpoint.Sim.Shields.the ShieldsBroken transition.refuses to mark an uninitialised fighter
  as broken` — `BRShieldSpec.cpp:649`: *expected `GetMaxShields()` 0.0, got 100.0*
- `Breachpoint.Sim.Combat.UBRAttributeSet clamps.clamps Health and Shields against an
  UNINITIALISED capacity of zero`

A freshly spawned fighter now arrives with `MaxShields = 100` where both specs assert it must be
0 until `GE_InitStats` runs. That is an attribute-set/default-value change in the sim, and this
packet's entire diff is a parameter name inside a CommonUI widget's checkmark visibility —
there is no causal path between them.

**contract_gap — the "no NEW failures vs the pre-fix baseline" box cannot be satisfied as
written.** A pre-fix baseline is unobtainable: before this packet the module did not compile, so
rung 2 could not run at all. There is nothing to diff against. The honest statement is the one
above — the two reds are in `AbilitySystem`/sim code that this diff does not touch, so I am
recording them as pre-existing on reasoning, NOT on a measured baseline. Whoever picks up the
shield defect can establish the real baseline by running rung 2 at `2078499`.

`Source/Breachpoint/AbilitySystem/` is outside this owner_path; per law 5 nothing was touched.
**It needs its own packet** — and it is the more serious of the two findings now open in
`Source/`, alongside BRHUDDirector's `InitializeDependency` (BP80, line 405).

**Rung honesty:** rung 1 PASS and rung 2 83/85 are what happened. Nothing was launched, no PIE,
no multiplayer. The button whose parameter this was has not been clicked.
