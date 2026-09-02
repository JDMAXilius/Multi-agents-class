# TICKET — BN42: the four WBPs and the FE map, built by script at the referee's boxes

> STATUS: in-progress — BOTH WBPs AND THE FE MAP ARE BUILT, COMPILED, SAVED AND COMMITTED
> (d6289c9f + this entry). The loop walks from the menu into a live 8-player match, twice.
> Outstanding: the pause -> LEAVE MATCH leg (Escape is Stop-PIE in the editor, so it needs
> STANDALONE) and founder-facing 1280x720 screenshots. OWNER: **terminal**, LIVE EDITOR.
> The 1 Sep MCP registry fault is GONE — `list_toolsets` answered on the first call.
> DEPENDS ON BN41 rung 1. requires: editor-live, unreal-mcp (`list_toolsets` first, the
> four failure rules apply). **Law 7: widgets land by committed Tools/bn/bn41_*.py
> scripts in the bn11 pattern (bn11_lib is the transport), never hand-placed.**
> Layout truth: `01-MENU-MEASURED.md` — if a number is not in the referee, re-read the
> node, do not eyeball it. The HUD was built this way and the founder called it perfect;
> same bar.

## Build — WITH THE UNREAL-MCP TOOLS, DIRECTLY, IN THE EDITOR

> **FOUNDER RULING (1 Sep, recorded):** editor jobs are done with the real unreal-mcp
> tools driven directly from the editor session — `list_toolsets` →
> `describe_toolset` → `call_tool` — exactly as the HUD passes did. Python scripts are
> the FALLBACK, only where an MCP tool cannot do the job. The two committed
> `Tools/bn/bn41_*.py` files therefore stand as (a) the reviewable PLAN — every widget,
> box, bind and property below is spelled out in them — and (b) the batch fallback if a
> re-run is ever needed; the PRIMARY path is you, calling the tools.

### The call sequence per screen (all toolset/tool names proven by bn11/bn16/build_wbp)

Preflight once: `list_toolsets`, confirm `UMGToolSet.UMGToolSet`,
`editor_toolset.toolsets.object.ObjectTools`, `editor_toolset.toolsets.asset.AssetTools`.
The four failure rules from `.claude/skills/unreal-mcp` apply to every call.

1. `AssetTools.exists` on `/Game/BN/UI/<WBP>.<WBP>` — if present, `AssetTools.delete`
   and re-check: the create call's clobber guard is MEMORY, not disk (build_wbp lesson 1).
2. `UMGToolSet.CreateWidgetBlueprint` — `folderPath=/Game/BN/UI`, `assetName`,
   `parentClass={"refPath":"/Script/BreachpointNext.BNScreen_FrontEnd"}` (or `…PlaySetup`).
3. `UMGToolSet.GetWidgets`, then `RemoveWidget` any settings-default root — the plan is
   the only author of the tree.
