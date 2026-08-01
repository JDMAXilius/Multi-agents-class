# HANDOFF — session of 31 Jul → 1 Aug 2026

Read this first, then `/tickets list`. Written at session close so the next session does not
re-derive it. **Nothing is claimed** — `.claude/active-packet.json` was left pointing at a
multi-ticket parallel pod and should be rewritten on the next pickup.

## Where the project actually is

**The module builds, all ten disciplines, and PIE runs OUR code.** `Source/Breachpoint/` has
`Core/ Input/ Character/ Match/ AbilitySystem/ Weapons/ AI/ Online/ Telemetry/ UI/ Data/`
populated. `GlobalDefaultGameMode` points at `BRGameMode`, so the template pawn is no longer
what spawns.

| Ticket | State |
|---|---|
| **BP01** skeleton + input | steps 1–4 landed and compile. **Step 3b (input assets) NOT run** — the generator exists, see below. Step 5 (verifier + PIE) not done. Zero Done-when boxes checked. |
| **BP00** ladder | step 1 landed (`Tools/run-ubt/specs/gauntlet/reimport`). Steps 2–3 blocked. |
| **BP02** GAS core | steps 1–4 landed. The full damage pipeline exists: ASC, `BRAttributeSet`, `BRDamageExecCalc`, six GE **C++ classes**, `BRGA_Sprint`, `CT_Combat.csv`. |
| **BP03** weapons | step 1 landed. **Step 2 (the fire path) was IN FLIGHT and was stopped — restart it.** |
| **BP04** match frame | landed — phases, kill attribution, one replicated end-time. |
| **BP07** arena | generator landed; **manifest v3 lands and validates PASS**. The `.umap` has never been built. |
| **BP08** bots | brain landed; `DT_BotTuning` / `DT_BotAmbitions` landed, so `LoadBotTables` can now succeed. |
| **BP10** UI | C++ layer + ViewModels landed. No WBP assets exist. |
| **BP11** online | `IBRServerLifecycle`, sessions, telemetry, host-quit decided. Steam untested. |
| **BP16** UE MCP | **NEW TICKET, cut this session.** See below. |

## The three things to do first tomorrow

1. **Run the input generator.** `Tools\gen_input\build-input.ps1` (`-PlanOnly` first). Without it
   `DA_InputConfig` does not exist, so `BRPlayerController` binds nothing and **you cannot move
   in PIE.** It creates the 8 missing `IA_*`, the config asset, and 10 `IMC_Default` mappings.
   Needs a free editor.
2. **Restart BP03 step 2** — the fire path. Its packet also has to add an `AbilitySet` column to
   `DT_Weapons.csv` (equip currently has nothing to grant) and declare the three
   `GameplayCue.Weapon.*.Fire` tags the CSV already names.
3. **Import the CSVs to DataTable/CurveTable assets.** Seven CSVs exist in `Content/Data/` and
   **none has been imported**, so no number is read at runtime yet. `Tools/reimport-tables.ps1`
   is written but its commandlet invocation has never completed successfully here.

## Blocked, with the reason

- **Rung 4 / Gauntlet** — the engine's own C# libs fail NuGet/SDK resolution through MSBuild on
  this machine (`Microsoft.Extensions`, `MongoDB`, `Polly`, `OpenTracing` all unresolved).
  Workstation repair, not a packet. Likely `Setup.bat` or a `dotnet restore` against the engine
  solution.
- **Rung 2 / specs** — `Source/Breachpoint/Tests/` holds only a `.gitkeep`. R25 settled the
  ownership question (one spec file per packet, exact-path grant) but nobody has written one.
  **Every rule landed tonight is unpinned.**
- **`ControlRocket` chain 1 is unreachable** — needs a `holding_power_weapon_norm` fact in
  `FBRBotFacts` (ai-builder) or a new ambition enumerator. Filed in BP08's Log.
- **R4's 90 s rocket timer has no table home** — `ApplyMatchRules` takes six params and
  `FBRMatchRulesRow` deliberately refuses a seventh. BP04 must widen the seam or the rocket never
  spawns, which also keeps `PowerWeaponAvailable` false and `ControlRocket` dead.
- **`Tools/audit_blueprints/`** — written but its agent was stopped mid-packet. **Unreviewed and
  unrun; do not trust it.** Until it works, R26 is enforced by goodwill and says so.

## Decisions made this session (do not re-litigate — `docs/DESIGN-RULINGS.md`)

**R19** build claims need a timestamp proof · **R20** `-Rebuild` isn't safely retryable ·
**R21** one build agent at a time; `TaskStop` orphans processes · **R22** `Damage.*` is flat ·
**R23** `Ability.*`/`GameplayCue.*` are open families · **R24** log channels per discipline ·
**R25** one spec file per packet · **R26** BP children as default-value containers, zero graph
nodes · **R27** the middle bot tier is `Marine` · **R28** tiers differ only on readable levers.

## Two things the founder should know

- **The UE MCP (BP16).** `ModelContextProtocol` and `MCPClientToolset` are enabled in the
  `.uproject`, but no Unreal MCP server was connected to this session, so nothing could call it.
  It is hosted **inside a running editor**. BP16's real subject is not transport but
  **jurisdiction**: `guard_laws.py` gates `Edit`/`Write` by `file_path`, and an MCP tool call has
  neither — so every mechanical protection this project has is blind to it. Decide the law-7
  boundary before an agent lands its first `.uasset` that way.
- **The founder's Blueprints don't match R26's naming — the fix is written, not run.**
  `GM_BR` / `GS_BR` / `PC_BR` / `PS_BR` / `BP_BRcharacter` are landed; R26 condition 5
  specifies `BP_<CppClassWithoutPrefix>`. **Decision taken: rename, don't amend** (the
  ruling's naming rule is what makes an off-name asset findable at a glance).
  `Tools/rename_r26/rename-r26.ps1` does it — **needs a free editor and has never been
  run.** `git mv` is NOT an alternative: a `.uasset` stores its package name inside the
  file, so renaming on disk leaves it stale and the editor refuses to load it; only
  `EditorAssetLibrary.rename_asset` rewrites the package and fixes up referencers.
  - `-PlanOnly` prints the five renames and the ini edit with no editor.
  - It repoints `Config/DefaultEngine.ini`'s `GlobalDefaultGameMode`
    (`GM_BR.GM_BR_C` → `BP_BRGameMode.BP_BRGameMode_C`) **only after all five renames
    succeed**, so the repo is never in a state where the ini names a missing asset. Until
    it runs, ini and assets agree on the OLD names and PIE is unaffected.
  - Idempotent; R21 editor guard; law-7 lfs-lock check. `git lfs unlock` the five paths
    afterwards, then re-run the audit — condition 5 should be clean.

## Honesty ladder position

**Rung 1 only, and not fully.** Three targets compiled with R19 evidence earlier in the session,
but the tree has changed a lot since and no clean owned pass covers the current HEAD. No PIE, no
multiplayer, no dedicated-server, no packaged claim has been made by anyone. **Zero Done-when
boxes are checked anywhere on the board**, deliberately.
