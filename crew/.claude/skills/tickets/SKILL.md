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
3. Print a compact table: ticket, status, one-line gist, what it's blocked on (keys, engine
   install, Gauntlet wiring — read the ticket header).
4. Recommend the next pickup (respect stated ordering — a ticket's "Ordering law" line names
   its gate).

## `/tickets <name>` — pick up and execute

1. Sync (above). Read the ticket fully; read any contracts it names.
2. Add directly under the ticket's H1:
   `> STATUS: in-progress — <machine/side> <date> (<short sha>)`.
   Commit + push immediately (`tickets: pick up <name>`) so other sessions see the claim.
3. Execute per the `game-lead` skill: dispatch the crew per the ticket's crew line, honesty
   laws, ladder rungs as written.
4. As tasks complete, check the ticket's `- [ ]` boxes and append findings under its `## Log`
   (dated, short). Verification output, defect lists, and decisions go there — that is what
   the next session reads. Commit + push at every meaningful step.

## `/tickets done <name>`

All boxes checked → `> STATUS: done — <side> <date>`, closing Log entry, move the file to
`docs/tickets/archive/`, push.
