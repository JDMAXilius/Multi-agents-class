# TICKET — BP01: Project skeleton, Core, and the owned input layer

> STATUS: in-progress — Windows box (lead session), 31 Jul 2026 (68e0d0b). Kickoff verified
> mechanically: source-built UE 5.8 present at `D:\Program Files\UE_5.8_Source`
> (`SourceDistribution.txt` present), crew kit at repo root, `guard_laws.py` armed and proven
> 6/6 this session. First claim in the project's life.

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
- owner_path: `Source/Breachpoint/`, `Config/`, `Content/Input/`,
  `Source/Breachpoint.Target.cs`, `Source/BreachpointEditor.Target.cs`,
  `Source/BreachpointServer.Target.cs`
  > The three `.Target.cs` entries added 31 Jul 2026 (second owner_path correction — see Log,
  > "contract_gap 2"). They are listed as exact files, not by widening scope to `Source/`:
  > `guard_laws.py` matches an owner entry by `rel == o` OR `rel.startswith(o + "/")`, so a
  > file directly in `Source/` needs its own exact entry, and granting `Source/` would hand
  > this packet every other discipline's folder.
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

---

**31 Jul 2026 — SESSION LAW: the crew binds to the launch directory, not to the repo.**
A lead session was attempted and **aborted before the claim.** No code was written.

A Claude Code session resolves agents, skills, and hooks from the directory it was **launched
in** — not from wherever its files happen to be readable. The session was launched in
`C:\Users\juand`, one level outside the game repo, and the result was total:

| Expected | Actually loaded in that session |
|---|---|
| 12 crew agents | **none** — only account-level `general-purpose`, `Explore`, `Plan`, … |
| `game-lead`, `tickets` skills | **neither** — no `/tickets list`, no Kickoff verification |
| `guard_laws.py` PreToolUse hook | **not registered** — `.claude/settings.json` is project-scoped and never loaded; its `$CLAUDE_PROJECT_DIR` would have resolved to the home directory regardless |

This is worse than the name-collision hazard `CLAUDE.md` warns about. There, a generic-named
agent (`builder`, `critic`, `verifier`) is *shadowed* by an account-level twin and misbehaves
subtly. Here they are simply **absent**, and — the part that matters — **the hook is absent with
them.** Dispatching BP01 from that session would have put writes into `Source/Breachpoint/` with
the laws sitting in the repo enforcing nothing: the exact condition proven dead and fixed in the
entry above, re-created one day later by a different mechanism. Law 5 does not fail loudly when
it fails this way. It just stops existing.

*Ruling:* **every crew session starts with the working directory at the game repo root.** Not
`--add-dir`, which grants file access (already sufficient) but not project identity — there is no
mid-session command that re-binds agents, skills, or hooks. Wrong root ⇒ exit and relaunch.

*Cheap pre-flight, before any claim:* ask for `/tickets list`. If the skill does not exist, the
hook does not either, and the session is not a crew session no matter what it can read.

*Verified this date on the Windows box (by execution, not by reading this Log):*

| Check | Result |
|---|---|
| `.claude/` contents committed | **22 files** — 12 agents (9 + 3 curators), 7 skills, 2 hooks, `settings.json`. Nothing machine-local; a clean clone gets the whole crew |
| `python3` resolves | Python 3.11.9 (real interpreter, not the Store stub) — the hook's `python3` command executes |
| `test_guard_laws.py` | **6/6 passed** — the `.as_posix()` fix holds on this machine |

*Cross-machine note (macOS):* the crew kit is plain text and portable — a Mac clone can read the
board, edit docs, and land Log entries. It **cannot** run this ticket. Steps 1–5 need the Win64
source-built engine at `ENGINE_ROOT` and `BreachpointServer` is a Win64 target; rung 1 exists
only on the Windows box. A Mac session may prepare, never pronounce — honesty ladder unchanged:
the rung belongs to the machine that ran it.

*Still unlanded, carried to the real session's first act* (TD directive, recorded here so it does
not die in a transcript): open item 2 above is **closed** — the project regenerates as
`Breachpoint` with the `BR` prefix from the UE 5.8 First Person C++ template, and all 49
`breachpoint*`/`Variant_Horror`/`Variant_Shooter` gameplay sources are deleted; only Content
(meshes, anims, weapon meshes, input assets) and project scaffolding survive. `.uasset`/`.umap`
track via **Git LFS**. The `EngineAssociation` GUID is in the table above and is not yet written
into any `.uproject`.

*Open, unrecorded elsewhere:* the four owner_path mismatches for **BP02/03/05/06** — expected as
`contract_gap`s once those packets claim — are **not written down in this repo.** Only BP01's own
correction (`Content/Input/`, above) exists. Whoever holds that analysis should land it in the
respective tickets before the packets run, or it will be rediscovered the hard way.

---

**31 Jul 2026 — CONTRACT_GAP 2 (lead, pre-claim): the three `.Target.cs` files were outside
this ticket's own owner_path.** Filed and fixed in the Kickoff block above BEFORE dispatch.

Found by reading `guard_laws.py` rather than by a builder hitting the wall. Law 5 tests
`rel.startswith(("Source/", "Content/"))` and then requires `rel == o` or
`rel.startswith(o + "/")` for some owner entry `o`. With `owner_path = ["Source/Breachpoint/",
…]`, the path `Source/Breachpoint.Target.cs` matches neither — `"Source/Breachpoint.Target.cs"`
does not start with `"Source/Breachpoint/"`. So step 1's headline deliverable ("add the three
targets; the server target must compile NOW") and the first Done-when box ("all three targets
compile clean") were **mechanically unreachable by the packet that owns them.**

Same defect class as the `Content/Input/` correction one entry above, and it was invisible for
the same reason: until today the hook was inert on Windows, so an owner_path could be wrong for
weeks without anyone paying for it. **Arming the guard is what made both bugs findable.** Expect
more of these as each packet claims — the four carried BP02/03/05/06 mismatches are the same
shape. Verifying `owner_path` against the ticket's actual deliverables is now a pre-claim step
of the lead's, not a discovery the builder makes mid-packet.

*Fix chosen:* three exact-file entries, NOT widening to `Source/`. Widening would grant this
packet `Source/Breachpoint/AI/`, `…/Online/`, and every other discipline's folder for the sake
of three files — trading a false block for a real hole.

**Honesty note on the guard's reach (recorded so no one over-trusts it):** `guard_laws.py` is a
PreToolUse hook keyed on `tool_input.file_path`, so it sees **Edit and Write only**. A `Bash`
`rm`/`git rm`/`mv` is not checked. Step 1 deletes the 49 template sources via shell, and that
deletion is governed by the ticket, not by the hook. Law 5 is enforced on *writes*; on
*removals* it is still goodwill. Worth a follow-up in BP14/BP15's adversarial pass — "can a
builder delete outside `owner_path`?" currently answers **yes**.
