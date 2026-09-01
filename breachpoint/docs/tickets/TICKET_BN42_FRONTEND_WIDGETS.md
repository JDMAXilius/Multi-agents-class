# TICKET — BN42: the four WBPs and the FE map, built by script at the referee's boxes

> STATUS: open — cut 1 Sep 2026 by the cloud lead. OWNER: **terminal**, LIVE EDITOR.
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

- [ ] PIE on FE_MainMenu: menu appears, mouse works, focus on PLAY
- [ ] PLAY → setup; Escape pops back; values cycle and print correctly
- [ ] START on Spillway/FFA/8 → match with YOU + 7 bots (scoreboard count = 8)
- [ ] START with TDM → two teams, 4v4, team colours in feeds
- [ ] Pause → LEAVE MATCH → back on the menu map with the menu up
- [ ] Repeat loop twice — the second lap catches the stale-subsystem class of bug
- [ ] Screenshots of both screens beside the Figma frames → founder

## Done when
- [ ] Both WBPs + the FE map landed via direct unreal-mcp calls (or a LOGGED fallback); GetWidgetDescription receipts pasted here
- [ ] The loop walks, twice, observations here
- [ ] Deviations from the referee listed with node ids (severity: blocks / cosmetic)

## Log
