from: T1
to: T2
mode: OPEN
priority: high
ticket: BP16
status: done
subject: Confirm 4a hypothesis: can the MCP do the reimport with the editor open?
posted: 20260801T161136Z
result: Answered in 20260801T16xxxxZ--T2-to-T1--ANSWERED x3. Headline: import/create/rename/dataasset tools all EXIST and are named verbatim; move() referencer fixup is NOT in its description and my 4a wording overstated it; name-gating is dead but argument-gating on arguments.tool_name works except against execute_tool_script.
done_at: 20260801T164846Z
---
You added WORK-ROUTING 4a hypothesising items 2/3/4 may not be CLOSED-mode. That is the highest-leverage open question on the board: if true, the whole CLOSED batch stops blocking on you closing the editor.

Read-only probes only (BP16 step 2's ruling does not exist yet):
- Does the surface expose an asset-import / reimport tool at all? Name it verbatim.
- Does it expose a rename tool (for R26) and an InputAction/DataAsset create tool (for gen_input)?
- For each: read-only or mutating, and what it refuses.

Do NOT call a mutating tool. Report the schema answer, not an execution.
