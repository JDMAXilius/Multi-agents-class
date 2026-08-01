---
name: tickets
description: Ticket board for the session-to-session handoff loop. Use to list open tickets in docs/tickets/, pick one up, execute it with the crew, and write status back so the next session (cloud, terminal, or another machine) can pull and continue. Invoke as /tickets [list|<name>|done <name>].
---

# Ticket board — the handoff loop

Active tickets live in **`docs/tickets/*.md`** (the folder listing IS the board — never trust
a hardcoded enumeration). Completed tickets move to `docs/tickets/archive/`. **Git is the
channel**: every session pushes to the shared branch. Every action here starts with a sync and
ends with a push — that IS the handoff.

## Always first: sync

```bash
git fetch && git pull --rebase   # never skip; other sessions push too
```

## `/tickets` or `/tickets list` — the board

1. Glob `docs/tickets/*.md` (skip `archive/` and `TICKET_TEMPLATE.md`).
2. Status is DERIVED, not stored: **OPEN** if the "Done when" checklist has unchecked `- [ ]`
   boxes and no `> STATUS:` line says otherwise; **DONE** if all boxes checked or
   `> STATUS: done`; **IN PROGRESS** if `> STATUS: in-progress`.
3. Print a compact table: ticket, status, **required context** (the Kickoff's `requires:`
   line — `files-only` / `engine-installed` / `editor-live`), one-line gist, what it's
   blocked on (keys, engine
   install, Gauntlet wiring — read the ticket header).
4. Recommend the next pickup (respect stated ordering — a ticket's "Ordering law" line names
   its gate).

## `/tickets <name>` — pick up and execute

1. Sync (above). Read the ticket fully; read any contracts it names.
2. **CONTEXT gate — check `requires:` FIRST, before anything else.** Every ticket's Kickoff
   opens with one line naming the execution context it needs
   (`BREACHPOINT-AUTHORING-MATRIX.md` §5). Check it against where this session actually is:
   - `files-only` — any session, including a cloud container with no engine.
   - `engine-installed` — a local machine with a **source-built UE 5.8**; headless
     (UBT, `-run=pythonscript`, the rungs). No editor session needed.
   - `editor-live` — a UE editor must be **open on this project** before the claim, because
     the work needs the MCP or an in-editor asset surface. **Open it first**, then claim.
     **R29** binds: one editor, one driver, and an editor session must not overlap a build
     (R21 is the build-lock half; R29 is the editor half).

   Wrong context → **do NOT claim.** Say which context the ticket needs and which this
   session is. This check is first because it is the cheapest: a `files-only` ticket is
   claimable from anywhere, and discovering an `editor-live` requirement after the packet
   is half-built wastes the whole packet.

3. **KICKOFF gate — check the rest, refuse on failure.** Verify every remaining line
   mechanically (file exists, validator passes, gating ticket DONE — run the checks, don't
   read prose optimistically). Any line false → do NOT claim; report which condition failed
   and stop. An output *existing* is not the gate; the gate is the output existing AND its
   validator passing.
4. Add directly under the ticket's H1:
   `> STATUS: in-progress — <machine/side> <date> (<short sha>)`.
   **Write the claim file** `.claude/active-packet.json`:
   `{"ticket": "<name>", "owner_path": [<the ticket's owner paths>]}` — this arms the
   owner-path hook (`guard_laws.py`) for every write in this session.
   Commit + push immediately (`tickets: pick up <name>`) so other sessions see the claim.
5. Execute per the `game-lead` skill: dispatch the crew per the ticket's crew line, honesty
   laws, ladder rungs as written.
6. As tasks complete, check the ticket's `- [ ]` boxes and append findings under its `## Log`
   (dated, short). Verification output, defect lists, and decisions go there — that is what
   the next session reads. Commit + push at every meaningful step.

## `/tickets done <name>`

All boxes checked → `> STATUS: done — <side> <date>`, closing Log entry, **delete
`.claude/active-packet.json`** (disarm the hook), move the file to
`docs/tickets/archive/`, push.
