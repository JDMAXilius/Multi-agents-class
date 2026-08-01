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
