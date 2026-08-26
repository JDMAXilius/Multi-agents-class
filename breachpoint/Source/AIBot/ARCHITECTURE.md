# AIBot — module architecture (the short law; the long form is docs/AIBOT-ROADMAP.md)

Self-contained bot framework. Halo Infinite's multiplayer-bot architecture, 1:1 where
published; our own designs, flagged as ours, where 343 published nothing (confidence
internals, curve shapes, every number). Plugin-shaped: extraction to `Plugins/AIBot/` is
Phase 10 and must stay a folder move.

## The five laws of this module

1. **Boundary.** Engine dependencies only — `GameplayAbilities` is deliberately absent from
   Build.cs, so GAS purity is a LINKER guarantee. Zero references to any game module,
   comments and abbreviations included. Check (CASE-INSENSITIVE — the first W-REVIEW
   caught `BREACHPOINT` in caps and the host's `BN` abbreviation sailing through a
   case-sensitive gate that then reported a false PASS):
   `grep -rniE "breachpoint|\bBN([A-Z_[:space:]-]|$)" Source/AIBot/ --include=*.h --include=*.cpp --include=*.cs`
   returns nothing. (Widened twice now: caps and `BNFoo` first, then word-alone `BN ` and
   `BN_Drop` shapes — W-REVIEW P2 C1 caught the second gap the first fix left.)
2. **Interfaces are the only door.** `Interfaces/` owns the contracts; the game implements
   them in its own adapter folder. The bot presses verbs and asks questions — it never
   activates, applies, or writes anything on the avatar.
3. **Server-only.** This module DECLARES no replicated property and no RPC — check:
   `grep -rn "Replicated\|DOREPLIFETIME\|NetSerialize" Source/AIBot/ --include=*.h --include=*.cpp` returns nothing.
   The one deliberate replication CAUSE is `bWantsPlayerState` (a bot IS a player, so its
   engine-replicated PlayerState carries name/score exactly as a human's does). The brain
   runs only on the authority: the controller refuses to operate without `HasAuthority()`,
   and `AIBBotManager` is the only sanctioned spawner.
4. **The brain is worldless.** `Brain/` and `Skills/` take `FAIBFacts` in, return decisions
   out. Check: `grep -rn "UWorld\|AActor\|GetWorld" Source/AIBot/Brain/ Source/AIBot/Skills/`
   returns nothing. World-touching code lives only in `Core/` (the facts BUILDER included —
   it touches the world, so it lives outside the worldless folders), `Perception/`,
   `Execution/`, `Team/`.
   *Glossary:* "worldless" is law 4, folder-scoped. "Headless-testable" is the weaker
   property `Perception/` has — time as a parameter, actors as opaque weak handles — which
   is why its specs run without an engine world. One word per meaning, on purpose.
5. **FAIRPLAY.md binds every file.** F1–F8, testable, cited by name in findings.

**The one per-frame surface (dated exception, 26 Aug 2026 — W-REVIEW P3).** The
CONTROLLER is tickless: thinking is the timer's, reacting is the clock's, and no phase
may enable its actor tick. But the executor's `UStateTreeAIComponent` ticks — that is
the only way a StateTree runs tasks, and steering, burst timing, and arrival checks
consume its DeltaTime. This is the module's single sanctioned per-frame surface: any
SECOND ticking component, or gameplay decision-making moved into that tick (deciding is
the brain's, at think cadence), is a finding. Recorded because "tickless by law" was
being cited while every task ticked — a true claim about the actor, a false one about
the module.

## Layout (phases fill it in; headers carry real contracts from day one)

    Core/        types, tags, AAIBBotController (the hand), AIBBotManager (lifecycle),
                 AIBFactsBuilder (world-touching, so it lives HERE, not in Brain/)
    Interfaces/  IAIBAvatarInterface · IAIBWorldQuery · IAIBAmbitionProvider
    Perception/  AIBSensorium · AIBReactionClock · AIBTargetMemory      (F1/F2/F3/F5)
    Brain/       considerations · ambitions · AIBAmbitionEngine ·
                 AIBConfidenceModel                                     (worldless)
    Skills/      AIBSkillProfile + Movement/Aim/Grenade/Melee policies  (worldless, F4)
    Execution/   IAIBExecutor · StateTree executor + tasks · AIBTreeAuthoring (editor-only)
    Team/        AIBTeamCoordinator (claims board)
    Data/        row structs · AIBTuningData (C++ defaults are truth; assets mirror them)
    Debug/       gameplay debugger category
    Tests/       AIBot.Sim.* specs

Crew: aib-builder writes here (only here); aib-critic attacks; aib-editor owns
`Content/AIBot/` + `Tools/aib/`; aib-verifier counts. Wave dispatch per `docs/AIBOT-WAVES.md`.

## The Phase-10 extraction delta (recorded so the "folder move" stays honest — W-REVIEW M6)

Moving to `Plugins/AIBot/` is a folder move PLUS exactly this, no archaeology:
1. Delete the `AIBot` entry from `Breachpoint.uproject` `Modules[]` (plugins declare
   modules in the `.uplugin`).
2. Remove `"AIBot"` from all three `Source/*.Target.cs` `ExtraModuleNames`.
3. Author `AIBot.uplugin` with the module block AND `Plugins[]` entries for `StateTree`
   and `GameplayStateTree` — the engine plugins Build.cs depends on, currently satisfied
   by the host project's own plugin list.
4. `Config=Game` on config classes becomes the plugin's own hierarchy if per-plugin ini
   shipping is wanted.
5. **The content mount (added 26 Aug 2026 — W-REVIEW P3 found the delta short by one
   whole category).** `/Game/AIBot/…` is the HOST project's Content mount; a plugin's
   own content mounts at `/AIBot/…`. Moving `Content/AIBot/` into the plugin means
   updating the two asset-path literals in `Execution/AIBTreeAuthoring.cpp`, their
   echoes in that file's header comment, and the consumer's
   `[/Script/AIBot.AIBBotController] BotStateTree=` ini value — or every bot stands
   still on a null soft path while the authoring happily rebuilds a second, unloaded
   tree at the old mount. `Tools/aib/70_aib_assets.py` carries the same paths plus
   host-project tooling names and must move or be rewritten with the plugin.
`PublicIncludePaths.Add(ModuleDirectory)` in Build.cs is move-invariant on purpose —
and as of 26 Aug the code actually says that (P2 C5 caught this line certifying a form
the Build.cs did not yet use).

Phase 6 obligation, recorded (P2 finding B): possession must `ClearAmbitions()` and
re-register core + current mode, or a mode change leaves the previous mode's ambitions
registered forever — a CTF want scoring inside Slayer.
