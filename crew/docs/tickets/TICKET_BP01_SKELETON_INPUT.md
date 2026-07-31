# TICKET — BP01: Project skeleton, Core, and the owned input layer

> STATUS: open — cut by lead session, 29 Jul 2026. First pickup of the project; nothing gates
> it except an installed UE 5.8 + the FPS template.

Founder directive: create the project exactly per `ARCHITECTURE.md` (v2) — one runtime module,
folder-per-discipline, three targets from day one, BR prefix. The input layer is OURS: Enhanced
Input → InputTag → ASC, no per-ability binding code, ever.

**Ordering law:** Step 1 gates everything in every ticket. Steps 2–4 in order.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- none (root ticket — nothing gates it), but the environment must be real:
  a **source-built UE 5.8** is installed and its path is known, and the game repo has
  `crew/`'s contents at its root (`CLAUDE.md`, `.claude/`, `docs/` all present)
- owner_path: `Source/Breachpoint/`, `Config/`

## Steps (in order)

1. Create `Breachpoint` from the UE 5.8 **First Person C++ template**. Strip template gameplay
   to pawn/camera/input scaffolding. Add the three targets (`Breachpoint`, `BreachpointEditor`,
   `BreachpointServer` — server target must compile NOW), `Breachpoint.Build.cs` deps per
   ARCHITECTURE §3, folder skeleton (Core/Input/AbilitySystem/Character/Weapons/Match/AI/
   Online/UI/Telemetry/Data/Tests). Owner: **builder**.
2. `Core/`: `BRGameplayTags.h/.cpp` (ALL native tags from ARCHITECTURE §3.1 — the one
   authoritative list; includes `InputTag.*` and `SetByCaller.*`) and `BRCore.h/.cpp` (log
   channels + collision aliases; add matching channels to `DefaultEngine.ini`).
   Owner: **builder**.
   > **Includes the montage→gameplay notify seam (ruling R17) — these are needed by BP03 and
   > BP05, and `Core/` closes with this ticket, so they land HERE or those packets stop:**
   > `Event.Melee.WindowBegin` · `Event.Melee.WindowEnd` · `Event.Weapon.ReloadCommit` ·
   > `Event.Weapon.SwapCommit`. They are declared and nothing consumes them yet — that is
   > correct; an unconsumed native tag is free, and a missing one is a `contract_gap` after
   > this folder is closed. Extension rule for later seams: `Event.<Verb>.<Moment>`.
3. `Input/`: `BRInputConfig` (UDataAsset; **soft** `TSoftObjectPtr<UInputAction>` refs +
   InputTag pairs, native and ability lists) and `BRInputComponent` (`BindNativeAction`,
   `BindAbilityActions` templates → two handlers carrying the tag). Content: `IMC_Default`,
   `IA_Move/Look/Jump/Crouch/Fire/Reload/Swap/Grenade/Melee/Grapple`, `DA_InputConfig`.
   Owner: **builder**. Contract: `data-and-assets.md` (soft refs law).
4. `Character/` shell: `BRCharacter` (template pawn rebased; `IAbilitySystemInterface`
   forwarding stub — PlayerState ASC arrives in BP02; wires `BRInputComponent`; native actions
   move/look/jump work) + `BRCharacterMovementComponent` (empty subclass, registered).
   `Match/` shell: `BRPlayerController` with `AbilityInputTagPressed/Released` forwarding
   stubs. Owner: **builder**.
5. Verifier: rung 1 on all three targets; PIE smoke — walk around the template map with the
   new input path (native actions only). Owner: **verifier**.

## Done when

- [ ] All three targets compile clean from scratch
- [ ] Folder skeleton matches ARCHITECTURE §3 exactly (crew owner_paths depend on it)
- [ ] `BRGameplayTags` declares every tag in ARCHITECTURE §3.1 **including the four R17 notify
      tags** (`Event.Melee.WindowBegin/WindowEnd`, `Event.Weapon.ReloadCommit/SwapCommit`) —
      grep the header against §3.1 and paste the diff (expected: empty) into the Log. Missing
      one is not caught by the compiler; it is caught by BP03 or BP05 stopping.
- [ ] Native input flows IMC → BRInputComponent → tags → controller stubs (log-proven)
- [ ] Zero hard asset references in any new C++ (grep-audited: no `ConstructorHelpers`,
      no hard `UPROPERTY` asset pointers)
- [ ] Findings + decisions in the Log

## Notes

- Crew: builder solo; verifier proves. No dangerous domains → no REFUTER pass needed.
- Binary files owned: `Content/Input/*` (new assets, no locks contested)
- Out of scope: any ability, any attribute, any replication beyond template defaults

## Log

(append findings here, dated, newest last)
