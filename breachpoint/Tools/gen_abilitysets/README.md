# Tools/gen_abilitysets — the ability sets, generated

Serves **TICKET BP20**: the four `DA_AbilitySet_*` assets were empty (or, for Core,
sprint-only), so every verb except jump was dead. Jump works because
`ABRPlayerState::GiveNativeAbility` grants it from C++; everything else waits on asset data.

Asset data that anyone can re-derive is a script. That is CLAUDE.md law 7: **the committed,
reviewable artifact is this generator and its JSON receipt — never the `.uasset` alone.**

| step | what | who writes it |
|---|---|---|
| B1 | `DA_AbilitySet_Core` grants sprint, melee, grenade, grapple | this generator |
| B2 | `DA_AbilitySet_AR` / `_Magnum` / `_Rocket` each grant fire, reload, swap | this generator |
| B3 | `GM_BR`'s four class overrides cleared so the C++ defaults win | this generator |
| B4 | `PC_BR`'s `Input\|Abilities` category is untouched by hand | **audited, never written** |
| B5 | `BP_BRcharacter` kept Move/Look/MouseLook and no dangling `JumpAction` | **audited, never written** |

B4 and B5 are reads on purpose. Writing to `PC_BR` would *be* the bug B4 warns about: a value
serialised on the Blueprint beats `Config/DefaultGame.ini` permanently and silently, because
`LoadConfig` runs before the Blueprint's own serialisation.

## The split

Same shape as `Tools/gen_input` and `Tools/blockout`. Every **decision** lives in plain
CPython you can run with no engine installed and no editor free; the editor half is a dumb
executor plus the asserts only a live asset can answer.

```
abilityset_profile.json   the SOURCE OF TRUTH. Content/Abilities is its projection.
abilityset_plan.py        pure CPython. Validates the profile and cross-reads the repo.
build_abilitysets.py      imports `unreal`. Writes .uasset bytes. Refuses loudly.
build-abilitysets.ps1     engine resolution, plugin check, R21 one-editor guard, logging.
selftest_no_editor.py     drives build_abilitysets.py against a FAKE editor.
```

## Running it

```powershell
# no editor needed — use these while someone else holds the project
Tools\gen_abilitysets\build-abilitysets.ps1 -PlanOnly
Tools\gen_abilitysets\build-abilitysets.ps1 -SelfTest

# the real thing (needs the editor CLOSED — R21)
git lfs lock Content/Abilities/DA_AbilitySet_Core.uasset      # law 7, one owner per binary
git lfs lock Content/Abilities/DA_AbilitySet_AR.uasset
git lfs lock Content/Abilities/DA_AbilitySet_Magnum.uasset
git lfs lock Content/Abilities/DA_AbilitySet_Rocket.uasset
git lfs lock Content/Core/GM_BR.uasset
Tools\gen_abilitysets\build-abilitysets.ps1 -DryRun           # audit first, write nothing
Tools\gen_abilitysets\build-abilitysets.ps1
```

Exit codes follow the ladder: `0` PASS · `1` FAIL · `2` INCONCLUSIVE · `3` BLOCKED.
The receipt lands in `Saved/AbilitySetGen/abilitysets-<stamp>-<verdict>.json` on **every**
path, including refusals. Paste the relevant part into the ticket Log.

## What `-PlanOnly` actually checks

It is not a schema lint. It cross-reads the repo, so a typo is caught with no editor instead
of as a silently-skipped verb in a match:

- every `input_tag` is declared in `Source/Breachpoint/Core/BRGameplayTags.cpp`;
- every ability class is declared in `Source/Breachpoint/AbilitySystem/Abilities/*.h`
  (and is spelled as a native `UClass` path — `/Script/Breachpoint.BRGA_Melee`, **no** `U`);
- every `AbilitySet` reference in `Content/Data/DT_Weapons.csv` is a set this profile builds;
- `Config/DefaultGame.ini`'s `StartupAbilitySet` names the set this profile calls Core;
- no row carries `InputTag.Jump` — that would be a *second* spec on `UBRGA_Jump`;
- no tag appears both in Core and in a weapon set (the startup set is never taken away, so
  once a weapon is equipped two specs would answer one press);
- everything `UBRAbilitySet::IsDataValid` rejects: a row with no class, a level below 1, a
  duplicate tag within a set.

## The three states a set can be in

`build_abilitysets.py` classifies what it finds on disk before it writes, because "nobody ever
filled this in" and "somebody hand-edited a generated asset" are different facts:

- **EMPTY_ON_DISK** — no rows. BP20's measured starting state for the weapon sets. Info.
- **INCOMPLETE_ON_DISK** — every row present is one the profile also wants, and the profile
  wants more. That is `DA_AbilitySet_Core` on 2 Aug 2026: sprint was there, the other three
  were the delta. Info.
- **DRIFT_SET_ROWS** — a row *contradicts* the profile. The run repairs it **and fails**, so
  the edit is seen. Re-run for a clean pass, or `-AllowDrift` and justify it in the Log.

## Honesty

Exit `0` means the sets were written and read back as the profile describes them, and
`UBRAbilitySet::IsDataValid` accepted every one. That is the **generated** rung.

It does **not** mean a key does anything. BP20's Done-when still owes a PIE run logging
`GRANTED:` for every ability with the right tag and `GA ACTIVATED:` on melee, grenade and
grapple with no ensure firing — pasted verbatim into the ticket Log.

And fire/reload/swap cannot be exercised at all yet: `UBREquipmentComponent::GiveWeapon` is
server-only and has no caller anywhere in the module, so no weapon is ever equipped. That is
BP20's "Blocked" section and belongs to the C++/weapons lane. B2 is still worth doing first so
the sets are correct when that lands.

`build_abilitysets.py` was authored with **no editor available** — BP20 was cut from a PIE run
and the MCP bridge was down. Every Python binding name it depends on is reached through a
defensive helper that tries the documented spelling, falls back through the plausible
alternatives, and fails loudly with what it actually saw. `selftest_no_editor.py` proves the
control flow against a fake editor; it proves nothing about the real bindings. The first real
run is the one that confirms them, and if a binding differs the generator stops instead of
half-writing an asset — report the read-back it prints and the fix is one line.