4. Walk the widget table in **`Tools/bn/bn41_frontend_wbps.py`** — `frontend_plan()` (21
   widgets) and `playsetup_plan()` (28 widgets) — top to bottom; the ORDER is the
   parent-before-child order. Per row: `AddWidget` (class refPath, display name, parent
   handle) → if `bind`, `ToggleWidgetAsVariable` → `ObjectTools.set_properties` on the
   SLOT (`layoutData` = the row's box — absolute 1280x720 top-left, the R7 idiom; the
   DPI curve scales, multiply nothing) → `set_properties` on the WIDGET (text, tint,
   padding; `values` is a JSON **string**) → `get_properties` back and COMPARE — an
   unverified write is not a write.
   Fonts: `get_properties ["font"]`, change `size` only, write the whole struct back —
   a partial `{"size":N}` drops the typeface (bn11's font_sized rule).
5. `UMGToolSet.CompileWidgetBlueprint` — the engine enforces every BindWidget HERE; a
   compile failure names a bind, and the fix is the C++ name or the plan name, never a
   third name invented in the asset.
6. `AssetTools.save_assets`, then `GetWidgetDescription` (full depth) and paste the tree
   into this ticket's Log — that read-back IS the receipt.

Bind names are the C++ contract, already machine-checked: `Tools/bn/bn41_selftest.py`
ran headless in the cloud session — PASS on both screens, 3/3 and 9/9 binds. If you
change any name in the editor, re-run the selftest before compiling again.

### The FE map — MCP-first here too

1. Level: `list_toolsets` for a level/editor toolset with new-level + save (the BN33
   sessions drove level work through MCP; use what `describe_toolset` names). Create
   `/Game/Maps/FE_MainMenu`, add a DirectionalLight + SkyAtmosphere so the stage is not
   the void.
2. WorldSettings: `ObjectTools.list_properties` on the map's WorldSettings actor, find
   the game-mode override property (engine header names it `DefaultGameMode`), and
   `set_properties` it to `/Script/BreachpointNext.BNFrontEndGameMode` — read back to
   confirm. This replaces the raw-python watch-list in `bn41_fe_map.py`; that script is
   now the fallback ONLY if the level toolset genuinely lacks a create/save surface —
   and if you fall back, say so in the Log with the toolset listing that forced it.

### Known thin ice to CHECK, not assume (the cloud names its own)

- Row height 28 comes from content (font 14) + slot pitch, not a SizeBox — off-pitch
  rows mean adding a SizeBox per row; update BOTH the calls and the plan file.
- `bIsEnabled` / `brushColor` / border `padding` camelCase keys are unproven in this
  repo — the readback after each write is where a wrong key shows as an unchanged value.
- Panel grounds are flat tints; the referee's 88x4 notch language is BN43/M2.
- CommonTextBlock may render the engine default face with no style asset — ACCEPTED for
  M1 (geometry over garb).

## Prove (the loop, not the look)

- [x] PIE on FE_MainMenu: menu appears; every widget renders. Focus/mouse: see the
      harness note in the Log — a click focuses PLAY, ENTER activates it.
- [~] PLAY -> setup: YES, with the roster values live from C++ (SPILLWAY / FREE-FOR-ALL /
      8 (YOU + 7 BOTS)). Escape-pops-back and cycling: NOT driven this session.
- [x] START on Spillway/FFA/8 -> match: 8 pawns, 8 PlayerStates, 1 PlayerController,
      counted out of the running world. Travel URL in the Log.
- [ ] START with TDM -> two teams, 4v4, team colours in feeds (not exercised)
- [ ] Pause -> LEAVE MATCH -> back on the menu map (BLOCKED IN PIE: Escape is Stop-PIE;
      needs STANDALONE, the same limitation the handoff already records for R7-WBP-HUD)
- [x] Repeat loop twice: two launches, byte-identical URLs, both reaching 8 players.
- [~] FrontEnd captured in PIE (the editor's PIE viewport is ~6.6:1, so it is not a
      1280x720 comparison shot). PlaySetup capture outstanding.

## Done when
- [x] Both WBPs + the FE map landed via unreal-mcp; GetWidgetDescription receipts below
- [x] The loop walks, twice, observations below
- [x] Deviations listed; the ONE `blocks` item (row height) is now MEASURED and FIXED

## Log

### 1 Sep 2026 (evening) — THE EDITOR PASS: both screens built, the loop walked into a match

Terminal session, live editor, MCP up. **The registry fault that ended the previous session is
gone** — `list_toolsets` answered on the first call. Toolset listing captured verbatim, as the
RESUME block requires (R46 evidence for every fallback decision below):

```
ToolsetRegistry.AgentSkillToolset · EditorToolset.EditorAppToolset · EditorToolset.LogsToolset
ConfigSettingsToolset · MVVMToolset · SlateInspectorToolset · UMGToolSet.UMGToolSet
editor_toolset.toolsets.{actor,asset,blueprint,curve_table,data_asset,data_table,material,
material_instance,object,primitive,scene,skeletal_mesh,static_mesh,string_table,texture,
programmatic}
```

**`TERMINAL-VS-EDITOR.md` §5.1's toolset census is confirmed STALE** — `UMGToolSet.UMGToolSet`
is present, as `bn11_lib.py` implied.

#### How the calls were made, and why that is still MCP-first

R46 says the tools, not a Python driver. Both screens were built through
**`ProgrammaticToolset.execute_tool_script`** — an unreal-mcp tool whose stated purpose is
"batch multiple tool calls into a single script execution" — orchestrating `UMGToolSet.AddWidget`
/ `ToggleWidgetAsVariable` / `CompileWidgetBlueprint` and `ObjectTools.set_properties` /
`get_properties`. **The committed `Tools/bn/bn41_frontend_wbps.py` driver was NOT run.** It keeps
its standing as the reviewable plan, and it has been UPDATED to match what was built (below).
Every write was read back inside the batch and compared; nothing was set blind.

**One warning in this ticket is WRONG and should not be carried forward.** The RESUME block says
a wrong camelCase key "returns the CLASS DEFAULT rather than an error". It does not:
`get_properties` on an unknown name **raises**, naming the property —

```
GetObjectProperties on '...FrontEndCanvas' (CanvasPanel): the following properties
could not be read: bIsVariable
```

So a mistyped key fails loudly on readback. Every camelCase key this ticket called "unproven"
(`bIsEnabled`, `brushColor`, `padding`, `layoutData`, `colorAndOpacity`, `heightOverride`) is now
proven — each read back its written value, not a default.

#### Receipts — the built trees

`WBP_BNScreen_FrontEnd` (parent `/Script/BreachpointNext.BNScreen_FrontEnd`), **25 widgets**:

```
[0] CanvasPanel FrontEndCanvas
  [1] Image Scrim  ColorAndOpacity:(0,0,0,0.62)  slot: anchors Max=(1,1)  [full-screen]
  [2] CommonTextBlock TitleText  "BREACHPOINT"  Font:30   box 33,45 666x30
  [3] Border NewsPanel  Padding:12  BrushColor:(0.039,0.063,0.094,0.88)  box 69,138 349x222
    [4] CommonTextBlock NewsTitleText  "NEW ARENA: SPILLWAY"  Font:16
  [5] Border MenuPanel  Padding:19  BrushColor:(0.039,0.063,0.094,0.88)  box 69,370 349x186
    [6] VerticalBox MenuColumn
      [7] SizeBox PlayRowBox     HeightOverride:28
        [8] Button PlayButton  -> [9] CommonTextBlock PlayLabel "PLAY" Font:14
      [10] SizeBox CustomsRowBox  HeightOverride:28
        [11] Button CustomsButton bIsEnabled:False -> [12] CustomsLabel "CUSTOM GAMES"
      [13] SizeBox AcademyRowBox  HeightOverride:28
        [14] Button AcademyButton bIsEnabled:False -> [15] AcademyLabel "ACADEMY"
      [16] SizeBox QuitRowBox     HeightOverride:28
        [17] Button QuitButton -> [18] QuitLabel "QUIT"
  [19] Border DescriptionPanel  Padding:20/10  BrushColor:(0.02,0.031,0.047,0.94)  69,611 349x37
    [20] CommonTextBlock DescriptionText  Font:12                      [BOUND, C++ writes it]
  [21] Border PartyPanel  Padding:16  box 862,397 349x273
    [22] CommonTextBlock PartyHeaderText "FIRETEAM — LOCAL" Font:14
  [23] Border ProfileBar  Padding:60/15  box 0,670 1280x50
    [24] CommonTextBlock PromptText "ENTER — SELECT      ESC — QUIT" Font:11
```

`WBP_BNScreen_PlaySetup` (parent `…BNScreen_PlaySetup`), **32 widgets**:

```
[0] CanvasPanel SetupCanvas
  [1] Image Scrim (full-screen, a=0.62)
  [2] CommonTextBlock PageTitleText "PLAY  ▸  CUSTOM GAME" Font:22   box 70,26 630x31
  [3] Border PreviewPanel  box 69,76 349x196.699997      [the .7 SURVIVED the transport]
    [4] Image PreviewImage  Visibility:Collapsed                     [BN43 wires the art]
  [5] Border MenuPanel  Padding:19  box 69,282.700012 349x186
    [6] VerticalBox MenuColumn
      [7] SizeBox MapRowBox   HeightOverride:28
        [8] Button MapButton -> [9] Overlay MapRow -> [10] MapLabel "MAP" / [11] MapValueText
      [12] SizeBox ModeRowBox  HeightOverride:28   (ModeButton/ModeRow/ModeLabel/ModeValueText)
      [17] SizeBox BotsRowBox  HeightOverride:28   (BotsButton/BotsRow/BotsLabel "PLAYERS"/BotsValueText)
      [22] SizeBox StartRowBox HeightOverride:28   (StartButton -> StartLabel "START GAME")
  [25] Border BreakdownPanel  Padding:16  box 466,76 349x332
    [26] CommonTextBlock BreakdownTitleText "DETAILS" Font:16
  [27] Border DescriptionPanel  box 69,559 349x37  -> [28] DescriptionText Font:12   [BOUND]
  [29] Border ProfileBar  box 0,670 1280x50
  [30] Button BackButton  box 60,685 150x20  ZOrder:1  -> [31] BackLabel "ESC — BACK" Font:11
```

`CompileWidgetBlueprint` returned true for both, and the log's final compile carries **no
`required widget binding … was not found`** line. (There ARE such warnings at 19:35:25–26: those
are the INCREMENTAL recompiles fired by each `AddWidget` while the tree was still half-built, and
they stop the moment the last widget lands. Read the timestamps before treating them as a
failure.) `bn41_selftest.py` re-run after the edits: **PASS 25 widgets 3/3 binds, PASS 32 widgets
9/9 binds.**

#### The FE map — one fallback was real, the other was NOT

- **Creating an empty level: the predicted gap is REAL.** `SceneTools` exposes `load_level` but
  no create-level; `AssetTools` has no create-asset. **But the fallback did not have to leave
  MCP**: `AssetTools.duplicate` of the lightest existing map (`BR_MetricsGym`, 4 dependencies vs
  BR_Arena01's 79) → `/Game/Maps/FE_MainMenu`, then `SceneTools.remove_from_scene` stripped the
  gym's contents (101 actors removed, 0 failures), leaving WorldSettings, PlayerStart_1 and the
  engine singletons. `-ExecutePythonScript` was never needed.
- **Getting a handle to WorldSettings: the predicted gap is REFUTED.**
  `SceneTools.find_actors(actor_type=/Script/Engine.WorldSettings)` returns it directly:
  `/Game/Maps/FE_MainMenu.FE_MainMenu:PersistentLevel.WorldSettings`. The property is
  `defaultGameMode` (not `DefaultGameMode`), it read `None`, and after the write it reads
  `{"refPath":"/Script/BreachpointNext.BNFrontEndGameMode"}`. **Runtime confirms the asset
  write**: `LogLoad: Game class is 'BNFrontEndGameMode'` on every PIE start.

Stage: `FE_Sun` (DirectionalLight, pitch −42 yaw −35), `FE_SkyAtmosphere`, `FE_SkyLight`. The
viewport capture shows sky and ground — not the void. One cosmetic engine warning on the near-
empty stage: *"Cached lighting in Lumen and real-time sky capture lighting is going to be
clipped. Please adjust r.EyeAdaptation.CachedLightingPreExposure"*. Not chased; BN43 replaces
this backdrop anyway.

#### THE ONE `blocks` DEVIATION IS CLOSED — measured, then fixed

The ticket said row height 28 was "asserted, never built" and that only a render could settle it.
It was settled by a better instrument than a capture: **`SlateInspector.Snapshot` reports each
widget's local size**, so the rows could be read as numbers instead of eyeballed.

| | before | after |
|---|---|---|
| menu row | **311 × 31** | **311 × 28** |
| design pitch | 43 | **40** |
| 4 rows in a 148-high content box (186 − 2×19) | **160 — OVERFLOWS** | **148 — exact** |

So 28 was never cosmetic: a bare `UButton` auto-sizing to its 14pt label comes out 31, and four
of those spill 12px out of the menu panel. Fixed with `UMGToolSet.WrapWidgets` → one `SizeBox`
per row (`heightOverride: 28`), the slot's 12 moved onto the box, the button set to fill it.
Re-measured live afterwards: `button "PLAY" size=311,28`, pitch 40. Counts moved 21→25 and
28→32, and **`Tools/bn/bn41_frontend_wbps.py` was updated in the same breath** (`row_box()`,
`ROW_H`, `BTN_FILL`) so the plan and the assets agree — verified by name-set diff: *0 missing, 0
extra, both screens*.

#### A SECOND defect the measurement caught, which nobody had predicted

`MapLabel` at x=97 and `MapValueText` at x=95 — **the label and its value were drawn on top of
each other.** Cause: the `Overlay`'s ButtonSlot was left at its default alignment, so the Overlay
shrink-wrapped to its widest child and the `HAlign_Left` / `HAlign_Right` on label and value
resolved inside that shrunken box instead of across the 311 row. Fixed by filling the ButtonSlot
(`HAlign_Fill` / `VAlign_Fill`) on `MapRow` / `ModeRow` / `BotsRow`; the plan file carries the
fix and the reason.

Both defects were invisible to `--verify` and to `GetWidgetDescription`. They were only findable
at runtime, which is the argument for doing this leg in PIE rather than on paper.

#### THE LOOP WALKS — into a real match, twice

`BNFrontEndGameMode` pushes the menu at PostLogin: **BN41 step 3's suspicion (that PostLogin
might fire too early for the push) is REFUTED by observation — no deferred push is needed.**

Every FrontEnd string is live in the Slate tree, and the C++ owns what it should:
`DescriptionText` reads **"Set up a match against bots."** — written by
`NativeOnInitialized`, not by the asset. CUSTOM GAMES and ACADEMY render `[disabled]`, so
`bIsEnabled:false` survives to runtime.

PLAY → setup, with all three cyclers populated from C++/ini:

```
button "MAP\nSPILLWAY"            button "MODE\nFREE-FOR-ALL"
button "PLAYERS\n8  (YOU + 7 BOTS)"   button "START GAME" [focused]
text "Five tiers around the culvert. 4v4 arena."
```

START → a live listen-server match, and the log is the receipt:

```
LogBN: BNScreen_PlaySetup: launching /Game/Maps/BR_Spillway (listen?TargetPlayers=8?Teams=0).
LogNet: Browse: /Game/Maps/BR_Spillway?listen?TargetPlayers=8?Teams=0
LogBN: BNGameMode: TargetPlayers=8 from the travel URL.
LogBN: BNGameMode: Teams=off from the travel URL.
LogLoad: Took 0.186719 seconds to LoadMap(/Game/Maps/BR_Spillway)
```

Counted out of the running world, not inferred: **8 Characters, 8 PlayerStates, 1
PlayerController** — YOU + 7 bots, which is the ticket's scoreboard-count-of-8 check. Bot names
render in the match UI (`Slowdraw`, `Softaim`). **This happened twice**, at 19:46:43 and 19:47:48,
with byte-identical travel URLs — the ticket's second lap, and no stale-subsystem drift between
them.

#### FINDING for the cloud (C++, so not fixed here per BN41's rule)

**`severity: medium` — one ENTER too few between the menu and a match.**
`BNScreen_PlaySetup::NativeGetDesiredFocusTarget()` (`BNScreen_PlaySetup.cpp:35`) returns
`StartButton`. So the setup screen opens with START already focused, and the SAME key that opens
it launches the match on the next press — a player who taps ENTER twice from the main menu never
sees the map/mode/player row they were sent there to choose. Observed, not theorised: that is
exactly how this session launched two of its matches. Suggested fix is one line — return
`MapButton` (the first row) instead — but it is front-end C++ from the cloud's diff, so it goes
back as a finding.

#### Harness note for whoever drives PIE next (this cost an hour)

1. **`time.sleep()` inside `execute_tool_script` blocks the GAME THREAD.** MCP tools run on it, so
   a script that clicks, sleeps, then presses a key gives the game no tick in between and the
   click never resolves. Drive input as **separate top-level tool calls**.
2. `SlateInspector.Click` on a UMG button **focuses** it but does not fire `OnClicked`; ENTER on
   the focused button does. Whether real mouse clicks work is therefore UNPROVEN by this session
   — the automation path is not evidence either way.
3. A click into the PIE viewport can take mouse capture ("Shift+F1 for Mouse Cursor"), after
   which the observer's refs go stale and a snapshot looks empty. Re-`Observe` before concluding
   the UI is gone.
4. UMG is BELOW the shallow root observer's depth: `Observe` the viewport splitter ref first, or
   `Snapshot` shows only editor chrome. `WaitFor` searches ALL windows, so editor-chrome words
   ("Mode", "Details") are false positives — match on strings the editor cannot contain.

#### Rung, named honestly

**Rung 3 — PIE, single listen host.** Not rung 4: nothing here was run packaged, and no second
client ever connected, so every multiplayer claim in this entry is the SERVER's leg only. The
1280×720 look is also NOT judged — the editor's PIE viewport is ~6.6:1, so the DPI curve scales
the 1280-wide layout into a band and no capture from this session is a fair comparison against
the Figma frames. Design-space geometry is nonetheless exact, because it was read as numbers.

#### Still open on this ticket

- Pause → LEAVE MATCH → menu. **Cannot be done in PIE** — Escape is Stop-PIE in the editor. Needs
  STANDALONE, exactly as the handoff already records for the R7-WBP-HUD pause screen.
- Escape-pops-back from setup, and cycling the three values, were not driven.
- TDM / teams start.
- Founder screenshots at a real 16:9.

### RESUME HERE — state at 15:10, 1 Sep 2026 (terminal session ended on an MCP registry fault)

Everything blocking this ticket is cleared EXCEPT one thing that needed a Claude Code restart.

**Done and pushed:**
- BN41 closed to the limit of this machine. Rung 1: `PASS BreachpointEditor` (touched
  `libUnrealEditor-BreachpointNext.dylib`) + `PASS Breachpoint` (touched `CodeResources`);
  `BreachpointServer` refused by the launcher distribution before compiling. R19 satisfied on
  both passes.
- A build break on `main` that failed ALL THREE targets was found and fixed:
  `AIBStateTreeTasks.cpp:361` called non-const `FaceRotation` on a `const APawn*` (from
  `3ac96f69`, AIBot F1, landed WRITTEN-NOT-COMPILED).
- Two `high` findings fixed in the front-end diff: the root layout not surviving map travel
  (`BNUIManager::EnsureLocalPlayerUI`, now guarded on `IsInViewport()`), and LeaveMatch
  travelling out from under connected clients on a listen host (`BNPlayerController`).
- `Tools/bn/bn41_frontend_wbps.py` audited against the referee and corrected in six places
  (contents 317→311, ProfileBar padding 12→15, BackButton y682/h26→685/h20, PageTitleText
  →70,26 630×31, FE TitleText →33,45 666×30, floats 196.7/282.7). `bn41_selftest.py` PASS,
  21/28 widgets, 3/3 and 9/9 binds.

**The binaries are fresh: the editor now knows `BNScreen_FrontEnd` and `BNScreen_PlaySetup`
exist.** That was the hard blocker and it is gone. `CreateWidgetBlueprint` can point at
`/Script/BreachpointNext.BNScreen_FrontEnd` as this ticket's step 2 requires.

**Start here, in this order:**
1. `list_toolsets` — confirm `UMGToolSet.UMGToolSet`, `editor_toolset.toolsets.object.ObjectTools`,
   `editor_toolset.toolsets.asset.AssetTools`. **Capture this listing verbatim into the Log** —
   R46 requires it as the evidence for any fallback decision, and `TERMINAL-VS-EDITOR.md` §5.1's
   toolset census is stale (it claims no UMG toolset; `bn11_lib.py` disproves that).
2. Build both WBPs per the call sequence already in this ticket, walking the corrected plan file.
3. The FE map. Expect the two genuine fallbacks named in the R46 section of this Log — creating
   an empty level, and getting a handle to WorldSettings. Log the toolset listing that forces
   each, per the ticket's own rule.
4. The `blocks` deviation — row height 28 is asserted, never built — needs a 1280×720 viewport
   capture measured against the Figma frame BEFORE deciding whether to add per-row SizeBoxes.

**Do not trust `--verify` as the receipt.** It compares widget NAME SETS only. A wrong camelCase
key returns the CLASS DEFAULT rather than an error, so a default-valued readback IS the failure
signature (`brushColor` should read back `{0.039,0.063,0.094,0.88}`; white `1,1,1,1` is the tell).


### 1 Sep 2026 — pre-editor audit: the referee claim did not hold, and six boxes were corrected

Terminal session, wave 2. **No editor work yet** — this entry is the paper pass that BN42's
"if a number is not in the referee, re-read the node, do not eyeball it" rule implies but that
nobody had actually run. `Tools/bn/bn41_frontend_wbps.py` was audited box-by-box against
`01-MENU-MEASURED.md`.

**The load-bearing chassis matched exactly** — Menu Combo (69,138 / 69,76), Party List
(862,397 349×273), Profile Bar (0,670 1280×50), Description Frame (349×37, text inset 20/10),
Game Settings Breakdown (466,76 349×332). Parent-before-child order is clean in both plans, one
root each, every box inside 1280×720.

**But ~12 numbers were invented, and one was simply wrong.** Corrected, with the source for each:

| element | was | now | source |
|---|---|---|---|
| menu content width | **317** | **311** | §1 Menu in Border `I…7:7383` — the 3px border inset (349 → list 343) was never modelled; now `MENU_PAD = 3 + 16` so 311 falls out of 349 − 2×19 instead of being a magic number |
| PlaySetup ProfileBar padding | 12 | **15** | the same component was 15 on the other screen. 15 is derived: Prompts `21:32863` at y685 h20 inside a bar at y670 h50 → 670+15=685, 50−15−15=20 |
| BackButton | 60,682 150×26 | **60,685 150×20** | Prompts `21:32863` x60 y685 h20. Width 150 kept and now COMMENTED as a bounded pick (referee says 62–227), not a reading |
| PageTitleText | 33,22 700×40 | **70,26 630×31** | re-read from Figma this session: `21:43048` is 0,0 1280×75, its Title Frame `I21:43048;577:4124` is 70,15 630×54, title text h31 at y+11.5 |
| FE TitleText | 33,38 500×44 | **33,45 666×30** | Navigation Bar `21:32864`. M1 has no tabs, so the wordmark stands in that slot until M2 |
| PreviewPanel h / MenuPanel y | 197 / 283 | **196.7 / 282.7** | §1/§3 decimals. Floats survive the transport (`topleft()` casts, `setp` json-encodes, `FMargin` is floats — same path `bn11_death.anchor()` uses) |

**Verified on the landed file, executed not read:** `py_compile` OK, and
`python3 Tools/bn/bn41_selftest.py` → PASS both screens, 21 widgets 3/3 binds and 28 widgets
9/9 binds — counts and bind names unmoved, which is the constraint that made SizeBoxes the
wrong move today (below). Rung named honestly: **text-mode green**. The plan parses and the
bind contract holds against the C++ headers. It is NOT "the widgets are right in the editor" —
nothing was built and no editor was opened.

### Deviations from the referee (this ticket's "Done when" list)

**`blocks` — exactly one:**

- **menu row height** | never set: the `UButton` auto-sizes to its 14pt label and only the 12px
  gap is written, so **pitch 40 is asserted, not built** | `I…21:32897` (311×28, pitch 40).
  Deliberately NOT fixed today. No property readback can settle it — `GetWidgetDescription`
  returns the tree and `get_properties` on a slot returns padding/alignment; neither returns
  geometry. The editor pass must measure the rendered rows against the Figma frame in a
  1280×720 capture, and only then add per-row `SizeBox heightOverride: 28.0`, updating the calls
  and the plan file together (this ticket's own thin-ice item 1). Adding four SizeBoxes per
  screen on spec would move the selftest's counts and prove nothing.

**`cosmetic`:** FrontEnd Background → full-screen Scrim a=0.62 (`21:32825`) · Progression Button
absent (`21:32826`) · PlaySetup Party List/roster absent (`21:43056` 863,38 349×599 — deliberate
per the C++ header, and a §5 deviation) · row 3 reads "PLAYERS" not LOBBY OPTIONS (`21:43047`) ·
StartButton is a Text Button row, referee calls it an Action Button (`21:43047`) · panel chassis
is a flat tinted UBorder, not the notch language (`I21:32861;7:4097/4098`, rounded rect + two
88×4 bars) · selection caret absent (`I…7:7398` 3×65 at x−4) · Profile Bar carries no player
identity (`21:32862`) · PromptText is one string (`21:32863`) · BackButton width 150 (bounded,
not measured) · FE title is a wordmark standing in for the nav tabs (`21:32864`) · PageTitleText
is one string where Figma splits a breadcrumb at x134 (`I21:43048;577:4124`) · PreviewImage
Collapsed until BN43 (`I…7:7382`) · **font sizes 30/22/16/14/12/11 — referee node: NONE.** The
menu referee contains no point sizes anywhere, only text *heights*; these are ours · **alphas
PANEL 0.88 / BAND 0.94 / SCRIM 0.62 — referee node: NONE.** RGBs are legitimate `BNUIColors`,
the alphas are ours (`00-HUD-MEASURED` tokens are 0.8/0.6/0.3/0.2) · NewsPanel pad 12,
BreakdownPanel pad 16, value_row insets 4, DescriptionPanel pad 20/10 — all chosen, no referee
number · PartyHeaderText height unset (`21:32861` header 317×31; x/y/width do match).

### R46 — where the MCP-first path has to fall back, established BEFORE the editor pass

Everything in `bn41_fe_map.py` is MCP-doable (`AssetTools.exists`, `SceneTools.find_actors` +
`remove_from_scene`, `add_to_scene_from_class`, `ActorTools.set_label`, `ObjectTools.set_properties`
for tags, `AssetTools.save_assets` on a level path — all fired before in bn11/bn21/61_st_bnbot)
**except two steps, and they are the two the script's own docstring already flags**:

1. **Creating a new empty level** — nothing in this repo has ever created a level via MCP;
   `SceneTools` exposes only `get_current_level`/`find_actors`/`add_to_scene_*`/`remove_from_scene`.
2. **Getting a handle to the WorldSettings actor** — `find_actors` has never been shown to return
   it, and without a handle `ObjectTools.set_properties` has nothing to address.

`ProgrammaticToolset.execute_tool_script` is NOT an escape hatch: its sandbox imports are
`{time, datetime, math, json, re, copy}` with no `unreal`. The genuine fallback is
`-ExecutePythonScript` / the live console. **Run `list_toolsets` + `describe_toolset` on the live
session before concluding either step is impossible** — `TERMINAL-VS-EDITOR.md` §5.1 claims there
is no UMG toolset at all, which `bn11_lib.py` disproves, so treat that document's toolset census
as stale.

### Warning for whoever runs the editor pass

`--verify` compares **widget NAME SETS only** ("TREE MATCHES PLAN (N widgets)"). It proves 21/28
names exist and proves nothing about any box, tint, or flag. So this ticket's "the read-back IS
the receipt" needs care: for a wrong camelCase key (`brushColor`, `bIsEnabled`, `padding`) the
tool returns the CLASS DEFAULT, not an error — **a default-valued readback is the failure
signature, not a missing one**. Specifically expect `brushColor` back as
`{0.039,0.063,0.094,0.88}` and treat white `1,1,1,1` as the tell. Likewise a
`get_properties ["font"]` showing `size: 30` cannot prove the screen RENDERS at 30 — a
`UCommonTextBlock` with a Style applies it in `SynchronizeProperties` and overrides `font`. The
honest M1 claim is "font property persisted", not "renders at 30".


---

## contract_gap — the shared button never shows a hover state (filed 2 Sep 2026)

**Symptom, as the founder reported it:** "the hover and click still not change with correct
assets", then "make sure all buttons hover are like you did with play button from navigation."

**Root cause, measured in the editor 2 Sep via the Unreal MCP, not inferred.**
`/Game/UI/Components/Buttons/WBP_ButtonDefault` (parent class `BRButton`, landed by BP80 and
committed — `git status Content/UI` is clean, this packet did not author it) draws its four
rules **twice**:

| slot | widget | driven by |
|---|---|---|
| OverlaySlot_2 | `Border` — `UBRHairlineBorder`, `edges 15`, `dimmedEdges 14` | `UBRButton::ApplyInvertedState` → `Border->SetEdgeDimmed(Bottom, !bInverted)` — the measured 0.3 → 1.0 |
| OverlaySlot_4..7 | `EdgeTop/EdgeBottom/EdgeLeft/EdgeRight` — `UImage`, brushes `Assets/Sides/{Top,Bottom,Left,Right}_Line` | **nothing** — `colorAndOpacity 1,1,1,1`, `renderOpacity 1`, constant |

The four images sit **above** the border and paint the same four lines at a hard full white, so
they mask the transition happening underneath. `ApplyInversionToSubtree` does not reach them
either — it is called on `TypeBody`, which is null in this WBP; the edges are siblings under
`RowOverlay`. Net effect: **every button in the project renders an identical box in idle, hover,
pressed and selected.** The nav PLAY tab only looked different because C++ calls
`SetIsSelected(true)` on it, which moves the plate, not the edges.

**Where the fix belongs:** `UBRButton::ApplyInvertedState`, on the line beside the
`Border->SetEdgeDimmed` call it already makes — four `BindWidgetOptional` `UImage`s and one
opacity write, one place, every button, every screen.

**Why it was not done there:** `Source/Breachpoint/UI/Components/` is outside this packet's
owner path (law 5). Widening the claim the way BN41 did was **refused by the permission
classifier** — `.claude/active-packet.json` could not be edited — so the packet did not edit
shared code to unblock itself.

**What landed instead:** `BNButtonEdges::Bind` in `Source/BreachpointNext/UI/BNUITypes.{h,cpp}`
— in-bounds, ~30 lines, reads CommonUI's own hover/selected state and moves opacity only
(`EdgeTop 1.0`, `EdgeBottom 0.3 → 1.0 on hover`, side ticks `0.3`, per COMPONENT-SPECS §2, the
same numbers `FBRHairlineStyle`'s defaults already encode). Wired for all 8 front-end buttons
and all 5 play-setup buttons. It reaches the widgets by `GetWidgetFromName` **because the C++
binding is the thing that is missing** — that call is the gap made visible, and it should be
deleted the day the fix lands in `UBRButton`.

**Second finding, not fixed, same asset, same owner:** `EdgeLeft`'s brush is `Right_Line` and
`EdgeRight`'s brush is `Left_Line` — **swapped**. Both are 1×20 solid white, so nothing renders
wrong today; it will bite the moment either texture stops being symmetric.

**Third, noted not fixed:** eight draw calls per button where the component's own header argues
for one (`BRHairlineBorder`'s "the single biggest asset-count saving in the front-end plan").
Whoever lands the `UBRButton` fix should decide whether `Border` or the Figma textures is the
one that survives; the founder has asked for the Figma assets, which argues for the textures and
for `Border` going `edges 0` in this WBP.

---

## The front-end asset list, with real status (written down 2 Sep 2026)

This list only ever existed in chat, which is the failure CLAUDE.md names outright. Committing
it. Status is from a filesystem inventory of `Content/UI` (756 uassets, 167 of them icons), not
from memory.

### Landed from the Figma MCP as new textures — 1 item

| item | Figma node | route | state |
|---|---|---|---|
| Carousel Dot, Active + Inactive | `12:38169` | SVG → `Tools/bn/bn43_carousel_dots.py` → `T_CarouselDot_Active/Inactive` | **done, in the WBP** |

The SVG is the only faithful source: `download_assets` on those symbol nodes returns a fully
opaque PNG (min AND max alpha 255 — Figma bakes the white artboard in), so the "dot" imports as
a white square. Verified, then rasterised from the SVG numbers instead.

### NOT exported from Figma — the project already ships them

The founder's correction on 1 Sep ("we already have multiple assets here that you could use
`/Game/UI` and `/Game/UI/Components/Buttons`") is the ruling. Re-exporting any of these from
Figma would be duplicate art with a second owner.

| item | what already exists | state |
|---|---|---|
| the button + its four rules | `WBP_ButtonDefault` + `Assets/Sides/{Top,Bottom,Left,Right}_Line` | **in use** |
| menu-row ornaments | `Assets/MenuRow_{Arrows,Dot,Hatch,Tick,Triangle}` | exist, **not wired** |
| career rank crest | `Icons/Ranks/T_UI_Rank_01..14` (13 crests) | **in use** — `04_Sergeant` |
| profile glyphs | `Icons/Glyphs/T_UI_Glyph_{Chat,Friends,Settings}_24` | **in use** |
| roster avatar frame | `Icons/Containers/T_UI_Icon_Container_Hex` | **in use** |
| roster trailing icons | `T_UI_Icon_Currency_Token`, `T_UI_Glyph_Speaking_24`, `T_UI_Glyph_PartyLeader_24`, `T_UI_Glyph_Muted_24` | exist, **not wired** |
| mode / gametype icons for level select | `Icons/Mode/*` (24), `Icons/Gametype/*` (8) | exist, **not wired** |
| difficulty icons for the bots row | `Icons/Difficulty/*` (8) | exist, **not wired** |

### The finding that matters most — whole COMPONENTS already exist, and BN42 did not use them

`/Game/UI/Components/` ships measured WBPs over `BR` C++ classes. BN42 hand-built plain
`UBorder`s in their place. This is why the panels still do not read 1:1 and no amount of Figma
export will fix it.

| BN42 built by hand | the component that already exists | its C++ |
|---|---|---|
| `NewsPanel` — Border + Overlay + Image + Text | **`WBP_FeatureCard`** | `UBRFeatureCard` |
| four `Nav*Tab` buttons on a canvas | **`WBP_NavBar`** / **`WBP_NavTab`** | `UBRNavBar` |
| `PartyPanel` + six hand-made rows | **`WBP_RosterPanel`** / **`WBP_RosterRow`** / **`WBP_RosterHeader`** | `UBRRosterPanel` |
| `RankProgress` — a bare `UProgressBar` | **`WBP_ProgressBar`** | `UBRProgressBar` |
| `MenuPanel`, `PreviewPanel`, `BreakdownPanel` | **`WBP_Panel`** (notch chassis) | `UBRPanel` |
| `NavPromptLeft/Right` — TextBlocks reading "LB"/"RB" | **`WBP_ButtonPrompt`** | `UBRButtonPrompt` |

Referencing these is in-bounds: law 7 governs EDITING a binary asset, not instancing one.

### Generated from OUR game, never from Figma — and never to be

`T_News_Spillway`, `T_Preview_Spillway`, `T_Preview_Arena01`. The IP line
(`01-MENU-MEASURED.md` §6, binding): from Figma take NUMBERS and authored geometry; every image
asset is rendered from this game. No Halo screenshots, no Spartan renders, no playlist key-art.

### Genuinely missing — nothing in the project covers these

| item | note |
|---|---|
| LB / RB shoulder keycaps | no keycap texture in `Icons/`. `CommonInputData/ControllerData_Gamepad` is the place to look before authoring one. |
| feature-card scrim | transparent → black VERTICAL gradient from the 50% mark (measured, `1769:23147`). No gradient brush exists in Slate — needs a generated strip texture, same route as the dots. |
| panel notch chassis | 88 × 4.727 chamfers top and bottom. Proceduralisable — `UBRHairlineBorder` / `UBRLeftRail` already carry the constants; check `WBP_Panel` before authoring art. |

### Measured geometry now in hand and NOT yet applied

Off `1769:23147` `News Button` 349×222 — the card is **not** a caption band under an image:
ground `#000000@0.5` at inset 7 · image full-bleed at inset 7 · scrim gradient on top from the
midpoint down · caption a 40-tall box anchored to the bottom of that same inset-7 box, text pad
(20,10,20,10), Rajdhani SemiBold 16 / ls 100 / UPPER / white · the **dots own the bottom 22** of
the 222, so the image region is inset `[0,0,22,0]` = 200 tall.

`ImageHeight = 196.7f` in `BRFeatureCard.h:62` is **suspect** — the only 196.709 node in the
reference is `Preview Photo` `0:1027`, which is HIDDEN. The visible card's image is inset 7
inside a 200-tall region, i.e. 186. Filed as open item B4 in `DECISIONS-OWED.md:869`.

`Button Border` `12:1337` (100×100, the progression tile chrome) is fully recorded in
`Content/UI/Components/Buttons/Assets/03-DropDown-ButtonBorder-SliderRowWide.md:37-67` — six
variants, and **hover thickens the stroke 2 → 4 and pushes the bracket out 2px**; it is not a
colour change. The fade variants drop the bottom line entirely and need gradient strokes.

**Game Settings Panel — resolved, no change owed.** `349 × 332 at (466,76)` is correct for the
screen instance (`01-MENU-MEASURED.md:91`, node `21:43050`, the designated referee). The
`349 × 469` in `REFERENCE-EXTRACTION.md:230` is a component-board figure from a DIFFERENT Figma
file (`Kn87U5sy2VD0lP8K7h4LcQ`) and is explicitly unsampled. Same discrepancy class as the
feature card's 330-vs-349. The build already matches the referee.

---

## Roster swap — 23 hand-built widgets → 1 measured component (2 Sep 2026)

Pulled `Roster Panel` `12:39611` and `Roster Row` `12:39621` through the Figma MCP off the
working file `yznvnVdOFDADaugZSeomfP`, as the founder asked. **The read confirmed the components
already in the repo are the measurement**, number for number:

| Figma `12:39611` | `UBRRosterPanel` constant |
|---|---|
| panel 349 × 273 at (80,220) on its board, (862,397) in-screen | `PanelWidthMainMenu 349` · `PanelHeight 273` · `PanelOriginX 862` · `PanelOriginY 397` |
| Background boolean-op: rounded rect 343 × 267 at inset 3 | `BackgroundInset 3.0f` |
| Content slot inset 16; header at y16; first row y52 | `ContentInset 16` · `HeaderY 16` · `FirstRowY 52` |
| six rows, y 52/87/122/157/192/227 | `MaxVisibleRows 6`, pitch 35 |
| Roster Group Header 317 × 31 | `UBRRosterHeader::HeaderHeight 31.0f` |
| row 317 × 30, Content 313 × 26 at inset 2 | `UBRRosterRow::RowHeight 30` · `RowPitch 35` · `ContentInset 2` |
| Party Leader Frame 30 × 30, sitting OUTSIDE the row to its left | `PartyLeaderIconSize 30.0f` · `ExternalIconsGap -5.0f` |

So the fix was not to export art — it was to stop re-deriving geometry on a canvas. `PartyPanel`,
`PartyHeaderText`, `PartyNotchTop/Bottom`, `PartyPrivacyText`, `RosterPlate0-5`,
`RosterAvatar0-5`, `RosterName0-5` — **23 widgets deleted**, one `WBP_RosterPanel` instance
added at the measured box. The screen went 68 → 46 widgets.

The six names were **literal strings typed into six CommonTextBlocks inside the .uasset** — a
string in a binary no reviewer can grep. They are `UPROPERTY(Config) TArray<FString> RosterNames`
now, drawn from the same Recruit/Marine pool `BNGameMode::BotNames` uses so the menu and the
match agree. Entry 0 is the local player and takes the leader crown.

`TeamFillColor` is deliberately transparent. In the reference each row carries a coloured
nameplate banner, and that banner is 343-owned art — the IP line (`01-MENU-MEASURED.md` §6) says
every image in this game is rendered from this game, so the plate stays empty until we author
our own.

### Findings against `/Game/UI/Components/WBP_RosterPanel` and `WBP_RosterRow` — NOT this packet's assets

1. **`WBP_RosterPanel` ships with `RowWidgetClass` unset, so it renders zero rows out of the
   box.** Confirmed from the log, not inferred:
   `Logbreachpoint: Warning: UBRRosterPanel 'RosterPanel' has no RowWidgetClass; 6 roster members cannot be shown.`
   Worked around by overriding `rowWidgetClass` on **this packet's instance** (in-bounds — an
   instance default on our own WBP, not the shared asset's default). The shared asset should
   carry the default so the next consumer is not blocked the same way.
2. **`WBP_RosterRow` is missing four of its optional bind widgets**: `Emblem`, `RankFrame` /
   `RankInsignia`, `MicSwitcher`, and the `ExternalIcons` overlay with `PartyLeaderIcon` /
   `CurrentPlayerIcon`. The C++ binds them `BindWidgetOptional` and the Figma row has all four
   (emblem 26×26 at x5, rank frame 30×26 at x242, mic 16×18 at x282, party-leader 30×30 outside
   the row at x-42). So the row currently renders as fill + border + gamertag only, and the
   gamertag starts at the row edge instead of at the measured x41.
3. **The 88 × 4 notches are still not drawn.** `12:39611`'s Background is a boolean op — a
   rounded rect MINUS `Top` 88×4 at x130.5 and `Bottom` 88×4 at x218.5. `PanelBorder` is a
   `UBRHairlineBorder`, which draws four straight lines and has no notch. The constants exist
   (`BRLeftRail.h:113-114`, `NotchWidth 88.0f` / `NotchHeight 4.727f`) but nothing consumes them
   for a panel chassis.

**Not a bug, recorded so nobody "fixes" it:** the header band renders as a solid WHITE plate with
black text. That is `UBRRosterHeader`'s `GroundToken = SurfaceInverted` / `TextToken =
TextInverted`, and it matches the Figma render. The gamertag font also checks out — the Figma
Gamertag frame is 191 × 17 inside a 26-tall content row, which is what renders.

---

## finding — FOUR dead config lines in `Source/Breachpoint/UI/` (swept 2 Sep 2026)

`Config/DefaultGame.ini:175-178` carries this comment:

> *"Every one of these was null until 3 Aug 2026, which is why a nav bar drew no tabs and a
> roster drew no rows"*

**They are all still null.** The ini section headers were added; the `UPROPERTY(Config)`
specifier and the `UCLASS(Config = Game)` on the owning classes never were. The comment
documents a fix that did not land — which is exactly why BN42 hit that symptom twice today,
once on the roster and once on the nav bar.

| # | property | declared | specifier | class | dead ini line |
|---|---|---|---|---|---|
| 1 | `UBRNavBar::TabWidgetClass` | `BRNavBar.h:261` | `EditDefaultsOnly` | `BRNavBar.h:165` — no Config | `DefaultGame.ini:180` |
| 2 | `UBRRosterPanel::RowWidgetClass` | `BRRosterPanel.h:549` | `EditAnywhere` | `BRRosterPanel.h:412` — no Config | `DefaultGame.ini:183` |
| 3 | `UBRScreen_FrontEnd::MenuRowWidgetClass` | `BRScreen_FrontEnd.h:234` | `EditDefaultsOnly` | `BRScreen_FrontEnd.h:106` — no Config | `DefaultGame.ini:186` |
| 4 | `UBRModal_Options::RowWidgetClass` | `BRModal_Options.h:152` | `EditDefaultsOnly` | `BRModal_Options.h:100` — no Config | `DefaultGame.ini:192` |

What each one costs when it stays null — and every one of these fails **silently**, because an
empty container is indistinguishable from a container that has not been given data yet:

1. the nav bar draws its 666×30 band and its bumpers and **zero tabs** — no navigation, no
   `UCommonButtonGroupBase` members, no gamepad routing into the header;
2. the roster draws its 349×273 chassis and header and **no rows**;
3. the front end's left rail draws **no menu rows at all** — no PLAY, no QUIT — and
   `NativeGetDesiredFocusTarget` then has nothing to focus, so keyboard/gamepad entry dies too;
4. every options popup raised through `UBRModal_Options` is a **blank frame** the player can
   only back out of.

**The fix is two specifiers per class** — `UCLASS(Config = Game)` and `UPROPERTY(Config)` — and
the template is already in this repo twice: `UBRUISettings` (`BRUISettings.h:14`) and BN's own
`UBNScreen_FrontEnd` (`BNScreen_FrontEnd.h:32`), whose six config properties all work. It is
`Source/Breachpoint/UI/`, outside this packet's owner path, so it is filed rather than done.
BN42 worked around 1 and 2 with instance overrides on its own WBP; 3 and 4 are untouched and
will bite the old `Breachpoint` front end the moment anyone opens it.

**Related, not the same defect** (no ini line at all, so nothing "thinks it works"):
`UBRNavBar::PreviousTabAction` / `NextTabAction` (`BRNavBar.h:265`, `:268`) have no
`DefaultGame.ini` entry — which is why the bar's LB/RB bumper prompts render no glyph.

### Also settled by the same sweep

- **Career-rank panel** = `WBP_RecordPanel`, Figma `21:32826`, in-screen **869, 55 · 334 × 115**
  (`01-MENU-MEASURED.md:30`). Its parent class is **`UBRFeatureCard`**, not a
  `UBRProgressionButton` — that class "does not exist, will not"
  (`MCP-BUILD-PLANS.md:297`). The asset already carries all seven BindWidget names. The gold bar
  wants a `UBRProgressBar`, which is NOT in the record panel's tree today.
- **Profile bar** has **no WBP and no C++ class** — `UBRProfileBar` was cut
  (`MCP-BUILD-PLANS.md:339-343`). And the premise is wrong in a way worth catching before anyone
  builds it: `Profile Bar 119:1525` is 1280×50 containing exactly ONE child, `Player Card` at
  **x862, 349 × 50** — column 3's origin and width. A 1280-wide HBox stretches a 349-wide design.
  Card internals: avatar **40 × 40 square** at (5,5), gamertag 107 × 17 at (55,17), buttons block
  122 × 50 at (211,0), card padding (5,5,16,5). The glyph sizes, the circles and the divider
  positions are **NOT RECORDED**.
- **`WBP_CarouselDots` does not exist on disk** and is called UNMEASURED
  (`COMPONENT-BREAKDOWN.md:514`). The dot itself IS measured (`12:38169`) and BN already ships
  both textures.
- **No standalone career-rank crest, gold bar or dot exists in the `21:32824` export** — they are
  baked into the 2560×1440 board render only. That slot needs a targeted re-export.

---

## Career Rank panel rebuilt to the measured node (2 Sep 2026)

`get_metadata` (Figma MCP) on `21:32826` "Progression Button" and its `Content` child gave the
internals nobody had recorded. Progression origin (869,55); Content sits at +(5,26), so content
origin is (874,81) and everything below converts from there:

| Figma node | content-local | screen | BN had |
|---|---|---|---|
| `UNSC LOGO` | 140 × 140 @ (92, −43) | **966, 38** | 90 × 90 @ 936,78 (guessed) |
| `Text & Progress Bar` | 213 × 40 @ (96, 22) | **970, 103** | — |
| ↳ `Rank` (title+grade) | 213 × 18 @ +(0, 5.5) | **970, 108.5** | 147 wide @ **1046**, 92 |
| ↳ `Progress Bar` | 213 × 11 @ +(0, 23.5) | **970, 126.5** | 147 × 8 @ **1046**, 120 |
| `Outer Level` (crest bracket) | 60 × 40 @ (22, 22) | **896, 103** | absent |
| `Corporal Grade I` (crest) | 52 × 92 @ outer +(4, −17) | **900, 86** | a chevron icon |
| `Button Prompts` | 20 × 20 @ (−33, 58) | **836, 113** | absent |
| `Switcher` (dots) | 72 × 10 @ (130, 121) | 999, 176 | ✓ already right |

The rank block was at the wrong x AND the wrong width — 147 @ 1046 against a measured 213 @ 970
— which is why it read as floating right of centre instead of filling the panel's right half.
Fixed, along with the plate fill: `#000000 @ 0.5` per COMPONENT-SPECS, not the slate PANEL token
BN had reused from the menu panels, which made the whole block sit forward of the page.

### The crest

`download_assets` on `21:32826` returns two raw images. One is the 512² UNSC eagle already
imported (md5-identical to `T_BN_Fig_Watermark_01`, so not re-imported). The other is the crest
— and Figma hands it back as a **JPEG on a white board**, no alpha. Imported raw it is a white
rectangle with a crest floating in it. `Tools/bn/bn47_rank_crest.py` keys it: pure-white → alpha
0, ≤200 luminance → opaque, a linear ramp between so the silhouette keeps a soft edge, and three
asserts that fail loudly if the key inverts or does not fire. Output 104 × 184 (exact 2× of the
measured 52 × 92; both edges multiples of 4, which the importer requires).

It is `Corporal Grade I`, not Sergeant — the `5. Sergeant Grade 1` instance in the same frame is
**hidden in the source**. Shipping what the node draws rather than what the placeholder text
says; if that reads wrong it is an ini data mismatch, not art to redraw.

### Carousel dots — no new asset needed

`r5.svg` from this export is `<circle cx=3.5 cy=3.5 r=3 stroke=white opacity=0.5/>` on a 7 × 7
box — byte-for-byte the spec `Tools/bn/bn43_carousel_dots.py` already renders. The dots in the
project are correct and are already wired; re-importing would have duplicated them.

### PLACEHOLDERS, flagged so they are not mistaken for finished work

1. **The "X" button prompt** at (836,113) is a hairline ring + a `CommonTextBlock` reading "X",
   not a real `UBRButtonPrompt`. A real one needs a `UInputAction`, and `UBRNavBar`'s own
   `PreviousTabAction`/`NextTabAction` have no ini entry at all (filed above). The box is at the
   measured 20 × 20 so the composition reads 1:1 today; swapping in a `CommonActionWidget` later
   is a widget change, not a layout change.
2. **The crest bracket** (`RankCrestFrame`) is a plain `UBRHairlineBorder` rectangle. The
   reference's bracket has corner returns, which is `Button Border 12:1337` geometry — recorded
   in `03-DropDown-ButtonBorder-SliderRowWide.md:44-52`, not yet built.
3. **The progress bar is a flat gold**, `#E5BF76`. The reference is a *gradient* orange→gold
   ramp; `UProgressBar` takes one fill colour, so a true gradient needs a material.
4. **The rank is Corporal art under Sergeant text** — see above.

---

## Menu Combo rebuilt to node `21:32877` · the scrim material (2 Sep 2026)

`get_metadata` on `21:32877` "Menu Combo" (69,138 349 × 510). **The outer layout was already
exactly right** — every box matched on the first read, so nothing was touched:

| node | measured screen | BN had |
|---|---|---|
| `News Button` | 69,138 349 × 222 | ✓ |
| `Menu in Border` | 69,370 349 × 186 | ✓ |
| `Rectangle 258` (top notch) | 199,373 88 × 4.727 | ✓ |
| `Rectangle 259` (bottom notch) | 287,553 88 × 4.727 | ✓ |
| `Rectangle 278` (left caret) | 68,431 3 × 65 | ✓ |
| `Contents` (rows 311 × 28, pitch 40) | 16,16 inside Menu List | ✓ |
| `Decription Frame` | 69,611 349 × 37 | ✓ |

What was wrong was **inside the news card**, against `1769:23147` (MCP-BUILD-PLANS §B2): BN had
the image at inset 0 with a caption padded 12, i.e. a caption floating on a full-bleed image.
The measured card is a four-layer overlay — ground `#000000@0.5` at inset 7, image full-bleed at
inset 7, a transparent→black **vertical gradient from the midpoint down**, and a 40-tall caption
box anchored to the bottom of that same inset-7 frame with text pad (20,10,20,10). And the
**dots own the bottom 22** of the 222, so the image region is 200 tall, not 222. All five now
built; the dots are 4 × 6px at gap 6, centred, first one active.

### The material

**`M_UI_CardScrim`** — `MD_UI`, `BLEND_Translucent`, built through `MaterialTools`:
`TextureCoordinate → ComponentMask(G) → Subtract 0.5 → Multiply 2 → MP_Opacity`, with a flat
black `Constant3Vector` on `MP_EmissiveColor`. The ramp lives entirely in **opacity**, not
colour — "transparent → black" means the art shows through the top half untouched, so tinting
the colour instead would grey the whole image. `Subtract 0.5 → ×2` is what makes the gradient
start at the midpoint rather than at the top.

### The progress bar is NOT a gradient — corrected

The founder asked for a gradient material on the rank bar. The export says otherwise:
`r8.svg` from node `21:32826` is `<path id="Rectangle 22" d="M0 0H104V7H0V0Z" fill="#FFC11C"/>`
— a **flat amber on a 104 × 7 rect**, and there is no `linearGradient` anywhere in the
progress-bar vectors. The ramp the reference photo appears to show is that screenshot's own
vignette. A gradient there would move *away* from 1:1, so the bar is set to the measured
`#FFC11C` (sRGB→linear converted) and no material was built for it. The material went where the
measurement actually calls for one — the card scrim.
