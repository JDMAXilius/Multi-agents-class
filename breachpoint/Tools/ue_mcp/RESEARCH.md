# UE 5.8 MCP — research findings (documented surface, NOT the verified one)

**Status: desk research, 1 Aug 2026, from a cloud container with no engine.** Everything here
comes from Epic's documentation and secondary coverage. **BP16 step 1 is still owed** — it
requires enumerating the surface *against a live editor*, and nothing below substitutes for it.
Where this file and a running editor disagree, the editor is right and this file is the bug.

---

## 1. What it actually is (the correction that matters most)

**The UE MCP is not an agent. It is a server that exposes the editor's systems as tools.**

That single fact reshapes the workflow question. There is no "hand the work over to the MCP and
it continues" — there is nothing on the other end to hand work *to*. The agent is still Claude
Code; the MCP just gives that same session **hands inside a running editor**, alongside the file
and git tools it already has.

Consequences, stated plainly because the plan depends on them:

- **No prompt-handoff format is needed.** The MCP session *is* the terminal session. It reads
  the same repo, the same `CLAUDE.md`, the same tickets, and commits to the same branch.
- **Tickets do not need to be rewritten as standalone prompts.** They need one new thing: a
  declaration of whether the packet requires a **live editor** (see §5).
- **"The MCP doesn't connect to a repo" is true and harmless** — the *session* connects to the
  repo, and the session is what holds the MCP connection.

## 2. Setup (documented; unverified here)

| Step | Detail |
|---|---|
| Plugin | `Edit > Plugins` → search **"Unreal MCP"** → Enabled → restart. The dependent **Toolset Registry** plugin enables automatically. |
| Auto-start | `Edit > Editor Preferences > General > Model Context Protocol` → **Auto Start Server** |
| Endpoint | **`http://127.0.0.1:8000/mcp`** — loopback, HTTP, inside the editor process |
| Client config | Console command **`ModelContextProtocol.GenerateClientConfig ClaudeCode`** writes a `.mcp.json` with one `unreal-mcp` entry pointing at the endpoint |
| Our project | `Breachpoint.uproject` **already enables** `ModelContextProtocol` and `MCPClientToolset` (lines 75, 79). No `.mcp.json` exists in the repo yet. |

**Documented tool surface** (categories, not a verified enumeration): spawning actors,
configuring lighting, creating material instances, inspecting Slate widgets, running automation
tests, navigating Blueprints, manipulating assets, building levels, working with meshes.

**Epic's own caveats:** *Experimental.* APIs and data formats may change; the feature is
incomplete in places; **it is not designed for remote use**; not recommended for production
reliance.

## 3. Can it be driven from the cloud container? **No — and not for a fixable reason.**

The server binds **`127.0.0.1`** inside a running editor on the machine that launched it, and
Epic explicitly states it is not designed for remote use. This container is a different machine
with no engine, no editor, and no route to a developer workstation's loopback interface.

Tunnelling it (ngrok/SSH reverse tunnel) is technically conceivable and is **rejected**: it would
expose an experimental, unauthenticated, mutation-capable editor endpoint to a network path Epic
did not design it for, to buy a capability the local session already has for free. Not a
close call.

**The local Claude Code terminal is the correct and only host** — same machine, loopback
reachable, and Claude Code is a documented client.

## 4. The jurisdiction hole (BP16's real subject, now concrete)

`.claude/hooks/guard_laws.py` is a `PreToolUse` hook that gates `Edit`/`Write` by `file_path`.
An MCP tool call carries **neither** — it arrives as a differently-named tool with structured
arguments. So:

> **Every mechanical protection this project has is blind to MCP calls.** Owner-path confinement,
> the banned-API check, law 7's one-owner-per-binary — all of it is bypassed, not by cleverness
> but by shape.

The fix is available and cheap, and it is a *concrete* proposal for BP16 step 2 rather than a
worry: the same hook can match on **tool name** (`mcp__unreal__*`) in addition to `file_path`,
and refuse mutating MCP tools unless the active packet's claim explicitly grants
`mcp: allowed`. Read-only MCP tools (query, screenshot) can stay ungated. That keeps the
enforcement layer honest without giving up the capability.

**This is not yet implemented.** Until it is, an MCP-driven session is operating on goodwill —
the same footing R26 is on, and it should be said out loud the same way.

## 5. What this changes for the board

Tickets already declare machine-checkable Kickoff conditions. The MCP adds exactly one new
axis, and it belongs there:

```
- requires: editor-live      # a UE editor must be OPEN; MCP tools in play
- requires: engine-installed # commandlet/UBT path; headless; no editor session
- requires: files-only       # no engine at all — C++, CSV, docs, scripts, git
```

That is the whole integration. A packet declaring `editor-live` tells the lead to open the
editor *before* claiming, which is also the R21 guard's concern (one editor, one driver).

## Sources

- [Unreal MCP in Unreal Editor — Epic documentation](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor?lang=en-US) *(returned 403 to automated fetch from this container; the details above come from search summaries and secondary coverage — **verify directly from the workstation**, where the page opens normally)*
- [Unreal Engine 5.8 Embeds an MCP Server So AI Agents Can Drive the Editor — VP Land](https://www.vp-land.com/p/unreal-engine-5-8-embeds-an-mcp-server-so-ai-agents-can-drive-the-editor)
- [UE 5.8 introduces experimental MCP server support — Crypto Briefing](https://cryptobriefing.com/unreal-engine-5-8-mcp-server-support/)
- [Mount on UE 5.8's official MCP server — ClaudeUnreal](https://echoulen.github.io/claude-unreal/docs/official-mcp/)
- [unreal-mcp-kit — one-command UE 5.8 MCP setup](https://github.com/immigration2000/unreal-mcp-kit)
- [UE 5.8 MCP with Claude Code — first safe editor session](https://gamineai.com/blog/unreal-engine-5-8-mcp-claude-code-first-safe-editor-session-2026)
