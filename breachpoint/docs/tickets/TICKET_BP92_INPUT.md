# TICKET — The owned Enhanced Input → InputTag → ASC bridge

> STATUS: open — cut 7 Aug 2026. Blocked on BP90 DONE. Runs in parallel with BP91;
> the ASC hand-off is a stub until BP93 lands.

Founder directive: input produces a **tag**, never a call. This is the seam that makes bots
and humans one API — a bot presses the same `InputTag` on the same ASC, and no privileged
path exists for either. A new ability must be a data row, not a line of binding code. Two
classes, one file pair, because they are never used apart.

**Ordering law:** none internally. The `PlayerController` relay is a stub returning early
until BP95 owns that class — do not build the controller here.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP90 is DONE
- `Core/BRGameplayTags.h` declares the `InputTag.*` family (BP91 step 1) — if BP91 has not
  landed, this ticket may declare the tags it needs and BP91 absorbs them, but the two must
  not both declare the same tag
- owner_path: `Source/Breachpoint/Input/`, `Content/Input/`, `Tools/`

## Steps (in order)

1. **[builder]** New `Input/BRInput.h/.cpp` — one pair, two `UCLASS`es:
   - `UBRInputConfig : UDataAsset` — two `TArray<FBRInputAction>` where
     `FBRInputAction = { TSoftObjectPtr<UInputAction> Action; FGameplayTag InputTag; }`.
     List one is **native** (Move, Look — handled by CMC directly), list two is
     **ability-driven** (everything else). The config maps hardware to tags and **never**
     names an ability class. Lookup helpers: `FindNativeAction(Tag)`, `FindAbilityAction(Tag)`.
   - `UBRInputComponent : UEnhancedInputComponent` — `BindNativeAction(Config, Tag, Event, Object, Func)`
     and templated `BindAbilityActions(Config, Object, PressedFunc, ReleasedFunc, TArray<uint32>& OutHandles)`.
     Every ability action binds exactly two functions, both carrying the tag. Handles are
     returned so unbind on unpossess is exact.
2. **[builder]** Wire the consumer stub in `Character/BRCharacter::SetupPlayerInputComponent`
   (the class is BP96's, so this is a **minimal, marked** stub: resolve the config, bind, and
   forward to two empty functions with a `// BP93: route to ASC` comment). BP96 replaces the
   stub; it does not rewrite the binding code.
3. **[builder]** Generate `Content/Input/IMC_Default`, the `IA_*` actions, and
   `DA_InputConfig` **by committed script** under `Tools/` (law: input assets are generated,
   never hand-placed). The script is the artifact; the assets are its output.
4. **[builder]** Local mapping context is pushed by the **PlayerController** on
   `OnPossess`/`OnRep_Pawn`, not by the pawn — a pawn that pushes its own IMC leaks the
   context across a respawn. Stub the call site; BP95 owns the controller.
5. **[verifier]** Rung 1 (three targets). Rung 3 functional: PIE, press each bound key,
   assert the matching `InputTag` reaches the stub exactly once per press and once per
   release. Rung 2 spec: `UBRInputConfig` with a duplicate tag in either list fails
   validation loudly.

## Done when

- [ ] `Input/` contains exactly one file pair (`BRInput.h/.cpp`) holding both classes
- [ ] `UBRInputConfig` names no ability class anywhere — `grep` for `UBRGameplayAbility` in
      `Input/` is empty
- [ ] Every `IA_*`, `IMC_Default` and `DA_InputConfig` is produced by a committed script
- [ ] Bind handles are returned and unbound on unpossess (no dangling binding after respawn)
- [ ] Rung 1 as above; rung 3 shows one press + one release per key, no doubles
- [ ] Rung 2: duplicate-tag config fails validation
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder throughout. sim-builder consults on the ASC seam signature only.
- Binary files this ticket OWNS: `Content/Input/*` (all generated — lock the generator, not
  the outputs).
- Out of scope: the ASC (BP93), the PlayerController (BP95), the character (BP96). Do not
  bind an ability here — there are none yet, and binding by class is the mistake this design
  exists to prevent.

## Log

(append findings here, dated, newest last)
