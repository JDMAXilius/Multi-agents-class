# AIBot — module architecture (the short law; the long form is docs/AIBOT-ROADMAP.md)

Self-contained bot framework. Halo Infinite's multiplayer-bot architecture, 1:1 where
published; our own designs, flagged as ours, where 343 published nothing (confidence
internals, curve shapes, every number). Plugin-shaped: extraction to `Plugins/AIBot/` is
Phase 10 and must stay a folder move.

## The five laws of this module

1. **Boundary.** Engine dependencies only — `GameplayAbilities` is deliberately absent from
   Build.cs, so GAS purity is a LINKER guarantee. Zero includes of any game module. Check:
   `grep -rn "Breachpoint" Source/AIBot/ --include=*.h --include=*.cpp` returns nothing.
2. **Interfaces are the only door.** `Interfaces/` owns the contracts; the game implements
   them in its own adapter folder. The bot presses verbs and asks questions — it never
   activates, applies, or writes anything on the avatar.
3. **Server-only.** Nothing replicates. A bot drives its pawn through the player input
   path, so at the netcode layer a bot IS a player.
4. **The brain is worldless.** `Brain/` and `Skills/` take `FAIBFacts` in, return decisions
   out — no UWorld, no AActor, no components. That is what makes `AIBot.Sim.*` possible.
   World-touching code lives only in `Core/`, `Perception/`, `Execution/`, `Team/`.
5. **FAIRPLAY.md binds every file.** F1–F7, testable, cited by name in findings.

## Layout (phases fill it in; headers carry real contracts from day one)

    Core/        types, tags, AAIBBotController (the hand), AIBBotManager (lifecycle)
    Interfaces/  IAIBAvatarInterface · IAIBWorldQuery · IAIBAmbitionProvider
    Perception/  AIBSensorium · AIBReactionClock · AIBTargetMemory      (F1/F2/F3/F5)
    Brain/       AIBFactsBuilder · considerations · ambitions · AIBAmbitionEngine ·
                 AIBConfidenceModel                                     (worldless)
    Skills/      AIBSkillProfile + Movement/Aim/Grenade/Melee policies  (worldless, F4)
    Execution/   IAIBExecutor · StateTree executor + tasks · AIBTreeAuthoring (editor-only)
    Team/        AIBTeamCoordinator (claims board)
    Data/        row structs · AIBTuningData (C++ defaults are truth; assets mirror them)
    Debug/       gameplay debugger category
    Tests/       AIBot.Sim.* specs

Crew: aib-builder writes here (only here); aib-critic attacks; aib-editor owns
`Content/AIBot/` + `Tools/aib/`; aib-verifier counts. Wave dispatch per `docs/AIBOT-WAVES.md`.
