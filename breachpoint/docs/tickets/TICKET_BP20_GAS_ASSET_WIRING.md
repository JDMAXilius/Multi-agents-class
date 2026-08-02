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
