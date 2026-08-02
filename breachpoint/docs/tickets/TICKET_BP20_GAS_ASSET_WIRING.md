# TICKET — BP20: The ability-set assets are empty, and that is why the verbs are dead

> STATUS: open — cut 2 Aug 2026 from a live PIE run. **Editor/MCP lane only.** Every step here is
> an asset edit; the C++ side landed and is green on all three targets (rung 1). This ticket
> exists because the code now proves, at runtime, exactly which assets are unwired — the
> diagnostics name them by tag. Do not re-derive the list; it is below, measured, not guessed.

**Relationship to BP18/BP19:** BP18 is the standing asset batch and BP19 phase B covers the
character. This ticket is neither's duplicate — it is the **delta created on 1–2 Aug 2026**, when
ability input moved to `ABRPlayerController`, jump became a GA, the GAS stage gate was deleted and
`ABRPlayerState::GiveStartupLoadout` began granting a startup set. Those changes made a set of
asset gaps *load-bearing* that were previously invisible because nothing GAS-side ran at all.

**Law 7 still stands and this ticket does not repeal it.** An MCP driving the editor is still
hand-placing. The committed, reviewable artifact is a script or a receipt — never the `.uasset`
alone. Operate under BP16 step 2 reading (a), MCP-as-executor, as BP18 does.

## Kickoff (machine-checkable)

- requires: **editor-live** — every step needs the editor open and the MCP reachable
- requires: **clean tree** — `git pull --rebase` first; the C++ this ticket depends on is on `main`
- owner_path: `Content/Abilities/`, `Content/Core/`, `Content/Characters/`

## The evidence this ticket is built on (measured 2 Aug 2026, do not re-derive)

Read directly from the `.uasset` string tables and from a PIE run's log:

| Asset | Contains | Consequence |
|---|---|---|
| `DA_AbilitySet_Core` | `BRGA_Sprint` + `InputTag.Sprint`, **nothing else** | melee, grenade and grapple are never granted |
| `DA_AbilitySet_AR` | **empty** | fire/reload/swap never granted, even holding an AR |
| `DA_AbilitySet_Magnum` | **empty** | as above |
| `DA_AbilitySet_Rocket` | **empty** | as above |
| `IMC_Default` | maps all 11 `IA_*` including `IA_Sprint` | **input mapping is NOT the problem** — verified, so do not "fix" it |

From PIE, verbatim:

```
LogBRAbility: GA ACTIVATED: BRGA_Jump on 'BP_BRcharacter_C_0' [Authority]
Ensure condition failed: ... input tag 'InputTag.Reload' reached the ASC but no granted
ability spec carries it.
Ensure condition failed: ... input tag 'InputTag.Swap' reached the ASC but no granted
ability spec carries it.
```

Jump works because it is granted from C++, not from an asset. Everything else is asset-blocked.

## B1. Populate `DA_AbilitySet_Core` — the three non-weapon verbs

Add three entries. **Do not add jump** — `ABRPlayerState::GiveNativeAbility` grants it in C++ and a
second grant would give it two specs.

| Ability | InputTag | Level |
|---|---|---|
| `BRGA_Melee` | `InputTag.Melee` | 1 |
| `BRGA_Grenade` | `InputTag.Grenade` | 1 |
| `BRGA_Grapple` | `InputTag.Grapple` | 1 |

**The `InputTag` field is not optional and is the whole mechanism.** `UBRAbilitySet::GiveToAbilitySystem`
stamps it onto the spec's dynamic source tags, and `UBRAbilitySystemComponent::AbilityInputTagPressed`
matches against exactly that. An entry with an ability and no `InputTag` is granted and permanently
unreachable by any key — the C++ now `ensureAlways`es on that case at grant time, so it will be loud.

Commits: the generation/receipt artifact, not the `.uasset` alone.

## B2. Populate the three weapon sets

