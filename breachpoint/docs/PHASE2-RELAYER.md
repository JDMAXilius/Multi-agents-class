# PHASE 2 — Re-layering plan

**Status:** written, NOT executed. Phase 1 is a compiling baseline; nobody has yet observed
`ABRCharacter` move. Every step below is gated on step 0.

**Why this file exists.** Phase 1 collapsed our input stack toward the Shooter template's shape so
that "WASD is dead" would have one suspect instead of six. Nothing was deleted — each bypassed layer
is still compiled and reachable behind a switch. This is the ordered list of switches to flip back,
one at a time, each with the PIE observation that proves the layer survived. One step per test run;
two at once and a regression has two suspects again, which is the exact hole Phase 1 was dug to
escape.

**The rule for every step:** re-enable, run PIE, read the named log line, move the character. If the
line is wrong or movement stops, the step just taken owns it. Revert that one switch, write what you
saw in the ticket Log, and stop — do not proceed with a known regression to "come back to it."

---

## Step 0 — GATE: prove the Phase 1 baseline actually moves

Nothing below is meaningful until this passes. It has NOT been done.

1. Close the editor, run `Tools\run-ubt.ps1` (see "owed work" — the editor target has never linked
   with these changes; pid 32668 held the DLL open).
2. PIE into `/Game/Maps/BR_Arena01`, click inside the viewport, press W.
3. Required log lines, in order, from `LogBRInput`:
   - `BRPlayerController '…': added mapping context 'IMC_Default' … N key mapping(s).`
   - `BRPlayerController '…': added mapping context 'IMC_MouseLook' …` ← **new**; its absence means
     the ini pin still is not landing and mouse look is dead.
   - `BRCharacter '…': input bound on '…' via direct action refs (PHASE 1 BASELINE) — Move=1 Look=1 Mouse=1 Jump=2 Crouch=2 …`
   - `BRCharacter '…': movement ready on 'BRCharacterMovementComponent' — MaxWalkSpeed=…`
   - `BRCharacter '…': FIRST Move input (0.00, 1.00) …`

**Exit criterion:** WASD moves, mouse turns, Space jumps, C/Ctrl crouches. Record the
`movement ready` numbers in the ticket Log — later steps compare against them.

---

## Step 1 — Confirm the ini pins beat the Blueprint defaults (no code change)

**What it is.** Phase 1 made `ABRPlayerController` and `ABRCharacter` `config = Game` so
`Config/DefaultGame.ini` is authoritative. But the spawned classes are Blueprints (`PC_BR`,
`BP_BRcharacter`), and a Blueprint's *serialised* property beats the ini-loaded C++ CDO. Properties
the Blueprint predates (everything added this session) inherit the ini value; properties it already
saved (`InputConfig`, `DefaultMappingContext`) do not.

