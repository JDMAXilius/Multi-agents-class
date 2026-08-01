# TICKET — BP02: GAS core — ASC port, attributes, generic effect library

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP01 (skeleton + tags exist).

Founder directive: this is the heart. Port OUR ASC (not Lyra's), one attribute set, one damage
pipeline, and the **generic-GE library** — one damage GE, one regen GE, one cooldown GE,
parameterized by SetByCaller + tags. GAS purity is law: costs/cooldowns are GEs, cues carry
all FX, nothing touches an attribute directly.

**Reference skills:** `gas-purity` throughout · `cmc-prediction` for step 3's `BRGA_Sprint`,
which is the first saved-move work in the project. **`cmc-prediction` is an UNVERIFIED draft
and carries an open decision this packet should settle:** compressed flags (4 custom bits
total, sprint and grapple spend two) vs. the structured move-data path. Record the choice as a
ruling — switching later touches every movement state at once.

**Ordering law:** Steps 1→2→3 strictly (each compiles against the previous). Step 4 parallel
with 3. BP00 step 2 (combat spec) lands against this ticket's output.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- Ticket BP01 is DONE and rung 1 is green (all three targets compile from clean)
- `Source/Breachpoint/Core/BRGameplayTags.h` exists and defines the `Ability.*`,
  `State.*`, `SetByCaller.*` and `Event.*` families this ticket applies
- owner_path: `Source/Breachpoint/AbilitySystem/`, `Source/Breachpoint/Data/`

## Steps (in order)

1. Port `BRAbilitySystemComponent` from our existing GAS codebase: input-buffered tag activation
   (`AbilityInputTagPressed/Released`), prediction-window helpers. Set
   `ReplicationMode::Mixed`; enable `ServerAbilityRPCBatch`. Wire `BRPlayerState` to own
   ASC + set (NetUpdateFrequency raised per ARCHITECTURE §5.4); `BRCharacter` forwards
   `IAbilitySystemInterface` and calls `InitAbilityActorInfo` on possess/respawn.
   Owner: **sim-builder**, **netcode-builder** reviews the replication settings.
2. `BRAttributeSet` (Shields/MaxShields/Health/MaxHealth/IncomingDamage-meta):
   `PreAttributeChange` clamps; `PostGameplayEffectExecute` = shields-first application,
   `State.Combat.RecentDamage` application, death detection → `Event.Death` + delegate.
   Owner: **sim-builder**.
3. `BRGameplayAbility` base (activation policy enum, cost/cooldown hooks, cancel hygiene) +
   `BRAbilitySet` (UDataAsset, **soft** class refs + InputTag, grant/revoke handles) +
   `BRDamageExecCalc` (SetByCaller.BaseDamage + `Damage.*` tag multipliers from `CT_Combat`,
   shields absorb → overflow) + **`BRGA_Sprint`** — the first concrete ability and the
   WhileHeld-policy prover: hold InputTag.Sprint → activate, release → end; grants
   `State.Movement.Sprinting` (ActivationOwnedTags); CMC reads it via the `FSavedMove_BR`
   flag and applies the `CT_Combat` speed multiplier; no cost. Proves the whole
   input → tag → ASC → ability → CMC chain end to end. Owner: **sim-builder**.
4. The generic-GE **C++ classes** (`UGameplayEffect` subclasses in
   `Source/Breachpoint/AbilitySystem/Effects/` — **not Content assets**, ruling R18):
   > ⚠️ **Prove one before writing six.** Constructor-authoring a `UGameplayEffect` is
   > legal but runs against Epic's grain — since UE 5.3 effect behaviour lives in
   > `UGameplayEffectComponent` instanced subobjects the editor is built to author, so a
   > C++ version means `CreateDefaultSubobject` per component and populating `GEComponents`
   > by hand. **Land `GE_RecentDamage` first** (the simplest: one duration, one granted
   > tag), prove it compiles and applies at runtime, and record the pattern in the Log.
   > If it does not work cleanly, that is a `contract_gap` against R18 — stop and escalate;
   > do not quietly fall back to assets.
   `GE_Damage` (SetByCaller + dynamic tags), `GE_Regen` (periodic, SetByCaller.RegenRate,
   activation-blocked by RecentDamage — configured for shields at 60/s after 2.5 s),
   `GE_Cooldown` (SetByCaller.CooldownDuration), `GE_InitStats` (curve row),
   `GE_RecentDamage` (2.5 s tag), `GE_Death` (infinite `State.Dead`; ability base blocks
   activation on it — the one death mechanism). Tables: `DT_MatchRules` gets a row struct in
   `BRDataRows.h`; **`CT_Combat` is a CurveTable — named curves, no row struct, not in
   `BRDataRows.h`** (`data-and-assets.md`). Owner: **sim-builder**.
   Contracts: `data-and-assets.md`, `gas-purity.md`.
5. Verify + refute: rungs 1–2 (BP00's spec now runs against real code, red→green);
   **critic REFUTER** on the attribute pipeline: negative damage, damage while dead,
   regen-while-damaged race, double-death event. Owner: **verifier**, **critic**.

## Done when

- [ ] Damage applied via `GE_Damage` breaks shields then health, exactly per `CT_Combat`
      (spec-proven, red-then-green)
- [ ] Shield regen starts at 2.5 s, 60/s, blocked by RecentDamage (spec-proven)
- [ ] Death fires exactly one `Event.Death` and applies `GE_Death`/`State.Dead` (all verbs
      blocked while dead — spec-proven); attributes clamp; no direct attribute writes and no
      engine-damage-API calls anywhere (grep-audited per `gas-purity.md`)
- [ ] ASC survives pawn destruction (PlayerState-owned, re-inits on possess — PIE-proven)
- [ ] Sprint: WhileHeld flows input → ASC → tag → CMC speed, predicted, and replays
      correctly through a forced server correction (multi-process PIE + net emulation)
- [ ] Critic findings addressed or waived in the Log

## Notes

- Crew: sim-builder leads · netcode-builder reviews replication settings · verifier proves ·
  critic refutes
- Binary files owned: **none** — the six generic GEs are C++ classes under R18, so this
  ticket adds no `.uasset`. `Content/Data/CT_Combat.csv` is text (a CurveTable source).
- Out of scope: any weapon, any ability beyond the base class and BRGA_Sprint, UI

## Log

(append findings here, dated, newest last)

**31 Jul 2026 — PRE-FILED CONTRACT_GAP (lead, from the BP01 session): this ticket's
`owner_path` cannot reach five of its own deliverables.** Filed before the packet claims so it
is not rediscovered mid-build. Derived mechanically from `guard_laws.py`, not from memory: an
owner entry `o` matches only when `rel == o` or `rel.startswith(o + "/")`, and confinement
fires on every path under `Source/` or `Content/`.

Current: `Source/Breachpoint/AbilitySystem/`, `Source/Breachpoint/Data/`

| Deliverable | Lives in | Status |
|---|---|---|
| `BRPlayerState` (step 1) — ARCHITECTURE §3.6 says **"ASC + set live here"** | `Source/Breachpoint/Match/` | BLOCKED |
| `BRCharacter` `IAbilitySystemInterface` forward + `InitAbilityActorInfo` (step 1) | `Source/Breachpoint/Character/` | BLOCKED |
| `BRGA_Sprint`'s CMC half — `FSavedMove_BR` flag + speed multiplier (step 3) | `Source/Breachpoint/Character/` | BLOCKED |
| Rung-2 specs, red→green (step 5) | `Source/Breachpoint/Tests/` | BLOCKED |
| `CT_Combat` CurveTable + `DT_MatchRules` values (step 4) | `Content/Data/` | BLOCKED |

`Source/Breachpoint/Data/` IS owned, so `BRDataRows.h` is reachable — the row structs are fine;
only the CSV/CurveTable *values* are out of reach.

The first row is the sharp one: the ASC this ticket exists to port lives in a folder it does not
own. This is not a scoping quibble — it is the packet's center.

*Not fixed here.* The lead amends `owner_path` at claim time, when the packet's real shape is
settled, and records the amendment in the Kickoff block the way BP01 did twice. Two candidate
shapes, to be decided then, not now: grant the exact files (tight, verbose, precedent set by
BP01's three `.Target.cs` entries), or split the CMC/Character work into a handoff packet for
the builder who owns `Character/`. Do not widen to `Source/Breachpoint/`.

See also the systematic note in BP03's Log: **`Tests/` is in no packet's `owner_path` at all**,
and four separate tickets need to write there.

---

**31 Jul 2026 — PROPOSED `owner_path` FOR THIS TICKET'S CLAIM (lead).** Assembled from
everything BP01 discovered while building the seams BP02 plugs into. Written now so the claim is
an amendment, not a rediscovery — and so BP02's builder never has to guess.

```
Source/Breachpoint/AbilitySystem/                    (already granted)
Source/Breachpoint/Data/                             (already granted)
Source/Breachpoint/Match/BRPlayerState.h  .cpp       NEW  — §3.6: "ASC + set live here"
Source/Breachpoint/Match/BRPlayerController.h .cpp   NEW  — replace BP01's two stubs
Source/Breachpoint/Character/BRCharacter.h .cpp      NEW  — GetAbilitySystemComponent + init
Source/Breachpoint/Character/BRCharacterMovementComponent.h .cpp  NEW — BRGA_Sprint's CMC half
Source/Breachpoint/Core/BRGameplayTags.h  .cpp       NEW  — R23: Ability.* tags
Content/Data/CT_Combat.csv                           NEW  — CurveTable values
Content/Data/DT_MatchRules.csv                       NEW
```

Exact files, never folders, wherever the grant reaches into another discipline — the precedent
BP01 set with its three `.Target.cs` entries, for the same reason: `Character/` and `Match/`
belong to other packets and BP02 needs precisely four files between them, not the folders.

**What BP01 left BP02, stated by the builder that left it** (so the seam is a handoff, not an
archaeology exercise):
1. Author `ABRPlayerState` owning the ASC + attribute set.
2. Implement `ABRCharacter::GetAbilitySystemComponent()` — BP01 shipped it returning `nullptr`.
3. Call `InitAbilityActorInfo(PS, this)` from **BOTH** `PossessedBy` (server) **and**
   `OnRep_PlayerState` (client). Either alone leaves a client ASC with no avatar — a bug that
   is invisible in single-process PIE and only appears with a real client. Exactly the
   silent-and-confident class the netcode doctrine exists for.
4. Replace the two controller stub bodies with the ASC relay. **Do not re-declare the handlers'
   signature** — `AbilityInputTagPressed(FGameplayTag)` takes its tag BY VALUE because Enhanced
   Input's variadic `BindAction` payload deduces `VarTypes = FGameplayTag`; a `const&` matches
   none of the three handler shapes and fails overload resolution.
5. **Idempotency:** the pressed handler is bound to `ETriggerEvent::Triggered`, so it fires every
   frame the key is held. The ASC's input buffer must be `AddUnique`-shaped. BP01 has a
   `LoggedHeldInputTags` set in `BRCharacter` that is a **diagnostic de-duplicator only** —
   **delete it**, do not promote it to state. Two copies of held-input state is precisely the
   drift class this project keeps paying for.

**Still unresolved and NOT fixable at claim time — `Source/Breachpoint/Tests/`.** BP02 step 5
needs specs there; the folder is currently inside **BP00's** `owner_path` and four tickets want
it. See BP03's Log for the full analysis and the three options. If BP00 has closed by the time
BP02 claims, the folder is free and this is a simple grant; if not, it is a genuine collision and
the lead sequences it.

---

**1 Aug 2026 — STEPS 1 AND 2 BUILT (sim-builder).** ASC + attribute set + PlayerState authored;
all five of BP01's handoff items closed. No rung is pronounced here — build results below are
observations.

Files: `AbilitySystem/BRAbilitySystemComponent.h/.cpp`, `AbilitySystem/BRAttributeSet.h/.cpp`,
`Match/BRPlayerState.h/.cpp` (new); `Character/BRCharacter.h/.cpp`,
`Match/BRPlayerController.h/.cpp` (edited). `Core/BRGameplayTags.h/.cpp` NOT touched — steps 1–2
introduce no ability and no cue, so R23 grants nothing to spend yet. `CT_Combat.csv` /
`DT_MatchRules.csv` NOT touched — no gameplay number appears in step 1 or 2 code.

*Seam closures, exactly as BP01's builder specified.* (1) `ABRCharacter::GetAbilitySystemComponent`
forwards to `ABRPlayerState`, caching nothing. (2) `InitAbilityActorInfo` is called from **three**
sites, not two: `ABRPlayerState::PostInitializeComponents` sets the OWNER with a null avatar (so
GameMode can grant a loadout before a body exists), then `ABRCharacter::PossessedBy` (server) and
`ABRCharacter::OnRep_PlayerState` (client) each re-point the AVATAR. (3) Controller stubs replaced
by a four-line relay; signature untouched (`FGameplayTag` by value). (4) Buffer is
`TArray<FGameplayTag>` + `AddUnique`, activation on the press edge only; TArray not TSet because
TSet iteration order depends on hashing and a pinned spec must not. (5) `LoggedHeldInputTags`
deleted — **note: it lived in `BRPlayerController`, not `BRCharacter` as the packet said**; both
were in owner_path so the disposition is unambiguous.

*Decided edge cases, recorded because "probably never" is an exploit schedule:* negative
IncomingDamage is REFUSED at Error level, never reinterpreted as healing · zero damage applies no
regen gate (a harmless source must not be able to suppress shields) · the meta attribute is
consumed BEFORE any branch that can return · `MaxHealth == 0` means uninitialised, not dead, and
`CheckForDeath` refuses to fire there · double death is guarded by a server-only latch that re-arms
when Health rises above zero, NOT by a `State.Dead` tag query (the tag is applied by whoever reacts
to `Event.Death`, so a tag query loses the race by construction) · an ability cancelled while its
key is still held does not auto-restart on the next `Triggered` (Halo sprint behaviour, chosen).

*Held-input flush moved from `OnUnPossess` to `SetPawn`* — `OnUnPossess` is authority-only, and the
buffer being flushed is the LOCAL client's. A remote client only ever sees `OnRep_Pawn -> SetPawn`.

*`BatchRPCTryActivateAbility` ships WITHOUT its `bEndAbilityImmediately` half, named not faked.*
UE 5.8 has no public external end: `UGameplayAbility::EndAbility` and `K2_EndAbility` are both
protected, and `CancelAbilityHandle` is a cancel, which is a different thing. **Step 3 closes it**
by adding `void ExternalEndAbility()` to `UBRGameplayAbility`.

*`FGameplayAbilitySpec::ActivationInfo` is `UE_DEPRECATED(5.5)`* and only ever applied to
non-instanced abilities. The prediction key for InputPressed/InputReleased is read from the live
instance instead (`GetCurrentActivationInfoRef()`), via a private `InvokeInputEventForSpec`.

**BUILD OBSERVATION (not a rung verdict).** `Tools\run-ubt.ps1 -Targets BreachpointEditor`,
started 2026-08-01T00:27:36.620, exit **6**, `Result: Failed (OtherCompilationError)` — and the
one error is **not in this packet's code**. Verbatim:

```
Source\Breachpoint\UI\BRUITypes.h(139): Error: Struct 'FBRKillfeedEntry' shares engine name
'BRKillfeedEntry' with struct 'FBRKillFeedEntry' in Source\Breachpoint\Match\BRGameState.h(78)
```

UHT aborts before this packet's headers are reached, so the repo build proves nothing either way
about BP02. To get an honest observation anyway, the whole tree was copied to a scratch directory
OUTSIDE the repo (no repo file touched, no owner_path crossed), the two foreign blockers were
patched THERE ONLY, and `BreachpointEditor` was built: **`Result: Succeeded`, 191.48 s,
`[961/989] Link [x64] UnrealEditor-Breachpoint.dll`.** UHT clean, module compiled and linked. The
two patches were (a) renaming BP10's `FBRKillfeedEntry`, (b) adding `"SlateCore"` to
`Breachpoint.Build.cs`. Both are filed below; neither was applied to the repo.

**contract_gaps (named, not fixed — all outside this packet's owner_path):**
- **BP04 × BP10 UHT name collision.** `FBRKillfeedEntry` (`UI/BRUITypes.h`) vs `FBRKillFeedEntry`
  (`Match/BRGameState.h`). UHT compares engine names case-insensitively. **Nothing in the module
  compiles until one is renamed.** Owner: whichever of BP04/BP10 the lead picks; two structs
  describing the same concept in two packets is itself the finding.
- **`Breachpoint.Build.cs` is missing `"SlateCore"`.** BP10's UI code instantiates `SObjectWidget`,
  and the module fails to LINK with 8 unresolved Slate symbols (`SWidget::SWidgetConstruct`,
  `EVisibility::Visible`, `SNullWidget::NullWidget`, `LLMTagDeclaration_UI_Slate`, …). `Build.cs`
  is in no packet's owner_path.
- **`State.Shields.Broken` has no applier.** The tag is declared in `BRGameplayTags` and §3.1, and
  step 2 does not apply it — purity law 5 forbids applying a State tag by hand, so it needs a GE.
  Step 4 must either add one or the tag is dead. Not decided here.
- **`GE_InitStats` modifier ORDER is load-bearing.** `PreAttributeChange` clamps Health to
  `GetMaxHealth()` as it currently is, so `GE_InitStats` must set MaxHealth/MaxShields BEFORE
  Health/Shields or a fighter spawns at zero health from a correct table. Called out in the clamp
  itself; step 4 owns it.
- **`UBRAbilitySystemComponent::RecentDamageEffectClass` is null until step 4.** Damage currently
  lands and logs a Warning that the regen gate was NOT applied. Loud on purpose.
- **`AbilitySystem/` has no log channel (R24).** Uses `LogBRCombat` and `LogBRInput`; `BRCore.h`
  is not in this packet's owner_path, so no `LogBRAbility` was added.
- **Another packet wrote `Source/Breachpoint/Data/BRDataRows.h`**, which is inside BP02's granted
  `owner_path`. Not touched here; flagged because it means two claims overlap on `Data/`.

---

**1 Aug 2026 — STEPS 3 AND 4 BUILT (sim-builder).** Ability base, AbilitySet, ExecCalc, BRGA_Sprint
+ the CMC half, and the generic-GE library. **No rung is pronounced**; the build results below are
observations. Two of three targets compiled — `BreachpointServer` was not built (it has never been
built on this machine and a from-scratch monolithic build would have held the global lock against
two live builders, R21).

*Files.* NEW: `AbilitySystem/BRCombatCurves.h/.cpp`, `AbilitySystem/BRDamageExecCalc.h/.cpp`,
`AbilitySystem/BRAbilitySet.h/.cpp`, `AbilitySystem/Abilities/BRGameplayAbility.h/.cpp`,
`AbilitySystem/Abilities/BRGA_Sprint.h/.cpp`, `AbilitySystem/Effects/BRGameplayEffects.h/.cpp`,
`Content/Data/CT_Combat.csv`. EDITED: `AbilitySystem/BRAbilitySystemComponent.h/.cpp`,
`AbilitySystem/BRAttributeSet.h/.cpp`, `Character/BRCharacterMovementComponent.h/.cpp`,
`Core/BRGameplayTags.h/.cpp` (one tag: `Ability.Sprint`, R23).

---

### RULING — saved moves: COMPRESSED FLAGS, and the premise for deferring was false

`cmc-prediction` asked BP02 to choose between compressed flags and the structured move-data path
"because switching later touches every movement state at once". **That premise does not hold in the
direction that matters**, and checking it is what made the decision cheap:
`FCharacterNetworkMoveData` — the structured path — **carries `CompressedMoveFlags` as one of its own
fields**. Adopting the container later is therefore ADDITIVE: every boolean already riding a custom
bit keeps riding it, untouched, and the container adds room for typed fields beside them. The painful
migration is the reverse (typed fields down into four bits), which nobody performs.

**Decision:** boolean movement INTENT rides the custom bits starting now. The structured container
arrives in its own packet the first time a movement state needs a field that is not a boolean —
BP06's grapple is the likely trigger (a target point or surface class does not fit in a bit) — and
that packet **does not have to touch sprint**.

**Bit budget, recorded because there are only four and running out silently is the failure mode:**

| bit | owner |
|---|---|
| `FLAG_Custom_0` | sprint (BP02, landed) |
| `FLAG_Custom_1` | RESERVED for grapple (BP06) — reserved, not used |
| `FLAG_Custom_2` | free |
| `FLAG_Custom_3` | free |

All four saved-move hooks are implemented (`Clear`, `SetMoveFor`, `PrepMoveFor`, `CanCombineWith`)
plus `GetCompressedFlags`/`UpdateFromCompressedFlags` symmetrically. `cmc-prediction` should be
corrected with this ruling; its section 3 code sketch is otherwise accurate for 5.8 (compressed flags
are NOT deprecated in 5.8 — checked against the engine).

---

### THE `GE_RecentDamage` PATTERN (R18 proved, with one crack — see contract_gaps)

Landed first, as the boxed warning required, then applied mechanically to the other six. Four moves:

1. **`CreateDefaultSubobject<TComponent>(TEXT("UniqueName"))` in the constructor — NOT the engine's
   `AddComponent<T>()`.** That helper calls `NewObject(this, NAME_None)`, and `NewObject` with the
   currently-constructing object as Outer is a **FATAL check** (`FObjectInitializer::
   AssertIfInConstructor`, `UObjectGlobals.cpp:4880`). `AddComponent` is for runtime-built effects.
2. **`GEComponents.Add(Component)`** — the array is `protected`, so a subclass may write it.
3. **Configure through the component's own public API.** For tags that means
   `SetAndApplyTargetTagChanges(FInheritedTagContainer)`, which is what writes the effect's
   `CachedGrantedTags`. **Setting `InheritableGrantedTagsContainer` and skipping that call yields an
   effect that debug-prints correctly and grants nothing** — the one silent failure in the pattern.
4. **Set the plain properties directly** (`DurationPolicy`, `DurationMagnitude`, `Modifiers`,
   `Executions`, stacking).

**Epic supports this deliberately.** `UGameplayEffect::PostInitProperties` has a native-class branch
("easy-to-overlook issues when implementing a Gameplay Effect as a native class") that scoops up any
default subobject you forgot to register (behind an `ensureMsgf`) and runs `IsDataValid` immediately,
so a misconfigured native GE reports itself at editor startup rather than at first application.

**Native tags in a CDO constructor are safe** — checked, not assumed: `FNativeGameplayTag`'s
constructor sets `InternalTag` immediately during module static init, which precedes CDO creation.

**No gameplay number appears in any of the seven effects.** Every magnitude is SetByCaller (filled by
the applier from `CT_Combat`) or an execution. `GE_Regen`'s tick period is written onto the SPEC
(`FGameplayEffectSpec::Period` is public and `GetPeriod()` returns it for anything non-Instant), so
period and rate are separate CSV cells and changing the period does not move the rate per second.

**`GE_InitStats` modifier order is fixed and is the class's whole design:** MaxHealth, MaxShields,
Health, Shields — capacities before current values, `Override` not `Additive` (respawn re-applies).
Modifiers execute in array order. The trap steps 1-2 flagged is closed.

**`State.Shields.Broken` landed as a SEVENTH effect, `GE_ShieldsBroken`** (infinite, one granted
tag), applied/removed by `UBRAbilitySystemComponent::SetShieldsBrokenState` from the attribute set's
one reaction point, on both the damage path and the regen path, as an idempotent state ASSERTION
rather than a transition event. Refuses while `MaxShields == 0` for the same reason `CheckForDeath`
refuses: that is "uninitialised", not "broken". Named loudly because it is outside the ticket's list
of six.

---

### Decided edge cases (recorded because "probably never" is an exploit schedule)

- **A `Damage.*` tag with no `<Tag>.Multiplier` curve contributes the identity 1.0**, logged once per
  curve name per process. Refusing the hit instead would turn a missing CSV row into an invulnerable
  player — a worse failure and a harder one to diagnose. `CT_Combat` ships a row for all five tags,
  so the identity path is unreachable in practice.
- **The exec calc does NOT split shields/health.** The packet describes the pipeline as "shields
  absorb -> overflow"; that half belongs to `UBRAttributeSet` (which already owns it, loudly), and
  duplicating it in the execution would give the shield seam two masters. The execution computes ONE
  number and writes `IncomingDamage`.
- **The curve name is DERIVED from the tag** (`Damage.Headshot` -> `Damage.Headshot.Multiplier`), so a
  new damage modifier is a tag plus a CSV row and **zero lines of code**. No switch, no tag-to-curve map.
- **A zero-length cooldown is REFUSED, loudly**, not applied: it succeeds, blocks nothing, and looks
  applied. Same for a zero-length RecentDamage gate (no gate + one Error beats a gate that lies).
- **`GE_Regen` is blocked by `State.Dead` as well as `State.Combat.RecentDamage`** — without the
  second tag a corpse waiting out a respawn timer silently recharges shields it never earned.
- **`ApplyDeathEffect` cancels running abilities**, because `ActivationBlockedTags` blocks activation
  and says nothing about an ability already active — a sprint started before death would otherwise
  hold the CMC at sprint speed on a corpse.
- **`ApplyInitStats` refuses rather than inventing a starting health** when `CT_Combat` is missing:
  an uninitialised fighter is a bug report, an invented health pool is a balance change nobody made.
- **WhileInputHeld uses `WaitInputRelease`, not an `InputReleased()` override.** On the SERVER an
  ability's `InputReleased()` only fires when `bReplicateInputDirectly` is set, which costs an RPC per
  press; the task listens to the replicated generic event the ASC already sends. `bTestAlreadyReleased
  = true` closes the tap case (a key released before a predicted activation completes would otherwise
  sprint forever).

---

### contract_gaps (named, not fixed)

- **`Source/Breachpoint/Tests/` is not in this claim's `owner_path`, so THESE RULES LANDED WITH ZERO
  PINNED SPECS.** This is the sharpest gap in the packet and it is against my own doctrine: a
  TTK-bearing formula with no golden suite. Mitigated structurally, not waved away —
  `UBRDamageExecCalc::ComputeFinalDamage(BaseDamage, Tags, CurveLookup, OutMissing)` is a **pure
  static function with no world, no ASC and no assets**, and `BRCombatCurves::SetTableOverrideForTests`
  lets a spec state its own coefficients. Both exist so the suite is about thirty minutes' work the
  moment `Tests/BRCombatSpec.cpp` is granted (R25 shape). Step 5 must not be closed without it.
- **R18 CRACK: `UGameplayEffect::StackingType` is `UE_DEPRECATED(5.7)` and its setter
  `SetStackingType` is `WITH_EDITOR` only.** There is therefore NO non-deprecated way for a native GE
  class to declare stacking in a packaged build — the API assumes effects are editor-authored assets.
  `GE_RecentDamage` writes the property inside a three-line
  `PRAGMA_DISABLE_DEPRECATION_WARNINGS` scope so that when Epic privatises it this fails to compile
  THERE, loudly, instead of silently losing the gate's stacking. Someone should decide before 5.9.
- **Section 3.3 IS WRONG ABOUT HOW FIRING CANCELS SPRINT.** It says `BRGA_WeaponFire` lists
  `State.Movement.Sprinting` in `CancelAbilitiesWithTag`. That field is matched against the target
  ability's **asset tags**, and `State.Movement.Sprinting` is granted to the ACTOR, not owned by the
  ability — so as written, firing would not cancel sprint and nothing would error. BP03/BP05/BP09 must
  list **`Ability.Sprint`** (declared this packet under R23). Not patched by giving the ability a
  `State.*` asset tag, which would have hidden the error.
- **TWO SOURCES OF TRUTH FOR THE HEADSHOT MULTIPLIER.** `FBRWeaponRow::HeadshotMult` is per-weapon
  (AR 1.0, Magnum 2.0 — rulings R1/R2/R4) *and* the exec calc composes a global
  `Damage.Headshot.Multiplier` curve. Shipping both live would multiply them. **`CT_Combat` therefore
  ships `Damage.Headshot.Multiplier = 1.0`** (identity — the modifier axis exists and is switched
  off), and BP03's fire path is expected to fold the weapon's `HeadshotMult` into
  `SetByCaller.BaseDamage` **on the server, after it validates the hit bone**. Raising the global curve
  would silently buff every weapon including the two whose 1.0 is a design position. Needs a ruling.
- **`SetByCaller.*` is a CLOSED tag family (R23), so `GE_InitStats` uses FName-keyed SetByCaller**
  (`MaxHealth`/`MaxShields`/`Health`/`Shields`) — first-class engine API, no invented tags. If the
  crew prefers tag-keyed magnitudes, section 3.1 gains four leaves and the class changes two lines.
- **The sprint bit is trusted.** `UpdateFromCompressedFlags` accepts the client's sprint bit and
  `GetMaxSpeed` acts on it without checking that the client has an active sprint ability, so a
  modified client can assert the bit and move faster without activating `BRGA_Sprint`. This is the
  standard UE arrangement (`bWantsToCrouch` has the same property) and it is bounded, but it is real.
  **The obvious closure was tried on paper and rejected, not overlooked:** requiring
  `State.Movement.Sprinting` before applying the multiplier would make the server routinely process a
  sprint move BEFORE the activation RPC that authorises it (different actor channels, no ordering
  guarantee) and correct the client at the start of every honest sprint. The gate belongs in
  `UBRCharacterMovementComponent::IsSprintIntentValid()`, which exists so the closure is one function.
  **netcode-builder rules on this.**
- **`Movement.Sprint.SpeedMultiplier = 1.2` is the GDD's "+20% move speed"**; every other CT_Combat
  value traces to a ruling or the ticket (100/100 EHP, 60/s, 2.5 s). `Shields.Regen.PeriodSeconds =
  0.25` is the one value with **no source** — it is tick granularity, not balance (rate per second is
  independent of it), and it is tuning-curator's to confirm.
- **`ActivationOwnedTags` replicate `CountToOwner`**, so a SIMULATED PROXY does not see another
  player's `State.Movement.Sprinting`. Fine today (observers read sprint from velocity), but any UI or
  cue that wants to show "that enemy is sprinting" needs a different channel. Named for BP10/BP11.
- **`AbilitySystem/` still has no `LogBRAbility` channel (R24)** — `BRCore.h` was in this claim's
  `owner_path` this time, but adding a channel mid-wave to a file three packets include was not worth
  the rebuild; everything logs on `LogBRCombat`.

---

**BUILD OBSERVATIONS (not a rung verdict).** `Tools\run-ubt.ps1`, build lock verified free first
(no `cl.exe`/`link.exe`/`UnrealBuildTool`/`UnrealHeaderTool` — only idle MSBuild node-reuse workers):

- `BreachpointEditor` — start 2026-08-01T01:23:22.441, exit **0**, `Result: Succeeded`, 6.43 s,
  artifact newer than start (R19 PASS). **Zero warnings** after the deprecation scope above; the
  first run of the same target (01:22:12, 12 actions, `Link UnrealEditor-Breachpoint.dll`) compiled
  all ten changed .cpp files and ran UHT over all six new headers.
- `Breachpoint` (Game, monolithic) — exit **0**, `Result: Succeeded`, 93.22 s,
  `Link [x64] Breachpoint.exe`. Run deliberately because it is the target that catches
  `WITH_EDITOR`-only mistakes the editor build cannot see (`UBRAbilitySet::IsDataValid`).
- `BreachpointServer` — **NOT BUILT.** Never built on this machine; a from-scratch monolithic build
  would have held the global lock against two live builders (R21). Rung 1 is therefore **not** claimed
  and cannot be claimed from this Log.

Nothing here has been run: **compiles is not works.** No PIE, no multi-process, no correction test,
and `CT_Combat.csv` has not been imported to a CurveTable asset yet (`Tools/reimport-tables.ps1
-Tables CT_Combat` is now unblocked — its precondition was this CSV existing). BP02's Done-when boxes
for sprint prediction and for the damage/regen/death numbers all still require step 5's specs and a
multi-process run.

**1 Aug 2026 — step 5: the project's FIRST two spec files landed. Rung 0.**

`Source/Breachpoint/Tests/` held only a `.gitkeep` until now, so every combat and shield rule
this project has landed was unpinned. Two sim-builder agents wrote one file each, taken by
**exact path per R25** (never the `Tests/` folder), under an R31 window claim:

- `BRCombatSpec.cpp` — 1036 lines, suite `Breachpoint.Sim.Combat`. Row algebra, headshot
  multipliers, `UBRDamageExecCalc`, TTK, attribute clamps.
- `BRShieldSpec.cpp` — 999 lines, suite `Breachpoint.Sim.Shields`, 29 tests in 5 `Describe`s.
  Recharge gate, rate/clamp, shields-first ordering, the `ShieldsBroken` transition, death.

**RUNG 0 — written, not compiled, not run.** An editor held the project lock the whole time
(R29.3), so no build was attempted and none of this is a correctness claim. First compile will
very likely need a debug pass; that is expected of a rung-0 artifact.

**Verified by the dispatching session (not taken on the agents' word):** both files pure ASCII
(40 em-dashes normalised out of `BRShieldSpec.cpp` — MSVC C4819 under warnings-as-errors is a
real build failure, and it is the same family as BP14's cp1252 defect), braces and parens
balanced, neither wrote outside its granted path, and `BRShieldSpec.cpp` writes **no attribute
directly** — the only mention of `SetShields`/`SetHealth`/`InitShields` is a comment documenting
that they are never called (law 1 intact). Every TTK number in `BRCombatSpec` was re-derived
against the shipped `DT_Weapons.csv`: AR solo 25 shots/2.400 s, Magnum all-head 5/1.3333 s, the
13th AR shot overflowing exactly 4 into health, Rocket 120 not one-shotting 200 EHP while 240
would. **No CSV number contradicts a ruling.**

**TWO CONTRACT_GAPS — filed, not worked around:**

1. **`UBRAttributeSet::ApplyIncomingDamageShieldsFirst` has no headless entry point.** It is
   private and only reachable from `PostGameplayEffectExecute`, which needs a live
   `FGameplayEffectModCallbackData` + ASC. The agent refused to re-implement "absorb then
   overflow" in the spec and assert it against itself — that is precisely the hollow spec R25
   forbids — and pinned the split's *observable consequence* instead. **Asks for the same
   factoring `ComputeFinalDamage` already has:** a `static void SplitShieldsFirst(float Raw,
   float ShieldsBefore, float& OutAbsorbed, float& OutOverflow)` on `UBRAttributeSet`, after
   which it is four assertions. Correctly NOT added — `AbilitySystem/` is outside the claim.
2. **There is no `ShieldsBroken` EVENT, only a GE-applied tag.** R12 makes break-off on
   shield-crack a legibility rule bots read. If R12 wants `Event.Shields.Broken`, that is a
   `contract_gap` under R23 (`Event.*` is a CLOSED family). The spec pins tag count across
   repeated hits instead — the observable a bot actually reads.

**A DEFECT IN THE DISPATCH, recorded because the agent was right and the packet was wrong:**
the packet prompt said "health reaching zero applies `GE_Death`." **It does not, deliberately.**
`BRAttributeSet.cpp:269` — *"The attribute set REPORTS death; it does not administer it."* The
agent pinned the code's real contract and added an alarm (asserting `State.Dead` is ABSENT
immediately after death is reported), so if anyone ever moves `GE_Death` into the attribute set
that line goes red. It reported the contradiction rather than quietly following the prompt.

**CORROBORATION OF A LIVE FINDING, by an independent method.** The shield agent found
`Content/Data/CT_Combat.uasset` does not exist (filesystem scan), so today
`GetCombatCurveTable()` returns null, `ApplyInitStats()` refuses (fighters spawn `MaxHealth 0`)
and `ApplyRecentDamageGate()` refuses (**regen ungated**). This independently confirms what a
read-only MCP query found the same afternoon — `/Game/Data` holds **zero assets**. Two methods,
same answer: **R5's pillars are currently unreachable at runtime.** Both suites are deliberately
immune, building their tables in memory from the committed CSVs via
`BRCombatCurves::SetTableOverrideForTests`, so they do not wait on the reimport.

**Not pinned, and named rather than smuggled:** the shields-first split itself (gap 1); elapsed-
time proof that recharge resumes at 2.5 s (rung 3/4 — the suite pins the number and the
mechanism, and simulates gate expiry by removing the effect); recharge RATE as a number
(deliberately left as the composition rule `per-tick == rate x period`, because rate/period/
capacities are TUNABLES and pinning them would turn a legitimate balance change red — the
literal 2.5 is the one hard-coded number, and only because R5 makes it a pillar); death
attribution (BP04's); the full GE path end-to-end (needs an ASC).
