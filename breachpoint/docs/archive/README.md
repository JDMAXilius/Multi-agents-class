# `docs/archive/` — the seventeen BreachpointNext task docs

Every `BREACHPOINT-NEXT-TASK-*.md` written between R2 and R7 lives here. All seventeen were
audited on **24 Aug 2026** and every one is **CLOSED**; they were moved here on **25 Aug** by
`git mv`, so their Logs — the record of what was measured, when, and on which rung — are intact
and `git log --follow` still reaches every commit. **Nothing was rewritten to tidy the folder.**

They are here because they are **history, not work**. The board is `docs/tickets/`; BN tickets
are cut there as `TICKET_BN<n>_<NAME>.md`. Anything still live from one of these docs has its
own ticket on that board naming it.

## Read these when you need to know WHY

These Logs are the only record of several decisions the source code cannot explain on its own:

- `R5-ST-BNBOT` — that a StateTree graph **cannot** be authored from Python or any MCP toolset,
  which is why `UBNBotAuthoring` exists at all and why `TICKET_BN10_BOT_ASSETS` needs a live
  editor.
- `R7-WBP-HUD` — the seven C++ gaps the terminal handed back on 22 Aug, four of which R7.6/R7.7
  closed and three of which `TICKET_BN11_HUD_SLOTS` still carries. Its Step 5 also holds the
  four read-backs that are the **FOUNDER's** pass, not any ticket's.
- `AIM-NATIVE-OWNER` / `AIM-LYRA-VERIFY` / `AIM-GRAPH-AUDIT` — the aim-assist lineage, still
  cited by `docs/BREACHPOINT-NEXT-RESEARCH-AIM-REFERENCES.md`.
- `HIT-REACTIONS` — the reaction table `Config/DefaultGame.ini` still names as not-yet-existing.

## One correction on the record

The 24 Aug update in `docs/tickets/HANDOFF.md` said these files were **not** being moved. They
were moved the next day at the founder's request; that block now says so. The reasoning it gave
still holds and is why this was a `git mv` and not a rewrite.