Each of `DA_AbilitySet_AR`, `DA_AbilitySet_Magnum`, `DA_AbilitySet_Rocket`:

| Ability | InputTag |
|---|---|
| `BRGA_WeaponFire` | `InputTag.Fire` |
| `UBRGA_Reload` (in `BRGA_WeaponUtility.h`) | `InputTag.Reload` |
| `UBRGA_WeaponSwap` (in `BRGA_WeaponUtility.h`) | `InputTag.Swap` |

These are granted by `UBREquipmentComponent` when a weapon is equipped, not by the startup loadout.

## B3. `GM_BR` — clear the Blueprint class overrides

`ABRGameMode`'s constructor now sets all four class defaults in C++. `GM_BR`'s saved values still
win (`PHASE2-RELAYER.md` step 1). Verified 1 Aug by reading the import table: its
`PlayerControllerClass` held **`BP_ShooterPlayerController`**, the template's controller — so
`ABRPlayerController`'s input never ran.

Clear the override on `PlayerControllerClass`, `DefaultPawnClass`, `PlayerStateClass` and
`GameStateClass` so the C++ defaults apply. **Clearing is the fix, not repointing** — that is why
the C++ defaults were added.

## B4. `PC_BR` — confirm nothing shadows the new ini pins

`Config/DefaultGame.ini` now pins eight ability actions and the startup set. Those properties are
new, so nothing can have serialised them yet — but if anyone opens `PC_BR` and touches a value in
`Input|Abilities`, the Blueprint's value wins over the ini permanently and silently.

Confirm the `Input|Abilities` category is untouched. Same check for
`[/Script/Breachpoint.BRPlayerState] StartupAbilitySet`.

## B5. `BP_BRcharacter` — the pawn kept only three actions

The pawn now owns Move, Look and MouseLook only. Jump moved to the controller. Confirm those three
are still assigned and that the now-removed `JumpAction` slot has not left a dangling override.

## Blocked, and not this ticket's to fix (C++ lane)

**Nothing equips a starting weapon.** `UBREquipmentComponent::GiveWeapon` is server-only and has no
caller anywhere in the module. Even with B2 done, fire/reload/swap cannot be exercised until
something grants a weapon on spawn. File this against the weapons lane; B2 is still worth doing
first so the sets are correct when that lands.

## Done when

- [ ] `DA_AbilitySet_Core` grants melee, grenade and grapple, each with its `InputTag`
- [ ] all three weapon sets grant fire, reload and swap, each with its `InputTag`
- [ ] `GM_BR`'s four class overrides are cleared
- [ ] a PIE run logs `GRANTED:` for every ability above, at startup, with the right tag
- [ ] pressing melee, grenade and grapple each logs `GA ACTIVATED:` and no ensure fires
- [ ] the run's verbatim log is pasted into the Log below — a claim without it is not a result

## Log

<!-- Append findings here. Numbers and calls go in the Log or they did not happen. -->

### 2 Aug 2026 — the landing mechanism exists; the editor half has NOT run

**Status: B1/B2/B3 authored and proven at rung 1 (static + fake-editor). Nothing has been
written to `Content/`. B4/B5 unconfirmed. The PIE proof is not started.**

`Tools/gen_abilitysets/` — profile, plan, executor, wrapper, self-test, README. Law 7's
committed artifact for this ticket, in the same split as `Tools/gen_input`: every decision in
plain CPython, the editor half a dumb executor plus the asserts only a live asset can answer.
It does B1, B2 and B3, and **audits** B4/B5 without writing — writing to `PC_BR` would *be*
the failure B4 warns about.

Chose the headless `-run=pythonscript` route over MCP-as-executor. Both are BP16 step 2
reading (a); this one leaves a re-runnable artifact rather than a transcript, and it does not
need the bridge — which is down (`127.0.0.1:8000` refused connection, no `unreal-mcp` tools in
session).

Rung 1, run and passing:

