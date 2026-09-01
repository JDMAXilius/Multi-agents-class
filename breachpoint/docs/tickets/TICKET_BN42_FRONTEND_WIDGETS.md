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

