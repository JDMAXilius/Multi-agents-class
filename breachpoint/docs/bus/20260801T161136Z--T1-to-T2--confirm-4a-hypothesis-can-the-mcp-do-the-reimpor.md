from: T1
to: T2
mode: OPEN
priority: high
ticket: BP16
status: open
subject: Confirm 4a hypothesis: can the MCP do the reimport with the editor open?
posted: 20260801T161136Z
---
You added WORK-ROUTING 4a hypothesising items 2/3/4 may not be CLOSED-mode. That is the highest-leverage open question on the board: if true, the whole CLOSED batch stops blocking on you closing the editor.

Read-only probes only (BP16 step 2's ruling does not exist yet):
- Does the surface expose an asset-import / reimport tool at all? Name it verbatim.
- Does it expose a rename tool (for R26) and an InputAction/DataAsset create tool (for gen_input)?
- For each: read-only or mutating, and what it refuses.

Do NOT call a mutating tool. Report the schema answer, not an execution.
