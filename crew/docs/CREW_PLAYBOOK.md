# The Crew Playbook — how this methodology works and why

This kit ports a proven agent-crew system to UE5 multiplayer development. This file is the
transferable METHOD; the agents and contracts are its instantiation. When you extend the crew,
come back here — the rules for growing it are at the bottom.

## 1. The core model: separation of powers

Three kinds of agent, three kinds of power, deliberately kept apart:

- **Producers (builders)** write code/content inside a scoped owner path. They never verify
  their own work and never merge.
- **The critic** is read-only and adversarial. Two modes, set by the packet: **JUDGE** (score
  competing designs against the contracts) and **REFUTER** (try to BREAK a finding or an
  implementation with a concrete attack). Agreement is a finding of last resort.
- **The verifier** runs the acceptance checks exactly as written and reports verbatim. It has
  **no write tools by capability**, so "quietly fixed the test" is structurally impossible.

Why it works: every failure mode of one role is caught by a different role with different
incentives. In practice the REFUTER pass pays for itself fast — on the source project it
killed an entire approach that reviewed as plausible (it would have shipped confident
under-counts), caught a latent regression in a "no behavior change" refactor, and found
schema/plan contradictions before any execution burned time on them.

## 2. Work packets

A packet is one unit of work: goal, owner_path, the contracts that bind it, named acceptance
checks, and inputs. Builders receive exactly one. If the packet is wrong or incomplete, the
builder files a `contract_gap` and STOPS that thread — it never improvises across the
boundary. Packets come from tickets (see the tickets skill); tickets come from the lead
session or the founder.

## 3. The validation ladder

Every packet climbs the same ladder; a rung skipped is a lie waiting to surface.

- **V1 (verifier):** compile → static checks → headless unit specs → functional tests →
  **networked smoke (dedicated server + 2 clients, via Gauntlet)** → perf budget spot-checks.
  Defined precisely in `contracts/testing.md`.
- **V2 (critic):** adversarial pass on anything that touches a dangerous domain (netcode, sim
  math, data schemas, save/load). For netcode the attack is literal: write the cheat.

## 4. The honesty law (game edition)

The multiplayer version of "null beats a guess":

- **Compiles ≠ works.** UHT/UBT green means nothing about behavior.
- **PIE ≠ multiplayer.** Single-process PIE shares memory — replication bugs are INVISIBLE
  there. "Works in PIE" is a claim about one rung only.
- **Listen server ≠ dedicated server.** Different code paths (no local player, no rendering).
- **Live-coding/hot-reload ≠ clean build.** Stale patching lies; verification runs from a
  clean compile.
- **Editor ≠ packaged.** Cook-time differences (missing assets, config strips) only surface
  packaged.
- Every "it works" in a report names its rung: "works (V1: dedicated+2 clients, Gauntlet
  scenario X)" — or it is an opinion, and labeled as one.

## 5. One source of truth per data kind

The data-ownership discipline, mapped to UE (full table in `contracts/data-and-assets.md`):

- **Tuning numbers** (damage, costs, cooldowns, drop rates) live in **DataTables backed by
  CSV/JSON in git** — text, diffable, agent-auditable. A number typed into a Blueprint graph
  or hardcoded in C++ next to a gameplay noun is a contract violation.
- **Gameplay logic** lives in **C++** (testable headless). Blueprints stay THIN: glue,
  cosmetic reaction, designer iteration surfaces — never authority logic, never math that
  needs an audit.
- **Config** ownership per file (`DefaultGame.ini`, `DefaultEngine.ini`) is assigned, not
  ambient.
- Derived artifacts (cooked builds, generated code) are never hand-edited — fix the source
  and regenerate.

## 6. The binary-asset problem (the biggest porting change)

The source project was 100% text — every file diffable, every change reviewable by a read-only
critic. UE is not: `.uasset`/`.umap` are binary. This kit's answer, in order of preference:

1. **Keep logic and data OUT of binaries** (C++ + CSV/JSON DataTables) so the things that
   need review are reviewable. This is the main reason for the BP-thin rule.
2. **One owner per binary file per ticket** — enforced socially via the ticket, mechanically
   via `git lfs lock` (or Perforce checkout if you use P4). Two agents editing one .uasset is
   an unresolvable merge, always.
3. Binary changes are reviewed by **behavior** (the ladder) and by **screenshot/PIE
   walkthrough**, not by diff — and the ticket says which.

## 7. Growing the crew — when to mint a specialist

Do NOT create an agent per feature. A specialist earns a definition file when a domain meets
BOTH tests (this rule produced netcode-builder, sim-builder, and ui-builder — and correctly
kept everything else generalist):

1. **Mistakes there are silent and confident** — the code works in casual testing and fails
   in production (authority bugs, save corruption, cook-only differences).
2. **The doctrine is real** — there exist non-obvious domain laws a generalist won't reliably
   apply (server-authority rules, prediction/rollback discipline, MVVM lifecycle).

A data-curation role (the analog of a canonicalizer that batch-normalizes content through a
zod-style schema gate with critic refutation before landing) is worth minting the day you have
bulk content to import — e.g. migrating item/ability tables from a spreadsheet or a previous
project. Copy the pattern: curator proposes structured records against a fixed schema +
allowlisted keys, critic refutes samples, a builder lands. Curators are read-only; they
RETURN data, they never write it.

## 8. Cadence

Small commits, push frequently, fast-forward only. The handoff between sessions (cloud ↔
local ↔ another machine) is the **tickets board** (`docs/tickets/`, tickets skill): claim
with a STATUS line, log findings in the ticket's `## Log`, check boxes as you land. Git is
the channel; the ticket is the shared memory. Every non-obvious decision gets written down
WHERE THE NEXT SESSION WILL LOOK — a decision that lives only in a chat transcript is lost.
