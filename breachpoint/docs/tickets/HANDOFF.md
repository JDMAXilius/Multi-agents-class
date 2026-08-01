# HANDOFF — session of 31 Jul → 1 Aug 2026

Read this first, then `/tickets list`. Written at session close so the next session does not
re-derive it. **Nothing is claimed** — and `.claude/active-packet.json` does **not exist**
(this line previously said it "was left pointing at a multi-ticket parallel pod"; it doesn't).
That matters more than it reads: `guard_laws.py` confines writes only *while a claim file
exists*, so with no claim, law 5 is off. Write it on pickup, per the tickets skill.

## Where the project actually is

**The module builds, all ten disciplines, and PIE runs OUR code.** `Source/Breachpoint/` has
`Core/ Input/ Character/ Match/ AbilitySystem/ Weapons/ AI/ Online/ Telemetry/ UI/ Data/`
populated. `GlobalDefaultGameMode` points at **`/Game/Core/GM_BR.GM_BR_C`**
(`Config/DefaultEngine.ini:30`) — a Blueprint child of `ABRGameMode`, so our game mode is what
spawns, not the template's. *Corrected 1 Aug: this line previously read "points at
`BRGameMode`", which reads as the C++ class directly and would mislead anyone grepping the ini.
The rename to `BP_BRGameMode` (R26 condition 5) has still not been run, so the ini and the
assets agree on the OLD names and PIE is unaffected.*

| Ticket | State |
|---|---|
| **BP01** skeleton + input | steps 1–4 landed and compile. **Step 3b (input assets) NOT run** — the generator exists, see below. Step 5 (verifier + PIE) not done. Zero Done-when boxes checked. |
| **BP00** ladder | step 1 landed (`Tools/run-ubt/specs/gauntlet/reimport`). Steps 2–3 blocked. |
| **BP02** GAS core | steps 1–4 landed. The full damage pipeline exists: ASC, `BRAttributeSet`, `BRDamageExecCalc`, **seven** GE **C++ classes** (`AbilitySystem/Effects/BRGameplayEffects.h` — RecentDamage, Damage, Regen, Cooldown, InitStats, Death, ShieldsBroken), `BRGA_Sprint`, `CT_Combat.csv`. *Was written as "six" here and in BP15's Done-when; corrected 1 Aug — BP15 step 1 parses counts, so an off-by-one is a false finding waiting to happen.* |
| **BP03** weapons | step 1 landed. **Step 2 (the fire path) was IN FLIGHT and was stopped — restart it.** |
| **BP04** match frame | landed — phases, kill attribution, one replicated end-time. |
| **BP07** arena | generator landed; **manifest v3 lands and validates PASS**. The `.umap` has never been built. |
| **BP08** bots | brain landed; `DT_BotTuning` / `DT_BotAmbitions` landed, so `LoadBotTables` can now succeed. |
| **BP10** UI | C++ layer + ViewModels landed. No WBP assets exist. |
| **BP11** online | `IBRServerLifecycle`, sessions, telemetry, host-quit decided. Steam untested. |
| **BP13** data crew | **steps 1–5 DONE** (crew ran live 29 Jul; `DT_Weapons.csv` + `arena_manifest.json` landed with verifier PASS). Step 6 — the reimport — is the remainder. *Was missing from this table entirely; added 1 Aug.* |
| **BP14** engine bridge | **step 1 DONE 1 Aug** — crew ported to `Tools/data-crew/`, replay exits 0 from the game repo. Steps 2–5 (code job type, real ladder, deliberate failures) untouched and need the engine. |
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
nodes · **R27** the middle bot tier is `Marine` · **R28** tiers differ only on readable levers ·
**R29** (cut 1 Aug) one editor, one driver, and an editor session must not overlap a build —
three documents were citing this as "R21," which says nothing about editors.

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

## Added 1 Aug (cloud session, Context A — files only)

- **UE MCP research landed:** `Tools/ue_mcp/RESEARCH.md`. Setup is `Edit > Plugins > Unreal
  MCP` → restart → `Editor Preferences > Model Context Protocol > Auto Start Server`, then the
  console command `ModelContextProtocol.GenerateClientConfig ClaudeCode` writes `.mcp.json`.
  Endpoint `http://127.0.0.1:8000/mcp`. Our `.uproject` already enables both plugins; **no
  `.mcp.json` is committed yet** — generate it on the workstation, then decide whether it is
  committed or gitignored (it points at loopback, so committing it is harmless and saves the
  next machine a step).
- **The framing correction:** the MCP is a *server exposing tools*, not a second agent. It is
  the same terminal session with the editor open. No prompt-handoff format needed.
- **Execution contexts are now written down** — `BREACHPOINT-AUTHORING-MATRIX.md` §5, four
  contexts (A cloud / B terminal-headless / C terminal+editor / D human), with the routing rule
  and the `requires:` line tickets should carry. Backfilling `requires:` onto BP00–BP16's
  Kickoff blocks is unstarted.
- **R26 rename tool landed** (`Tools/rename_r26/`) — see the founder-Blueprints note above.

## Honesty ladder position

**Rung 1 only, and not fully.** Three targets compiled with R19 evidence earlier in the session,
but the tree has changed a lot since and no clean owned pass covers the current HEAD. No PIE, no
multiplayer, no dedicated-server, no packaged claim has been made by anyone.

*Corrected 1 Aug:* this section previously claimed **"zero Done-when boxes are checked anywhere
on the board, deliberately."** That was false when written — `TICKET_BP13_DATA_CREW.md` carries
four checked boxes from the 29 Jul crew run, and BP13 was also missing from the state table
above. The boxes are legitimate (their evidence is `assignments/03-agent-crew/output/` in the
planning repo, which is where step 1 says the crew runs), but *"zero, deliberately"* is exactly
the kind of tidy claim that stops the next session from checking. **One more was checked on
1 Aug: BP14's step-1 box**, and it is not an engine claim — it is a Python script exiting 0.
(BP13's `DamageDelivery` box stays unchecked even though the column exists at
`BRDataRows.h:120`, because the box's second half — "reimport is clean after" — needs the
editor. Half a box is not a box.)

The honest statement is: **no box anywhere on this board rests on a ladder rung above 1**, and
rung 1 itself has no clean owned pass at current HEAD.
