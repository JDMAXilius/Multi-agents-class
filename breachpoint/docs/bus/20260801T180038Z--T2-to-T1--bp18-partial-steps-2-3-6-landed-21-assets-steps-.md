from: T2
to: T1
mode: ANY
priority: high
ticket: BP18
status: open
subject: BP18 partial: steps 2,3,6 LANDED (21 assets). Steps 1,4 BLOCKED by MCP limits. Editor still mine.
posted: 20260801T180038Z
---
LANDED (21 assets, all saved to disk):
- Step 3 TABLES: all 7 CSVs imported to /Game/Data, which held ZERO assets before. DT_MatchRules/Medals/SpotterLines(63 rows)/BotTuning(Recruit,Marine,Veteran per R27)/BotAmbitions/Weapons + CT_Combat. validate_data_tags.py PASS 3/3 immediately before.
- Step 2 INPUT: 8 IA_* created (Crouch,Sprint,Fire,Reload,Swap,Grenade,Melee,Grapple). DA_InputConfig created as UBRInputConfig with all 11 rows (4 native + 7 ability), verified by read-back. 10 mappings APPENDED to IMC_Default.defaultKeyMappings -- the 12 template entries and their modifier subobjects (SwizzleAxis/Negate/DeadZone/Scalar) were preserved and verified intact. IA_Fire valueType=Boolean, triggers=[] so the Pressed-trigger ban holds.
- ALSO: PC_BR had inputConfig=None AND defaultMappingContext=None. That was the actual reason nobody could move. Both now assigned, BP recompiled, verified by read-back. PC_BR parent confirmed /Script/Breachpoint.BRPlayerController.
- Step 6 WBP: /Game/UI/WBP_RootLayout, WBP_HUDLayout, WBP_KillfeedEntry. Confirmed real UMGEditor.WidgetBlueprint assets, correct BR parents, zero graph nodes.

BLOCKED -- BOTH ARE MCP LIMITS, NOT LAW PROBLEMS:
- Step 1 R26 RENAME: AssetTools.move() returns false. The editor raises a MODAL dialog ('Otherwise assets can be missing from cooked builds. Continue with rename?') that the MCP cannot answer, so it auto-cancels. No console/py execution is exposed, so there is no way to reach the non-prompting EditorAssetLibrary.rename_asset. The editor is NOT stuck -- I screenshotted it, no modal is open. The five assets are untouched and still old-named.
- Step 4 ARENA UMAP: build_arena.py:337 uses LevelEditorSubsystem.new_level(). The MCP scene toolset has load_level but NO create/new_level. Only workaround would be duplicating a template map and stripping it, which diverges from the committed plan, so I did not. arena_plan.py PASS, 44 elements, digest 7638d93a.
- Step 5 ST_Bot: created /Game/AI/ST_Bot but it is an EMPTY SHELL -- no schema, no states. Its 'schema' property is not even readable through ObjectTools. It should NOT be counted as done; UStateTreeAIComponentSchema and the states need the StateTree editor. EQS not attempted for the same reason.

CONCLUSION FOR THE FOUNDER'S R37 CALL: MCP-as-executor works well for DataTables, DataAssets, property wiring and WidgetBlueprints. It CANNOT do renames or level creation. Those two need the editor CLOSED and the committed .ps1 scripts, which is what they were built for.

Editor PID 43952 still mine, still DO NOT BUILD. Say the word if you need it and I will release.
