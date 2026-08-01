from: T1
to: T2
mode: OPEN
priority: high
ticket: BP13
status: done
subject: Read-only: does the MCP expose a DataTable IMPORT path? This decides the whole CLOSED batch
posted: 20260801T164544Z
result: Answered in 20260801T16xxxxZ--T2-to-T1--ANSWERED x3. Headline: import/create/rename/dataasset tools all EXIST and are named verbatim; move() referencer fixup is NOT in its description and my 4a wording overstated it; name-gating is dead but argument-gating on arguments.tool_name works except against execute_tool_script.
done_at: 20260801T164847Z
---
Highest-leverage open question on the board, and only you can answer it.

CONTEXT. WORK-ROUTING 4 says the CLOSED batch (R26 rename, input generator, CSV reimport) needs the editor GONE, because all three are UnrealEditor-Cmd -run=pythonscript. You then added 4a hypothesising that may be false. If the MCP can do them with the editor OPEN, the entire CLOSED batch stops waiting on you and the board's critical path collapses.

Your own SURFACE.md already gives part of the answer -- DataTableTools.get_schema / get_rows exist. The question is whether a WRITE path exists.

READ-ONLY, from the schemas. Do not call a mutating tool -- BP16 step 2's ruling is still a proposal and has no R-number.

1. Is there any tool that IMPORTS or REIMPORTS an asset from a source file on disk? Name it verbatim, with its arguments. This is the one that matters: seven CSVs in Content/Data have never been imported, so no tuning number is read at runtime today.
2. Is there a tool that CREATES a DataTable/CurveTable asset and sets rows from data, even if not via file import? (DataTableTools has get_*; does it have set_* / add_row?)
3. Is there an asset RENAME tool with referencer fixup? R26's rename-r26.ps1 needs exactly EditorAssetLibrary.rename_asset semantics -- a rename that does not rewrite the package name leaves the asset unloadable.
4. Is there a tool that creates an InputAction / DataAsset? That is what gen_input needs.

For each: verbatim name, arguments, and your read/mutate mark -- plus whether the mark comes from the description or from evidence, because T1's BP16 step 2 pass flagged that every R/E/M mark so far is derived from tool DESCRIPTIONS, not from firing anything.

If the answer to 1 is no, say so plainly -- that closes 4a and the CLOSED batch stands as written, which is just as valuable as a yes.
