# TICKET — BP71: Compile the eight audit packets. Nothing downstream is claimable until this is green.

> STATUS: open — cut 3 Aug 2026 after the menu and HUD audits landed eight packets across nine
> commits (`2a190df`..`b2eaaf8`). **engine-installed.** Every one of those commits is rung 0:
> written in a cloud container with no UE toolchain, grep-verified, compiled by nothing.

Founder directive, restated from law 6: *compiles ≠ works*. This ticket claims only the first
rung, and it exists because eight packets of unread C++ is the largest single risk on the board.
The changes are not speculative — each was pattern-matched to adjacent proven code and every
deletion was grep-verified before and after — but **nothing has read them back**, and a compiler
is the cheapest possible disagreement with that work.

**Ordering law:** this ticket gates BP72 and the verification half of BP74. Claim it first, and
do not claim it on a machine that cannot run `Tools/run-ubt.ps1` for all three targets.

## Kickoff (machine-checkable)

- requires: **engine-installed** — source-built UE 5.8; headless is fine, no editor needed
- `git log --oneline` shows `b2eaaf8` (HUD-A) as an ancestor of HEAD
- `python3 Tools/gen_ui/wbp_plan.py` prints `PLAN OK` (the plan parses the *new* headers —
  it already did at authoring time, and a header that moved since would fail here first)
- owner_path: `Source/Breachpoint/UI/`, `Source/Breachpoint/Breachpoint.Build.cs`,
  `docs/tickets/TICKET_BP71_UI_COMPILE_GATE.md`

## Steps (in order)

1. **Editor target first**, because it is the one that surfaces UHT errors fastest:
   `Tools/run-ubt.ps1 -Target BreachpointEditor`. The high-risk surfaces, in the order they
   will break:
   - `BRViewModels.h` — `FBRCombatAttributeBindings` moved here from `BRUITypes.h`; seven
     `FMVVMEventField`s and `FBROnKillfeedEntryAdded` were deleted. Any file that referenced a
     cut field fails here.
   - `BRUITypes.h` — `AttributeSet.h` include removed. Anything that got `FGameplayAttribute`
     transitively through this header now fails, and **that is the finding, not a regression**:
     it names the file that should have included it directly.
   - `BRHUDLayout.h` — `UBRKillfeedEntryWidget` gained four `BindWidget` members and lost six
     BIEs; `UBRHUDLayout` lost its hit-marker and state subscriptions.
   - `BRMatchBand` / `BRKillfeed` — re-based `UBRActivatableWidget` → `UCommonUserWidget`.
     `Super::BindViewModels()` calls are gone; anything still calling them fails.
   - `BRHUDDirector.cpp` — **~380 lines that have never been parsed.** It reaches into
     `Match/`, `Weapons/` and `AbilitySystem/` headers this session read but never compiled
     against: `ABRGameState`'s four delegate signatures, `UBREquipmentComponent::GetWeaponInSlot`,
     `UBRWeaponInstance::GetRow`, `UBRAttributeSet`'s `ATTRIBUTE_ACCESSORS_BASIC` statics,
     `UWorld::GameStateSetEvent`, `APlayerController::OnPossessedPawnChanged`.
   - `BRAmmoBlock` — `EBRAmmoReadoutState` lost `Low` and `Battery`; any switch on them fails.
2. **Then the other two targets**: `Breachpoint` (Development) and `BreachpointServer`. The
   server target matters here even though this is UI work — `UBRHUDDirector` is a
   `ULocalPlayerSubsystem` and must not drag client-only UI symbols into a server build.
3. **Fix forward, do not revert.** Every error this finds is a real defect in an unverified
   packet. Record each one in the Log with its file:line and what it proves — that record is
   the honest measure of how much a cloud-authored packet can be trusted, and the next audit
   pass is calibrated by it.
4. `Tools/run-specs.ps1` — the existing `Breachpoint.Sim.*` suites. **They do not cover the UI**
   and are not expected to; running them proves the eight packets broke nothing they do cover.
5. Verifier records the three target results verbatim in the Log. **No rung above 1 is claimable
   from this ticket** — a compiled HUD is not a rendered one.

## Done when

- [ ] `BreachpointEditor`, `Breachpoint`, `BreachpointServer` all compile clean
- [ ] Every compile error found is recorded in the Log with file:line and its cause
- [ ] `run-specs.ps1` is green (or its failures are proven pre-existing on `a249800`)
- [ ] The Log states the rung reached in one sentence, and it is **rung 1**
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: **verifier** owns the run; **ui-builder** fixes UI-folder errors; **netcode-builder**
  consults on anything `BRHUDDirector` hits in `Match/` or `Weapons/` — that file is the only
  one in this pass that touches gameplay-owned headers.
- Binary files this ticket OWNS: none. It compiles; it authors no asset.
- Out of scope: opening the editor (that is BP72), building any WBP, any PIE claim, and
  **reverting an audit packet because it does not compile** — fix it forward or file the
  finding.
- **Expect `BRHUDDirector.cpp` to carry most of the errors.** It is the newest file, the only
  one crossing module folders, and the only one with no adjacent proven twin to have been
  pattern-matched against.

## Log

(append findings here, dated, newest last)
