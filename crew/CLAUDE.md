# BREACHPOINT — Project Memory (read first, every session)

UE 5.8 · pure native C++ · GAS · 4v4 arena FPS · Steam listen server behind
`IBRServerLifecycle`. One runtime module `Source/Breachpoint/`, folder-per-discipline.
Class prefix `BR`. You are one agent in a crew — your definition is in `.claude/agents/`;
the method is `docs/CREW_PLAYBOOK.md`; the law is `docs/contracts/`.

## The laws (violations are findings, not style notes)

1. **Server-authoritative; clients send intent.** New replicated property or `Server` RPC
   = netcode-builder packet + critic REFUTER. Every `_Validate` is real.
2. **GAS purity** (`contracts/gas-purity.md`): attributes mutate only via GameplayEffects;
   abilities are the only action entry; ONE damage pipeline — the engine damage API
   (`TakeDamage`/`ApplyRadialDamage`) is banned; costs/cooldowns are GEs; state = GE-applied
   tags; all FX via GameplayCues. Named exceptions (ammo, movement, match meta) are in the
   contract's ledger — nothing joins it inline.
3. **Data is not code:** tuning numbers live in `Content/Data/*.csv`; row structs in
   `Source/Breachpoint/Data/BRDataRows.h`; asset refs in data are SOFT
   (`TSoftObjectPtr`) — no hard asset refs or `ConstructorHelpers` in C++.
4. **No gameplay Tick.** Timers, delegates, gameplay events, cue notifies.
5. **Owner paths:** write only inside your packet's folder. Blocked? File a `contract_gap`
   in the ticket and STOP — never edit shared code to unblock.
6. **Honesty ladder:** compiles ≠ works · PIE ≠ multiplayer · listen ≠ dedicated ·
   editor ≠ packaged. Every "works" names its rung; multiplayer claims come in threes
   (server, acting client, observing client).
7. **Binary assets:** one owner per `.uasset`/`.umap` per ticket; lock before editing.
8. **Design rulings are closed** (`docs/DESIGN-RULINGS.md`): reviews judge against the
   ledger, never re-litigate it. Only `high`-severity findings block a landing; the rest
   land in the risk register with the artifact.

Laws 2, 3, and 5 are ALSO enforced mechanically: `.claude/hooks/guard_laws.py` blocks
banned APIs and out-of-owner-path writes at tool-call time (claim file:
`.claude/active-packet.json`, written by the tickets skill). A hook block is not an
obstacle to route around — it is the law firing; file the contract_gap.

## Workflow

`git pull --rebase` → `/tickets list` → claim with a STATUS line (commit + push the claim)
→ execute per the ticket's contracts → run the ladder (`Tools/run-ubt.ps1`,
`run-specs.ps1`, `run-gauntlet.ps1`) → write findings to the ticket's `## Log` → push.
Small commits, fast-forward only, never force-push. A decision that lives only in chat is
lost — it goes in the Log or it didn't happen.

## Memory (many readers, one writer)

Shared state is git: contracts, rulings, tables, tickets — any agent reads; every artifact
has ONE writer. Your context is your ticket + its named contracts (+ the rulings ledger if
you review) — never "the repo so far." Decisions outlive transcripts: numbers and calls go
in the ticket's Log; closed tickets archive. Full policy: `CREW_PLAYBOOK.md` §11.

## Quality bars

Perf/net budgets, definition-of-done, playtest protocol: `docs/BREACHPOINT-QUALITY-BARS.md`
(copied from the planning repo). Suites: `Breachpoint.Sim.*`, `Breachpoint.Bots.*`.
Gauntlet smoke: `BRGauntlet.SmokeTS2C`. All three build targets compile on every rung-1.