```
build-abilitysets.ps1 -PlanOnly    exit 0    4 sets, 13 rows, 0 error(s), 0 warning(s)
                                             plan digest 8e4ca36aa137fe4f
build-abilitysets.ps1 -SelfTest    exit 0    8/8 cases
```

`-PlanOnly` is not a schema lint — it cross-reads the repo, and every cross-read resolved
(0 warnings means none of them failed to read):

- all 7 `InputTag.*` declared in `BRGameplayTags.cpp`;
- all 6 ability classes declared in `AbilitySystem/Abilities/*.h`, spelled as native `UClass`
  paths (`/Script/Breachpoint.BRGA_Melee` — **no** `U`, the prefix is dropped);
- all 3 `AbilitySet` refs in `DT_Weapons.csv` are sets this profile builds;
- `DefaultGame.ini`'s `StartupAbilitySet` names the Core set;
- no row carries `InputTag.Jump`, asserted statically *and* at write time *and* in
  post-write verification. It is a contract constant, not profile data.

**BLOCKED at the editor rung, correctly.** R21 fired: `UnrealEditor.exe (pid 3096)` holds the
project, and this generator rewrites binaries in `Content/Abilities` and `Content/Core`. Exit
3, nothing launched. To land B1/B2/B3, close the editor and run:

```
git lfs lock Content/Abilities/DA_AbilitySet_{Core,AR,Magnum,Rocket}.uasset
git lfs lock Content/Core/GM_BR.uasset
Tools\gen_abilitysets\build-abilitysets.ps1 -DryRun
Tools\gen_abilitysets\build-abilitysets.ps1
```

The receipt lands in `Saved/AbilitySetGen/` on every path including refusals. Paste it here.

**Two calls worth arguing with:**

1. *Clearing* GM_BR's overrides is done by setting the BP CDO's property to `ABRGameMode`'s
   CDO value. A Blueprint CDO is delta-serialised against its parent, so a property equal to
   the parent's is not written to the `.uasset` at all — same bytes as the reset arrow. The
   run refuses if `GM_BR` does not actually derive from `ABRGameMode`, because then clearing
   would *blank* the classes rather than hand control to the C++ constructor.
2. An empty set, an incomplete set and a drifted set are three different findings, not one.
   `DA_AbilitySet_Core` holding sprint-only is `INCOMPLETE_ON_DISK` (info) — this ticket's
   delta. Only a row that *contradicts* the profile is `DRIFT_SET_ROWS`, and only that fails
   the run. Conflating them would make the first BP20 run fail on the asset it was written to
   fix, and train the next reader to reach for `-AllowDrift` by reflex.

**Unproven, and the generator says so on exit 0:** not one Python binding name is verified.
`FBRAbilitySetEntry.import_text`, the CDO path, `SoftClassPath` conversion — all authored
against the 5.8 headers with no editor, each behind a helper that fails loudly rather than
half-writing. The self-test uses a fake `unreal` and proves control flow only. The first real
run is what confirms the bindings; if one differs it stops and prints the read-back, and the
fix is one line.

**contract_gap (minor, shared code, worked around locally):**
`Tools/_BRLadderCommon.ps1:296` `Get-BRLiveEditorProcesses` returns `@()`, which PowerShell
unrolls to `$null` on return — so `$liveEditors.Count` is a hard error under
`Set-StrictMode 2.0` in the **normal** case (no editor running). `build-input.ps1:181` and
`build-abilitysets.ps1` both had the line; ours now wraps the call in `@()`. Owner of the
ladder should fix the helper. Not edited here — law 5.

**Still owed after a green run** (unchanged from Done-when): a PIE run logging `GRANTED:` for
every ability with the right tag, and `GA ACTIVATED:` on melee, grenade and grapple with no
ensure. fire/reload/swap remain unexercisable regardless — `UBREquipmentComponent::GiveWeapon`
still has no caller. PIE is not multiplayer.