**Marker.** `Config/DefaultGame.ini:13-31` (the PC block's header comment).

**Re-enabling looks like.** Nothing to switch — this is a verification step, and it is first because
every later step assumes the ini is the source of truth.

**PIE observation that proves it.** Temporarily change `DefaultMappingContextPriority=0` to `7` in
`Config/DefaultGame.ini`, restart the editor (ini is read at CDO construction, not per-PIE), and
confirm the mapping-context log prints `at priority 7`. If it still prints `0`, `PC_BR` is
overriding it and the founder must clear that property on `PC_BR` in the editor. Set it back to `0`.

---

## Step 2 — Re-enable tag-based native verb binding through `UBRInputConfig`

**What it is.** The one deliberate architectural bypass. Phase 1 binds Move/Look/Jump/Crouch from
five `TSoftObjectPtr<UInputAction>` properties on the pawn (the template's shape, lawfully
expressed). ARCHITECTURE.md §3.2's permanent shape is that the pawn names *tags* and
`DA_InputConfig` maps tag → asset, so adding a verb is a data edit.

**Markers.**
- `Source/Breachpoint/Character/BRCharacter.h:171` — `bBindNativeVerbsFromInputConfig`, the switch.
- `Source/Breachpoint/Character/BRCharacter.h:125` — the five direct action properties.
- `Source/Breachpoint/Character/BRCharacter.cpp:249` — the branch.
- `Source/Breachpoint/Character/BRCharacter.cpp:326` — `BindNativeVerbsDirect`, the Phase 1 binder.
- `Config/DefaultGame.ini:38` — the five ini pins.

**Re-enabling looks like.** One ini line: `bBindNativeVerbsFromInputConfig=True` in
`[/Script/Breachpoint.BRCharacter]`. No recompile — both binders are always compiled. Leave the five
pins in place for this run: if the config path fails to resolve at all, the code logs an error and
falls back to the direct path rather than leaving the player unable to move.

**PIE observation that proves it.** The bind line changes to
`via DA_InputConfig tags (Phase 2 shape) — Move=1 Look=1 Mouse=1 Jump=2 Crouch=2` **and** WASD still
moves. The counts matter more than the words: `Move=0` means `DA_InputConfig` has no
`NativeInputActions` row carrying `InputTag.Move`, or its row's `IA_Move` will not load — and the
error line immediately below names which. `Mouse=1` must persist; mouse look is pinned outside the
config either way.

**Then, and only after that passes:** delete the five properties, `BindNativeVerbsDirect`, the
switch, and `Config/DefaultGame.ini:38-53`. That deletion is the actual end of Phase 2 step 2 — a
switch left in the tree becomes a second supported code path nobody tests.

---

## Step 3 — Restore `UBRInputComponent` as a hard requirement, or accept the stock cast permanently

**What it is.** `SetupPlayerInputComponent` used to `Cast<UBRInputComponent>` and return early on
failure, so one lost line in `Config/DefaultInput.ini` killed every key on every pawn. Phase 1 casts
to the stock `UEnhancedInputComponent` and only *warns* when the component is not ours.

**Marker.** `Source/Breachpoint/Character/BRCharacter.cpp:191-215` (the cast and the warning).

**Re-enabling looks like — a decision, not a switch.** The recommendation is **do not re-enable it.**
The binder functions only ever needed `UEnhancedInputComponent::BindAction`; the stricter cast bought
nothing and risked everything, and the template pawns that kept working cast to the stock type. Treat
this as a *permanent fix*, not Phase 1 debt. `UBRInputComponent` stays in the tree and stays
`DefaultInputComponentClass` — it is simply no longer load-bearing for movement. Revisit only if a
future packet adds a member that the pawn genuinely needs.

**PIE observation that proves the current state is right.** The warning
`input component is '…', not a UBRInputComponent` must be **absent**. If it appears, the
`DefaultInputComponentClass` line at `Config/DefaultInput.ini:110` is not being honoured — that is
now a diagnosable warning instead of a silent total input failure, which is the whole point.

---

## Step 4 — Ability input routing (blocked on GAS grants, not on Phase 1)

**What it is.** The eleven-action tag relay: every ability row binds to
`ABRPlayerController::AbilityInputTagPressed/Released` with its tag as payload. Phase 1 did not
change this and did not bypass it — it binds today.

**Marker.** `Source/Breachpoint/Character/BRCharacter.cpp` — the ability block, guarded by
`if (Config && BRController)`.

**Why it is not step 2.** It is not blocked by anything Phase 1 did. It is blocked because the ASC
has nothing granted: `ApplyInitStats` and the startup ability set are unbuilt (ticket **BP19 step
A0**). Every press currently reaches an ASC with no matching ability and correctly does nothing.

**PIE observation that proves it.** Deferred to BP19. When grants exist: press Fire and expect the
`AbilityInputTagPressed` path to log the tag, then the ability's own activation log. Testing this
before BP19 produces a false negative.

---

## Step 5 — Movement tuning numbers leave the Blueprint

**What it is.** `MaxWalkSpeed`, `JumpZVelocity`, `MaxWalkSpeedCrouched` currently live on
`BP_BRcharacter`'s CharacterMovement details panel, which is the *only* lawful place they can live
today — and it is a place C++ and ini cannot reach.

**Marker.** `Source/Breachpoint/Character/BRCharacter.cpp` — `LogMovementSnapshot()`, which reads and
reports these and sets none.

**Constraint discovered in Phase 1, worth not rediscovering.** `[/Script/Engine.CharacterMovementComponent]`
in an ini **does not work**: `UCharacterMovementComponent::MaxWalkSpeed` and `JumpZVelocity` are
declared `EditAnywhere, BlueprintReadWrite` with **no `config` specifier**
(`CharacterMovementComponent.h:274-275` and `:163-164`). An ini section for them is read by nobody.

**Re-enabling looks like.** A real design step, owned by BP02 (sprint) rather than by this recovery:
add rows to `Content/Data/CT_Combat.csv` and have `UBRCharacterMovementComponent` read base speeds
from the curve table the way `GetSprintSpeedMultiplier()` already does, so law 3 holds and the
numbers are data. Until then the Blueprint panel is correct and the snapshot log is the guard.

**PIE observation that proves it.** `movement ready … MaxWalkSpeed=…` matches the CSV row, and the
`CT_Combat has no usable '…' curve` error is absent.

---

## Known-safe, verified in Phase 1, needs no step

`UBRCharacterMovementComponent::GetMaxSpeed()` **cannot** return 0 when the `CT_Combat` lookup fails.
`GetSprintSpeedMultiplier()` gates on `Multiplier > 0.f` (`BRCharacterMovementComponent.cpp:202`) and
falls back to the identity `1.f` (`:214`), and the multiplier is only applied when sprint intent is
valid (`:222-229`). A missing CurveTable therefore costs the sprint bonus and logs an error; it
cannot zero walk speed. Verified by reading, not by PIE.
