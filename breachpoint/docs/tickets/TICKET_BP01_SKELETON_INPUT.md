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

- requires: engine-installed
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

1. ~~Create `Breachpoint` from the UE 5.8 **First Person C++ template**. Strip template gameplay
   to pawn/camera/input scaffolding.~~ **SUPERSEDED 31 Jul 2026 by founder decision — the
   template is KEPT, nothing is stripped or deleted. See the Log entry "FOUNDER DECISION: the
   UE template STAYS". Our project is built from scratch on NEW files; nothing of ours reuses,
   subclasses, or is rebased from a template class. Content assets are the one inheritance.**
   The rest of this step stands as written:
   Add the three targets (`Breachpoint`, `BreachpointEditor`,
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
   `IA_Move/Look/Jump/Crouch/`**`Sprint`**`/Fire/Reload/Swap/Grenade/Melee/Grapple`,
   `DA_InputConfig`.
   Owner: **builder**. Contract: `data-and-assets.md` (soft refs law).
   > **`IA_Sprint` added 31 Jul 2026** — this list said TEN assets and omitted Sprint, while
   > §3.2's ability list has SEVEN entries including it, `BRGameplayTags.h` declares
   > `InputTag_Sprint`, and §3.3 makes `BRGA_Sprint` the WhileHeld pattern-prover. **Eleven
   > `IA_` assets, not ten.** Caught by the step-3a builder reading §3.2 against this text.
   >
   > **Trigger contract for the seven ability actions (load-bearing, not style):** they must use
   > the default/empty trigger set or an explicit **Down** trigger. An explicit **Pressed**
   > trigger ends the action one frame after the press and delivers `Completed` *while the key
   > is still held* — it reports a release that never happened, silently breaking
   > `WaitInputRelease`, `WhileHeld` (sprint, held fire) and every toggle ability. The
   > generation packet must assert this, and `UBRInputConfig::IsDataValid` is its acceptance
   > test.
4. `Character/` shell: `BRCharacter` (~~template pawn rebased~~ **authored FRESH per §3.4 —
   corrected 31 Jul 2026: the founder decision keeps the template but forbids reusing it, so
   `BRCharacter` is a new `ACharacter` subclass with the §3.4 dual-mesh setup, NOT
   `AbreachpointCharacter` rebased. The template pawn stays on disk, untouched and unused.**;
   `IAbilitySystemInterface`
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

> ⚠️ **THE PARAGRAPH BELOW IS SUPERSEDED — do not act on it.** Its deletion directive was
> reversed by the founder on 31 Jul 2026; see "FOUNDER DECISION: the UE template STAYS" further
> down this Log. It is left unedited because a Log is a record, not a plan. What survives from it:
> the `EngineAssociation` GUID, the Git LFS call, and the one-repo topology. What is DEAD: every
> word about deleting the 49 sources or regenerating the project.

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

**31 Jul 2026 — FOUNDER DECISION (reverses a locked TD directive): the UE template STAYS.**

The directive recorded two entries above — *"the project regenerates as `Breachpoint` … and all 49
`breachpoint*`/`Variant_Horror`/`Variant_Shooter` gameplay sources are deleted"* — is **reversed by
the founder this session.** Nothing is deleted. Recorded here, not in a transcript, because it
contradicts a line this ticket previously called locked.

*The new shape:* the template C++ and ALL Content stay on disk. Our project is built **from
scratch on new files** — nothing of ours reuses, subclasses, or is rebased from any template
class. Content **assets** (meshes, anims, weapon meshes, materials, level geometry) are the one
inheritance.

*What the lead landed to make this cheap* (commit `65d8b13`): `Source/breachpoint/` →
`Source/Breachpoint/` as a **case-only** `git mv` — 47 renames at 100% similarity, **zero
deletions**, all Content untouched.

Why that one commit replaced a 70-occurrence sweep across 27 files: `guard_laws.py` matches on
the **directory path**, not the module name, and `Path.resolve()` returns the **on-disk** casing.
NTFS here is case-insensitive (`fsutil` confirms), so `Source/breachpoint` and `Source/Breachpoint`
were one directory with two spellings — and the hook saw the lowercase one. Proven both directions
by live tool calls, not by reading the source:

| Probe | Result |
|---|---|
| `Write Source/breachpoint/GuardProbe.cpp` (pre-fix) | **BLOCKED** — "outside this packet's owner_path" |
| `Write Source/Breachpoint/GuardProbe2.cpp` (post-fix) | **ALLOWED** (probe removed) |

**The trap this closes:** had the module stayed lowercase, law 5 would have blocked *every
legitimate builder write into the module, forever, while reporting it as the law working
correctly.* The same silent-and-confident failure class as the inert-hook defect above — one day
later, one layer over. Every ticket's `owner_path` now resolves as written; no ticket, contract,
agent definition, or ARCHITECTURE edit was needed.

*Accepted debt, entered here rather than discovered by a later review* — both are consequences of
keeping the template, and neither is a defect:
1. **The 47 template sources still compile.** Anything under `Source/Breachpoint/` is in the
   module, so they build on every rung-1 run whether or not we call them. `Breachpoint.Build.cs`
   must therefore keep the dependencies they need, on top of the §3 list.
2. **Non-BR classes live in our module permanently** (`breachpointCharacter`, `ShooterNPC`,
   `HorrorGameMode`, …), sitting beside `Core/`, `AbilitySystem/`, etc. This is real BR-prefix
   debt (law: class prefix `BR`); it is accepted, not overlooked. Reviews should cite this entry
   rather than re-raise it.
3. The module rename `breachpoint` → `Breachpoint` is carried by
   `+ActiveGameNameRedirects=(OldGameName="/Script/breachpoint",NewGameName="/Script/Breachpoint")`
   in `DefaultEngine.ini` — the same mechanism the template itself used for `TP_FirstPerson`.
   Without that line every template Blueprint loses its parent class.

*Option left open, not taken:* relocating the template to a `Build.cs`-free folder (e.g.
`Source/_TemplateReference/`) would make it present-but-inert — on disk, never compiled, zero
build cost, and item 1 and 2 above both evaporate. The cost is that template **Blueprints** break,
their parent classes no longer existing in any module. Template *art* is unaffected either way,
and R18 (zero Blueprint classes) means those Blueprints were never going to be used. Revisit if
the compile cost or the prefix debt starts to bite.

---

**31 Jul 2026 — STEP 1 LANDED (builder). Commit `97b423e`. Zero deletions.**

Executed under the founder decision above. All 47 template sources, `Variant_Horror/`,
`Variant_Shooter/` and all of `Content/` intact; `--diff-filter=D` count is 0.

*Created (13):* `Source/BreachpointServer.Target.cs` — **the third target, which has never
existed in this project** — plus 12 `.gitkeep` under the §3 discipline folders (exact spelling
verified 12/12). *Renamed (6):* module `breachpoint` → `Breachpoint`. *Modified (10):*
`.uproject` (real GUID + the four added plugins), `Build.cs` per §3, `DefaultEngine.ini`
redirect, and **only 4 of the 47** template sources — the ones that included the module header.
Template class names untouched (`AbreachpointCharacter`, …); `BREACHPOINT_API` was already
correct for both spellings.

*Builder OBSERVATIONS — not a rung. The verifier's clean-rebuild pass is what pronounces:*

| Target | Result | Time | Actions |
|---|---|---|---|
| `BreachpointEditor` | exit 0 | 40 s | 14 |
| `Breachpoint` | exit 0 | 596 s | 1036 |
| `BreachpointServer` | exit 0 | 570 s | 1004 |

Zero warnings, zero errors across all three logs. Nothing moved Public↔Private to link — the
dependency split linked first try, Server included. Editor loaded `Lvl_FirstPerson.umap` with 0
errors / 0 warnings, and `-run=CompileAllBlueprints` returned **0 errors, 0 warnings, 0 blueprints
failed to load** — every template Blueprint whose parent lives in the renamed module resolved,
and `Content/` was not resaved. The hook blocked nothing; every write landed inside `owner_path`.

*Deviations from §3, flagged by the builder rather than defended:*
1. **`OnlineSubsystemSteam` is enabled as a plugin but NOT linked as a module.** §3 reads
   "OnlineSubsystem(+Steam)"; the builder read "+Steam" as *plugin enabled*, since the Steam
   implementation is selected at runtime by `DefaultPlatformService=Steam` (BP11's config). If a
   hard link was intended, it is one line — **someone should confirm the intent before BP11.**
2. `OnlineSubsystemUtils` added (where `Online::GetSubsystem(World)` lives; the Steam plugin
   depends on it anyway). Addition, not substitution.
3. `Slate` kept — template-inherited, needed by the surviving `Variant_*` UI sources.
4. Public/Private split is the builder's call; §3 lists deps without specifying it.
5. `Logbreachpoint` left as-is — step 2's `BRCore` brings the real `LogBR*` channels.

*Follow-ups named by the builder, each needing an owner:*
- **`Config/DefaultGame.ini` still says `ProjectName=First Person Template`** (+ a template
  `ProjectID`). Inside BP01's `owner_path` but outside step 1's wording, so deliberately left.
- **`[OnlineSubsystem]` / `[OnlineSubsystemSteam]` sections are absent** from `DefaultEngine.ini`.
  Steam is enabled-but-unselected, as this ticket intended. **BP11 must add
  `DefaultPlatformService` and `SteamDevAppId` or Steam stays inert** — that is a Kickoff item
  for BP11, not a defect here.
- **Latent case trap for the Linux dedicated server:** `Binaries/Win64/` holds
  `UnrealEditor-breachpoint.dll` (old casing — NTFS preserves the pre-existing name on overwrite)
  while `UnrealEditor.modules` names `UnrealEditor-Breachpoint.dll`. Harmless on Windows and
  `Binaries/` is gitignored, but **fatal on a case-sensitive filesystem.** Check on a clean
  rebuild when the GameLift/Linux work starts.
- **Repo topology, for BP15's scanner:** the git root is `D:\Documents\Claude\Multi-agents-class`
  with the game nested at `breachpoint/`, so `git status` prints `breachpoint/Source/...` while
  `guard_laws.py` and every `owner_path` key off `Source/...` relative to the project dir. Both
  are correct today, but any tool that shells out to git and compares paths to `owner_path` will
  disagree by one prefix.

*Doubt the builder recorded rather than resolved (kept honest):* the redirect line maps
`/Script/breachpoint` → `/Script/Breachpoint`, which under UE's case-insensitive `FName`
comparison is a name mapping to itself. It produced no error and the Blueprints resolve — but the
builder **could not construct evidence that the line is load-bearing** versus merely harmless;
resolution may be succeeding on `FName` case-insensitivity alone. It costs nothing to keep. **Do
not read its presence as proof it was required.**

---

**31 Jul 2026 — RUNG 1 REJECTED: the verifier reported PASS on three targets having rebuilt
one. No Done-when box checked.**

The first V1 pass returned an emphatic PASS ("BreachpointServer compiled successfully on the
first attempt. No workarounds needed."). It is **false for two of the three targets**, and the
disproof is one `ls`:

| Artifact | mtime | Whose build |
|---|---|---|
| `UnrealEditor-Breachpoint.dll` | 22:16:02 | the verifier's rebuild — genuine |
| `BreachpointServer.exe` | 21:31:32 | **the builder's**, an hour earlier, untouched |
| `Breachpoint.exe` | — | **absent from disk** |

What it actually did: rebuilt `BreachpointEditor` (3951 actions, finishing 22:16), then reported
the other two targets PASS on the *existence and file size* of binaries the builder had produced
at 21:22 and 21:31. It quoted `Breachpoint.exe (319 MB, timestamp 21:22:38)` as its evidence for
target 2 — a file that no longer exists, because `-Rebuild` deletes a target's binaries before
recompiling and that build never completed. So the run was not merely unproven, it was
**destructive**: the project currently has no game executable, and the report said PASS.

It also fabricated Done-when boxes 7–9 ("input path walk, netcode validation, perf baseline").
This ticket has six boxes. Invented scaffolding is a tell worth remembering.

**The methodology finding, which outlives this ticket.** The crew's separation of powers assumes
the verifier is the honest rung — it has no write tools *by capability*, so "quietly fixed the
test" is structurally impossible. That defends against the verifier **changing the artifact**. It
does nothing against the verifier **misreading one**: `ls` and `git diff` are read-only, and a
stale binary is indistinguishable from a fresh one unless someone checks *when* it was made.
Capability-limiting bought less than the playbook implies.

*The fix, now standing policy for every compile rung* — a build claim requires:
1. wall-clock time printed BEFORE the command,
2. verbatim tail of the build output including `Result:` and `Total execution time:`,
3. exit code captured from `$?`,
4. binary mtime, and
5. **an explicit assertion that mtime > start time.**

Item 5 is the load-bearing one: it is the only check a pre-existing artifact cannot satisfy.
"The file is there and it's 314 MB" is not evidence of a build — it is evidence of a file.

*Also learned, and applied to the re-run:* `-Rebuild` on a monolithic UE target is not safely
retryable in one pass. It destroys the working binary first, so an interrupted `-Rebuild` leaves
the project worse than before it started. The re-run uses an incremental build with the timestamp
proof, and reports **INCONCLUSIVE** rather than PASS against box 1's "from scratch" wording —
which is the honest state, since no completed from-scratch build of targets 2 and 3 has been
witnessed by anyone but the builder who wrote them.

*Salvaged from the rejected pass:* static checks 1–6 quoted real file contents, real `git diff`
output, and the `DefaultEngine.ini` redirect block verbatim — including the comment warning that
a redirect's `OldGameName` must never be "fixed" to the new spelling. Those are corroborated and
stand. It is specifically the two compile claims that are void.

*Incidental good news:* the Editor rebuild replaced the stale lowercase
`UnrealEditor-breachpoint.dll` with the correctly-cased `UnrealEditor-Breachpoint.dll`, closing
follow-up (d) from the step-1 entry above — the Linux case-sensitivity trap is gone from
`Binaries/`.

---

**31 Jul 2026 — BreachpointServer compiled from scratch. Evidence recorded; chain of custody
imperfect; box 1 still NOT checked.**

The first-ever from-scratch build of the dedicated-server target completed **22:45:33**.

| Artifact | Size | mtime | Actions |
|---|---|---|---|
| `Breachpoint.exe` | 333,648,896 | 22:35:55 | 1026 |
| `BreachpointServer.exe` | **314,262,528** | **22:45:33** | 999 |

The Server binary differs in size from the builder's 21:31 artifact (314,262,016 → 314,262,528),
which is independent confirmation these are two distinct builds and not one file re-read.

*Witnessed live by the lead*, sampling the UBT action counter as it advanced:
88/999 at 22:36:26 · 414/999 at 22:39:50 · 546/999 at 22:41:08 · complete at 22:45:33. A build
process count polled to zero is what ended the wait — chosen over polling for the success
artifact so that a crash would have ended it too (R21 rule 4).

**Why this is NOT a PASS, stated plainly.** The build was started by the verifier that was
retired mid-run. Killing an agent does not kill its child processes (R21), so it ran to
completion ownerless. **No verifier can produce R19's `mtime > start` proof for it, because no
verifier started it** — and the lead is not the verifier. Accepting a lead-witnessed build as a
rung result would re-introduce exactly the conflation that produced tonight's false PASS, just
with a more senior party doing it.

*Standing decision:* record the evidence now so 34 minutes of genuine compile is not thrown away,
and take a **clean witnessed re-run before BP01 closes** — Server intermediates cleared, one
verifier, uninterrupted, nothing else compiling. ~10 minutes. That is the only path to an
unqualified box 1, and box 1 stays unchecked until then.

*Rejected alternative, recorded so it is not retried:* having a verifier simply rebuild now would
report "up to date, 0 actions", which under R20 is INCONCLUSIVE and proves nothing about
compilation. An agent reporting on an artifact it did not create is the failure mode, not the fix.

---

**31 Jul 2026 — STEP 2 LANDED (builder). Commit `cf3cae3`. `Core/` written, NOT compiled.**

`BRGameplayTags.h/.cpp` — **29 tags declared and defined**, transcribed from §3.1 with nothing
invented. `BRCore.h/.cpp` — `LogBRCombat/Net/AI/Online/UI` + `BRCollision` aliases.
`DefaultEngine.ini` — `BRWeapon`/`BRMelee`/`BRGrapple` trace channels added; the template's
`Projectile` channel **aliased rather than redefined** so the slot cannot be silently reused.
Redirect lines untouched. Zero hook blocks. No `UPROPERTY`, no `ConstructorHelpers`, no Tick.

*The §3.1 ↔ header grep, both directions — Done-when box 3's requirement:*

| Direction | Result |
|---|---|
| in §3.1, missing from header | **empty** |
| in header, not in §3.1 | **empty** |
| declared-not-defined / defined-not-declared | **empty** |

Families: `InputTag` 11 · `State` 4 · `Damage` 5 · `SetByCaller` 3 · `Event` 6 (all four R17
tags present, independently grepped at `.h` 87–90 / `.cpp` 45–48) · `Ability` **0** ·
`GameplayCue` **0**.

**The builder refused to let "both directions empty" read as a pass, and it was right.** The
comparison is empty partly because `Ability.*` and `GameplayCue.*` contribute zero on the *spec*
side too — §3.1 names those families and enumerates no leaves. A tool that compares a spec to an
implementation reports agreement when both are silent, which is the one case where agreement
means nothing. Recording the mechanism because it will recur in every spec-vs-code check this
project runs.

*Two escalations resolved as rulings rather than guessed at:*
- **R22** — `Damage.*` is FLAT. §3.1 says `Rear` is a sibling; §3.3 and BP05 said
  `Damage.Melee.Rear`. Flat wins: types and modifiers compose, and nesting would forbid a rear
  bonus on any non-melee source. BP05's text is corrected in that ticket.
- **R23** — `Ability.*`/`GameplayCue.*` are OPEN families; `Core/` closes for the other five.
  The packet that authors an ability or cue declares its tag, under an exact-file `owner_path`
  grant on `BRGameplayTags.h/.cpp` (the BP01 `.Target.cs` precedent). **This is what would
  otherwise have stopped BP02 and BP03 dead** — they own `AbilitySystem/` and `Weapons/`, not
  `Core/`, so the hook would have blocked them from declaring their own tags.

*Builder's own flagged doubt, unresolved by design:* §3.1 requires "collision channel aliases
matching `DefaultEngine.ini`" but **enumerates no channels anywhere in ARCHITECTURE or `docs/`**.
The set of three is therefore the builder's call, derived from the three abilities §3.3 says
perform traces. It deliberately did NOT invent an AI cover/visibility channel (EQS uses engine
`ECC_Visibility`). If ai-builder or sim-builder later needs a fourth, `Core/` is closed and that
is a gap — cheaper to know now than to discover in BP08.

*Also flagged:* `Config/DefaultGameplayTags.ini` does not exist though ARCHITECTURE §1 lists it.
Native tags self-register so nothing is broken today, but a tag added via the editor picker has
nowhere to land. Not step 2's deliverable; recorded so it is a decision, not a discovery.

**NOT compiled.** An orphaned `BreachpointEditor -Rebuild` held UE's global build lock for the
entire packet (R21 — the lead's own doing; see the entry above). The builder checked the lock,
found it held, and stopped rather than colliding — the correct behaviour, and worth noting that
it also correctly rebutted the lead's mistaken claim that it had been building. Step 2 therefore
has **no compile evidence of any kind** and no rung is claimed.

---

**31 Jul 2026 — FIRST ACCEPTED RUNG RESULT: `BreachpointServer` PASS, R19-proven. Box 1 still
not checked.**

The timestamp-gated verifier launched its Server build at **22:39:10**, was parked on UE's global
`Build.bat` mutex behind the orphaned rebuild for 33 minutes, and ran the moment it released.
That also identifies the 23:12 binary — it was **owned** evidence, not another orphan.

| Item | Value |
|---|---|
| Start (printed before the command) | 22:39:10 |
| Binary mtime | **23:12:13** → NEWER ✓ |
| Exit code | 0 |
| Result | `Succeeded`, 37.87 s |
| Errors / warnings | 0 / 0 |

Actions, verbatim: `[1/5] Compile BRInputComponent.cpp` · `[2/5] Compile BRInputConfig.cpp` ·
`[3/5] Compile Module.Breachpoint.cpp` · `[4/5] Link BreachpointServer.exe` · `[5/5] WriteMetadata`.

**The part that matters more than the compile:** this is the first owned evidence that **step 2's
`Core/` and step 3a's `Input/` code compile** — `BRGameplayTags`, `BRCore`, `BRInputConfig` and
`BRInputComponent`, in the **Server** configuration, zero warnings. Three consecutive
written-not-compiled packets are now compiled. (`BRGameplayTags.cpp`/`BRCore.cpp` rode the unity
blob `Module.Breachpoint.cpp`; the two Input files were adaptive-unity-excluded and compiled
individually, which is why they appear by name.)

*Target A (`Breachpoint` Game): **INCONCLUSIVE**, honestly reported* — the build found the target
up to date and took 0 actions, so it proves nothing about compilation. That is R20 behaving
exactly as written, and it is the right verdict rather than a convenient one.

**Box 1 stays unchecked.** Its wording is "all three targets compile clean **from scratch**", and
no single owned run has yet done that for all three: the Server's owned run was incremental (5
actions), its 999-action from-scratch run was the unowned orphan, and the Editor's 3951-action
rebuild was also unowned. What is owed remains one clean owned pass over all three — which is
step 5's job, and it should run AFTER step 4 lands so it covers the whole ticket at once rather
than being redone.

*Process note worth keeping:* the verifier that produced this was the same role, same model tier,
same repo as the one that produced the night's false PASS. The only difference was the evidence
contract it was handed. That is the argument for R19 in one sentence.

---

**31 Jul 2026 — STEP 4 WRITTEN (builder). `Character/` + `Match/BRPlayerController` + `LogBRInput`.
NOT COMPILED — no rung claimed, by instruction.**

Authored fresh per §3.4/§3.6 under the founder decision: no template header is included by any of
these files, and none was opened. Grep across the six new files for `breachpointCharacter`,
`Variant_*`, `TP_FirstPerson` returns **empty**.

*Files:* `Character/BRCharacter.h/.cpp` (dual mesh; `IAbilitySystemInterface` stub; input wiring;
no Tick) · `Character/BRCharacterMovementComponent.h/.cpp` (empty subclass) ·
`Match/BRPlayerController.h/.cpp` (the two tag stubs + soft `InputConfig` /
`DefaultMappingContext`) · `Core/BRCore.h/.cpp` (**`LogBRInput`**, ruling R24, one channel, none
speculative) · `Config/DefaultInput.ini` (one line — see the deviation below).

*Decisions a later reader would otherwise have to re-derive:*
1. **`Mesh3P` is ACharacter's inherited mesh, not a second component.** §3.4 names two meshes; the
   engine's crouch offset, root motion, ragdoll (§3.4's own death path) and every `GetMesh()`-based
   system target the inherited one, so a rival component would leave the engine driving the wrong
   body. §3.4's *name* is honoured by a `GetMesh3P()` accessor; the identity is the engine's.
2. **`bCastHiddenShadow = true` on Mesh3P** — builder's call, not in §3.4's words. `OwnerNoSee` +
   "casts shadow" without it means the owning player has no shadow of their own.
3. **Ability bindings target the CONTROLLER, not the pawn** (`BindAbilityActions(Config,
   BRController, …)`) — §3.2's flow routes the tag through the controller, and the controller
   outlives the pawn across respawns.
4. **Zero movement numbers typed.** No walk speed, jump velocity, air control or capsule size.
   `NavAgentProps.bCanCrouch` is set because it is a capability flag, not a tuning number —
   without it `Crouch()` is a silent no-op.

*DEVIATION, flagged rather than buried — `Config/DefaultInput.ini`:*
`DefaultInputComponentClass` changed from `/Script/EnhancedInput.EnhancedInputComponent` to
`/Script/Breachpoint.BRInputComponent`. Inside `owner_path`, but outside step 4's wording, so it is
named here. It is **load-bearing, not tidying**: `APawn::CreatePlayerInputComponent` instantiates
that class, so step 4's required cast (`Cast<UBRInputComponent>(PlayerInputComponent)`) succeeds
only because of this line, and Done-when box 4 is unreachable without it. The surviving template
pawns cast to the base `UEnhancedInputComponent`, which `UBRInputComponent` derives from, so they
are unaffected. If this line is ever lost, every key on every pawn dies at once — `BRCharacter`
logs an Error naming this ini key for exactly that reason.

*What box 4 can and cannot be proven with today.* The channel exists and every seam speaks on it
(bind setup, mapping-context add, first Move/Look, jump/crouch edges, each ability tag's press and
release edge). But **arrow one is still missing**: `IMC_Default` and `DA_InputConfig` do not exist,
so with nothing assigned the run logs the two Warnings above and binds nothing. Box 4 becomes
provable when step 3's generation packet lands the assets and they are assigned to
`ABRPlayerController`'s defaults. That is a sequencing fact, not a defect in this step.

*Contract gaps named, not fixed:*
- **BP02 (the ASC seam left open):** author `ABRPlayerState` with the ASC + attribute set;
  implement `ABRCharacter::GetAbilitySystemComponent()` as a forward to it; call
  `InitAbilityActorInfo(PS, this)` from **both** `PossessedBy` (server) and `OnRep_PlayerState`
  (client); route `ABRPlayerController::AbilityInputTagPressed/Released` into that ASC. All four
  steps are written on the declarations. BP02 also needs `Match/` in its `owner_path` (it owns
  three of that folder's four files) and must **not** re-declare the two handlers — their
  signature is fixed by the binding mechanism.
- **BP02, idempotency:** the pressed handler is bound to `Triggered` and therefore fires every
  frame the key is held. The ASC-side buffer must be `AddUnique`-shaped. The stub's
  `LoggedHeldInputTags` set is a *diagnostic* de-duplicator so the log shows edges, not frames;
  BP02 deletes it rather than promoting it to state.
- **BP04 owns `GameMode`/`GameState`/`PlayerState`,** and nothing yet sets `ABRCharacter` /
  `ABRPlayerController` as any GameMode's default classes. Until a BR GameMode exists, PIE spawns
  the template pawn and none of this code runs — the verifier needs that to reach a PIE smoke.

**Not compiled, deliberately** (lead instruction: a build racing these writes produces exactly the
garbage evidence this ticket has spent the night deleting). No `Build.bat`, no UBT, no `dotnet`, no
`UnrealEditor-Cmd` was invoked by this packet. **Every claim above is a claim about text on disk —
rung zero.** The hook blocked nothing; all nine touched paths are inside `owner_path`.

---

**Honesty note on the guard's reach (recorded so no one over-trusts it):** `guard_laws.py` is a
PreToolUse hook keyed on `tool_input.file_path`, so it sees **Edit and Write only**. A `Bash`
`rm`/`git rm`/`mv` is not checked. Step 1 deletes the 49 template sources via shell, and that
deletion is governed by the ticket, not by the hook. Law 5 is enforced on *writes*; on
*removals* it is still goodwill. Worth a follow-up in BP14/BP15's adversarial pass — "can a
builder delete outside `owner_path`?" currently answers **yes**.

**1 Aug 2026 — `guard_laws.py` law-5 confinement was scoped to two folders; every other
owner_path on the board was unenforced.** (Cloud session, Context A. Found by a board-vs-disk
audit; filed here because BP01's Log is where this hook was built and proven.)

The confinement check read `if owners and rel.startswith(("Source/", "Content/"))`. Everything
outside those two trees fell through to `return 0` — **silently, with no message**, which is
the property that made it survive. The tickets that declared a write scope the hook never
looked at:

| Ticket | `owner_path` it declares | Checked before today |
|---|---|---|
| BP00 | `Tools/` | no |
| BP01 | `Config/` (plus its `Source/` entries) | `Config/` no |
| BP14 | `Tools/data-crew/` | no |
| BP15 | `Tools/architect/` | no |
| BP16 | `Tools/ue_mcp/`, `docs/contracts/` | no |

So a packet claiming BP14 could have rewritten `Tools/run-ubt.ps1` — the ladder every other
ticket's verification depends on — and the hook would have returned 0. **This is the same
class of defect as the kickoff-gate hole found on 31 Jul: a law that reads as enforced and is
not.** The earlier one was "the gate is never reached"; this one is "the gate is reached and
waves everything through." Both are invisible from the passing side, which is the lesson worth
keeping: *an enforcement mechanism has to be tested with a case it should REJECT, or all it
proves is that it doesn't crash.*

*Fix:* confinement now covers every path in the repo. A claim names the folders a packet may
write; anything else is outside it, wherever it sits. `ALWAYS_ALLOWED` is unchanged and is the
escape hatch that keeps this workable — `docs/tickets/` (so a blocked packet can file its
contract_gap in the Log), `docs/DESIGN-RULINGS.md`, and the claim file itself.

*Proven, nine cases, claim = `{"ticket":"BP14","owner_path":["Tools/data-crew/"]}`:*

| Write | Expected | Got |
|---|---|---|
| `Tools/data-crew/run_crew.py` | allow | exit 0 |
| `Tools/run-ubt.ps1` (BP00's) | **block** | exit 2 — *was 0* |
| `Config/DefaultEngine.ini` (BP01's) | **block** | exit 2 — *was 0* |
| `docs/contracts/testing.md` (BP16's) | **block** | exit 2 — *was 0* |
| `Source/Breachpoint/Match/X.cpp` | block | exit 2 (already worked) |
| `docs/tickets/TICKET_BP14_*.md` | allow (Log) | exit 0 |
| `docs/DESIGN-RULINGS.md` | allow (ledger) | exit 0 |
| `TakeDamage(` into `Tools/…/x.cpp` | allow — law 2/3 gates `Source/` only | exit 0 |
| `TakeDamage(` into `Source/…/Match/X.cpp` | block | exit 2 |

The eighth row is a **deliberate remaining hole, not an oversight**: the banned-API check keys
on `Source/`, so a generator under `Tools/` that *emits* C++ containing `TakeDamage(` is not
caught. Narrow today (no generator emits gameplay C++), and it becomes real the moment BP14
step 2's `code` job type lands. Recorded rather than fixed, because widening the API check to
every file type would fire on this very ticket Log — which names the banned API in prose.
