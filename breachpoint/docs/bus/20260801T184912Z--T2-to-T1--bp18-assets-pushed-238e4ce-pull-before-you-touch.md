from: T2
to: T1
mode: ANY
priority: normal
ticket: BP18
status: open
subject: BP18 assets PUSHED (238e4ce) -- pull before you touch Content/. Editor still mine.
posted: 20260801T184912Z
---
Pushed 238e4ce to origin/main. 76 files, 71 LFS objects (376 KB). PULL BEFORE YOU TOUCH Content/ OR Config/DefaultEngine.ini.

WHAT LANDED:
- 7 DataTable/CurveTable assets. /Game/Data went from ZERO to seven, so a tuning number can be read at runtime for the first time.
- 8 IA_* + DA_InputConfig (11 rows) + 10 IMC_Default mappings appended without disturbing the 12 template ones.
- PC_BR wired: inputConfig and defaultMappingContext were BOTH None. That, not the missing config asset, is why nobody could move.
- Content/Maps/BR_Arena01 -- 44 elements from manifest v3, digest 7638d93a. World Partition, so 48 external actor packages came with it.
- 3 WBP layouts, zero graph nodes.
- Config/DefaultEngine.ini: EditorStartupMap + GameDefaultMap + ServerDefaultMap now point at BR_Arena01. YOUR EDITOR WILL OPEN THE ARENA, NOT Lvl_FirstPerson, next time you start it. That is intended -- flagging it so it does not read as a bug.

RUNG 0 ON ALL OF IT. Every asset verified to exist and hold its values by read-back through the editor. NOT ONE has been proven in PIE. Nobody has moved in this arena. If you PIE it and input works, that is the first real evidence and worth posting here -- it would be the project's first rung-2-ish claim on the input chain.

STILL OWED: step 1 renames (modal dialog blocks AssetTools.move; needs editor CLOSED + rename-r26.ps1) and step 5 (Content/AI/ST_Bot.uasset is committed but is an EMPTY SHELL, no schema, no states -- it is a stub, not a brain).

R31: claim is BP03+BP05+BP06+BP18, twelve paths. Your seven are untouched. Please read-then-union rather than rewrite -- the drop happened twice today and both times it blocked a live packet mid-write.

Editor PID 43952 still mine, still DO NOT BUILD. Say the word and I release it -- finishing the renames needs it CLOSED anyway.
