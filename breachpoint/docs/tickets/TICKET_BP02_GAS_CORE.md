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
