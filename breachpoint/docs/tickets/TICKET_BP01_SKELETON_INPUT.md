# TICKET — BP01: Project skeleton, Core, and the owned input layer

> STATUS: open — cut by lead session, 29 Jul 2026. First pickup of the project; nothing gates
> it except a **source-built** UE 5.8 + the FPS template. (Corrected 31 Jul 2026: this line
> previously read "installed UE 5.8", which contradicts the Kickoff and Done-when — an
> installed/launcher build cannot compile `BreachpointServer`. See Log.)

Founder directive: create the project exactly per `ARCHITECTURE.md` (v2) — one runtime module,
folder-per-discipline, three targets from day one, BR prefix. The input layer is OURS: Enhanced
Input → InputTag → ASC, no per-ability binding code, ever.

**Ordering law:** Step 1 gates everything in every ticket. Steps 2–4 in order.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- none (root ticket — nothing gates it), but the environment must be real:
  a **source-built UE 5.8** is installed and its path is known, and the game repo has
  `crew/`'s contents at its root (`CLAUDE.md`, `.claude/`, `docs/` all present)
- owner_path: `Source/Breachpoint/`, `Config/`, `Content/Input/`
  > `Content/Input/` added 31 Jul 2026. Step 3 authors `IMC_Default`, the `IA_*` actions and
  > `DA_InputConfig`, and this ticket's Notes already declare `Content/Input/*` as the binaries
  > it owns — but `guard_laws.py` confines every write under `Source/` and `Content/` to the
  > claimed `owner_path`, so the packet would have been blocked writing its own deliverable.
  > Harmless until now only because the hook was inert on Windows (see this ticket's Log);
  > with the hook live it fires on the first input asset.

## Steps (in order)

1. Create `Breachpoint` from the UE 5.8 **First Person C++ template**. Strip template gameplay
   to pawn/camera/input scaffolding. Add the three targets (`Breachpoint`, `BreachpointEditor`,
   `BreachpointServer` — server target must compile NOW), `Breachpoint.Build.cs` deps per
   ARCHITECTURE §3, folder skeleton (Core/Input/AbilitySystem/Character/Weapons/Match/AI/
   Online/UI/Telemetry/Data/Tests). Owner: **builder**.
   > **Enable the plugins those deps require, in this same step.** A `Build.cs` entry naming a
   > plugin module whose plugin is not enabled in the `.uproject` is a hard compile failure, so
   > this ticket's Done-when ("all three targets compile clean") cannot pass without it.
   > Audited 31 Jul 2026: the template ships only `ModelingToolsEditorMode`, `StateTree`,
   > `GameplayStateTree`. Still to enable — **`GameplayAbilities`** (BP02's entire surface),
   > **`CommonUI`** and **`ModelViewViewModel`** (BP10's Kickoff checks both by name), and
   > **`OnlineSubsystemSteam`** (BP11). Enable them now even though nothing consumes them yet:
   > an unused enabled plugin is free, and a missing one stops the packet that needs it long
   > after `Config/` has closed — the same asymmetry that decides the R17 tags in step 2.
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

**31 Jul 2026 — engine decision: 5.8 only, source-built. TD directive.**

Founder call: the project targets UE **5.8 exclusively**, and the dedicated server must work.
No mixed-version path, no deferring the server target.

*Evidence the launcher build cannot satisfy this* (measured, not assumed):
`D:\Program Files\UE_5.8` is an Installed Build (`Engine/Build/InstalledBuild.txt` present,
`SourceDistribution.txt` absent, 5.8.1 CL 56057345). Its precompiled targets under
`Engine/Intermediate/Build/Win64/x64/` are `UnrealEditor` and `UnrealGame` only — there is
no `UnrealServer` directory and no `UnrealServer.exe` in `Engine/Binaries/Win64/`.
`UnrealGame.exe` ships; `UnrealServer.exe` does not. So a `BreachpointServer` target has no
server-configuration engine binaries to link against. This is not a flag or a setting — it is
what the installed build omits. BP00's premise stands as written.

*Action:* cloning `5.8.1-release` (tag `63e13ee6`) from `EpicGames/UnrealEngine` to
`D:\UnrealEngine_5.8`. Tag pinned to **5.8.1**, not branch head, to match the CL that saved
the template's `.uasset` files. Build host: Ryzen 9 7950X / 32 threads / 63 GB, D: 602 GB free.

**31 Jul 2026 — engine built and registered. Kickoff condition 1 is now MET.**

| | |
|---|---|
| `ENGINE_ROOT` | `D:\Program Files\UE_5.8_Source` |
| Version | 5.8.1 (tag `5.8.1-release`, `63e13ee6`) — matches the CL that saved the template assets |
| Build type | **source** (`SourceDistribution.txt` present, `InstalledBuild.txt` absent) — this is what makes `BreachpointServer` compilable |
| Registered GUID | `{018BF183-4F19-F6B0-0277-F682F40F4B85}` — use this as `EngineAssociation` in `Breachpoint.uproject` |
| Toolchain | MSVC `14.50.35717` (cl.exe 14.50.35737) — UBT `FamilyRank` 0, the top preferred band for 5.8 |
| Result | `UnrealEditor Win64 Development`, exit 0, **9051 actions, zero errors**, ~52 min |

Note for whoever writes `Tools/env.local` in BP00 step 1: `ENGINE_ROOT` is the path above, and
it is machine-local — `env.local` is never committed (`testing.md` fill-in).

*Open, not yet decided* (these gate step 1 and are NOT resolved by the engine call):
1. Repo topology — tickets assume a standalone game repo with `crew/`'s contents at its root;
   the project currently sits nested at `breachpoint/breachpoint/` inside the planning repo.
2. Project name — template was generated as lowercase `breachpoint` (module `breachpoint`,
   classes `breachpointCharacter`/`GameMode`/`PlayerController`). ARCHITECTURE requires module
   `Breachpoint` and the `BR` prefix; `guard_laws.py` owner-paths and BP15's scanner both key
   off the literal path `Source/Breachpoint/`.
3. The pushed project has **two** targets, not three — no Server target exists yet.

*Resolved 31 Jul 2026 (TD):* engine source → `D:\Program Files\UE_5.8_Source`; the project
stays at `D:\Documents\Claude\Multi-agents-class\breachpoint` (one repo, project nested —
open item 1 above is closed). Open item 2 (project name / `BR` prefix) is still undecided.

---

**31 Jul 2026 — HARNESS DEFECT: `guard_laws.py` was inert on Windows. Fixed, proven.**

Found while checking what the nested-repo layout would cost the owner-path hook. The hook
computed `rel = str(Path(path).resolve().relative_to(project.resolve()))`, which on Windows
yields `Source\Breachpoint\A.cpp` — backslashes. Every downstream test compares against
forward-slash prefixes (`rel.startswith("Source/")`, `rel.startswith(("Source/", "Content/"))`).
All of them were therefore False, and `main()` fell through to `return 0` on every input.

**Consequence: on this machine the crew's laws have never been enforced by mechanism.**
Law 2/3 (banned engine APIs — `TakeDamage`, `ConstructorHelpers`, unseeded `FMath::RandRange`)
and Law 5 (owner-path confinement) were both dead. CLAUDE.md's claim that "the laws are hooks,
not goodwill" was false on Windows — this is exactly the silent-and-confident failure class the
netcode doctrine warns about, sitting in the enforcement layer itself.

*Fix:* `.as_posix()` on the relative path — normalizes separators on every platform. One line.

*Proof (red-then-green, same 6 cases, new suite `.claude/hooks/test_guard_laws.py`):*

| Case | pre-fix | post-fix |
|---|---|---|
| engine damage API into `Source/` | exit 0 — **not blocked** | exit 2 blocked |
| hard `ConstructorHelpers` ref | exit 0 — **not blocked** | exit 2 blocked |
| unseeded `FMath::RandRange` | exit 0 — **not blocked** | exit 2 blocked |
| write outside claimed `owner_path` | exit 0 — **not blocked** | exit 2 blocked |
| write inside `owner_path` | exit 0 (vacuous) | exit 0 |
| ticket Log always allowed | exit 0 (vacuous) | exit 0 |

Pre-fix 2/6, and both "passes" were vacuous — everything passed. Post-fix 6/6.

*Follow-up for whoever picks up BP14 step 5 / BP15 step 6:* the adversarial questions those
steps ask ("can a builder land a diff outside `owner_path`?") had a **yes** answer this whole
time, for a reason no prompt-level review would have found. Re-run `test_guard_laws.py` as part
of those passes rather than reasoning about the hook's source.
