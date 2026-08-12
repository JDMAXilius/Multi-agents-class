# BREACHPOINT NEXT — the crew

**Cut:** 12 August 2026 · **Agents:** `.claude/agents/bn-*.md` (3 files) ·
**Binds to:** the NEXT doc family only.

The crew was sized by the same rule as the file manifest: **count the kinds of work, not the
kinds of titles.** Roadmap 1 contains exactly three kinds of work — writing C++, catching
multiplayer mistakes, wiring assets in the editor. Three kinds of work, three agents.

## The visual structure

```
                         FOUNDER
             plays Stage A (standalone) → Stage B (client+server)
             the ONLY verifier · the only "works" that counts
                              ▲
                              │ checkpoint builds
                              │
   ┌──────────────────────────┴──────────────────────────┐
   │              LEAD  (the main session)               │
   │   cuts goals into waves · dispatches · merges ·     │
   │   writes nothing itself that an agent owns          │
   └──┬──────────────────┬──────────────────────┬────────┘
      │ goal (C++)       │ diff                 │ goal (assets)
      ▼                  ▼                      ▼
 ┌───────────┐     ┌───────────┐         ┌───────────┐
 │ bn-builder│────▶│ bn-critic │         │ bn-editor │
 │  writes   │diff │ read-only │         │ scripts + │
 │ NEXT C++  │     │ ONE lens: │         │ read-back │
 │ one goal  │     │multiplayer│         │  audits   │
 │ at a time │     │correctness│         │ Tools/bn/ │
 └───────────┘     └───────────┘         └───────────┘
      │                  │                      │
      └── blocking finding → back to bn-builder │
                         │                      │
                    PASS ─┴──────────────────────┴──▶ checkpoint offered
```

| Agent | Writes | The one thing it guards |
|---|---|---|
| `bn-builder` | `Source/BreachpointNext/` C++ | scope — only the goal's files, tight, multiplayer from line one |
| `bn-critic` | nothing (read-only) | the standalone-fine / multiplayer-broken bug class — and NOTHING else (style/naming/scope findings are forbidden) |
| `bn-editor` | `Tools/bn/` scripts + the assets they generate | manual editor state — everything is script + read-back audit; the founder never clicks through setup |

## Deliberately not minted

| Not an agent | Why |
|---|---|
| verifier | the founder is the verifier (Stage A/B protocol in every roadmap). A verifier agent without an engine would verify nothing. |
| lead | the main session dispatches and merges; a lead agent file would be the session describing itself |
| curators (tuning/content) | R1 has no content to curate. Minted when a roadmap carries tuning tables or spoken lines |
| per-domain builders (ui/ai/anim) | one builder until a roadmap's work exceeds one context or one discipline's doctrine. Splitting early buys handoffs, not quality |

## The loop (per wave)

1. Lead hands ONE goal to `bn-builder` (C++) or `bn-editor` (assets) — the goal text is the
   packet; no separate ticket format.
2. `bn-builder` returns diff + 5-line report → lead sends the diff to `bn-critic`.
3. `bn-critic`: PASS → merge. Blocking finding (with its input→wrong-output scenario) → back
   to `bn-builder`, once. Notes → recorded in the roadmap file.
4. Wave complete → checkpoint offered to the founder. Founder's Stage B pass is the only DONE.

## Roadmap 1, cut into waves

| Wave | Goals | Agents | Ends at |
|---|---|---|---|
| 1 ✅ | G1 module · G2 tags | (lead — pre-crew) | compiles on founder's machine |
| 2 | G3 GAS spine · G4 input | bn-builder → bn-critic | **Checkpoint A** — walkable in standalone |
| 3 | G5 jump/crouch abilities | bn-builder → bn-critic | **Checkpoint B** — abilities visible cross-client |
| 4 | G6 anim instance · G7 asset scripts | bn-builder + bn-editor → bn-critic (G6) | **Checkpoint C** — mannequin animates in both windows |

## The evolution rule

This crew serves Roadmap 1 and grows ONLY at a roadmap boundary: when the next roadmap's
work has no owner (UI screens, bot brains, session flows), the lead proposes the minimal
addition — new agent, or a doctrine line added to an existing one — in that roadmap's doc.
An agent is never added mid-wave, and never because it "might help."
