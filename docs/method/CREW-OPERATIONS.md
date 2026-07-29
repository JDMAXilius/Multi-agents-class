# Crew Operations — the ultra plan
## How the 12-agent studio actually runs, day to day, for six weeks

**Companion to:** `CREW-ROSTER.md` (who) · `crew/CREW_PLAYBOOK.md` (method) ·
`breachpoint/BREACHPOINT-ROADMAP.md` (when). This doc is the **operational layer**: session topology,
model economics, enforcement, escalation, and the metrics that tell us the crew is working.
**Grounded in:** the 2026 production framework for agent teams — *enforced rules → hooks;
contextual knowledge → skills; delegation boundaries → subagents; always-on guidance → a
short CLAUDE.md* — plus Anthropic's orchestration findings already in
`ARCHITECTURE-VALIDATION.md`.

---

## 1. The four-layer enforcement stack (research applied)

| Layer | Mechanism | What lives there |
|---|---|---|
| **Always-on memory** | `CLAUDE.md` (short, at game-repo root — NEW) | The laws in one page, workflow, pointers. Every session reads it free. |
| **Enforced rules** | Hooks + permissions + CI | The grep gates (banned damage API, hard refs, NativeTick widgets, direct attribute writes) run as a `Tools/purity-gate` script: wired as a pre-commit hook and a CI step (BP11). **A rule enforced by goodwill is a suggestion** — until CI lands, the verifier runs the gate every rung-2. |
| **Contextual knowledge** | Skills (`game-lead`, `tickets`) + contracts | Loaded when relevant; contracts are named per-ticket so a builder loads only what binds it. |
| **Delegation boundaries** | Subagent definitions (`.claude/agents/`) | One owner path, one doctrine, scoped tools (reviewers read-only *by capability*). |

**Context-economy rule (from the research):** each agent gets a *clean, minimal* context —
the ticket, its named contracts, its owner path. Never paste the whole doc stack into a
builder; the design history is the lead's context, not the worker's.

## 2. Session topology (who runs where)

```
Juan (TD) ──reviews diffs, plays gates, holds P4/Steam creds, arbitrates
   │
   ├── LEAD session (terminal, game repo, /game-lead) ────────── 1 at a time
   │      decomposes tickets → dispatches → lands → keeps board true
   │      trivial edits: does them DIRECTLY (crew overhead > value)
   │
   ├── BUILDER work ── subagents inside the lead session (default)
   │                └─ OR a parallel session per heavy packet (max 2 extra)
   │                     each claims its ticket via STATUS commit first
   ├── REVIEW ── critic + verifier runs, dispatched by lead per packet
   └── NIGHTLY ── unattended: soak (20 matches) + ladder → morning report
```

- **WIP cap: 3 heavy packets in flight** (TD review is the throughput ceiling).
- **One heavy packet per session** — a session juggling two packets pollutes both contexts.
- **Parallel sessions coordinate ONLY through git + the board** — the claim-commit makes
  double-pickup impossible; owner paths make merge conflicts near-impossible; binaries are
  locked, not merged.

## 3. Model & effort assignment (the economics)

| Role | Model | Why |
|---|---|---|
| Lead session | **Strongest available** (Opus-class) | Decomposition and arbitration are where wrong is expensive |
| netcode-builder, sim-builder | **Strong** (Opus/Sonnet-class, high effort) | The silent-confident domains — quality dominates cost |
| builder, ui-builder, services-builder, anim-builder, ai-builder | **Sonnet-class** | Well-specified packets against contracts; excellent cost/quality |
| critic (REFUTER) | **Strong, high effort** | A weak critic is review theater — this is the last line |
| verifier | **Haiku** (already pinned) | Runs commands verbatim; capability-limited by design |
| curators | Sonnet-class | Structured data against schemas |

**Cost order of magnitude** (estimates, tracked weekly against reality): a heavy packet
chain (build → verify → refute) ≈ 1–3 M tokens; a typical week ≈ 15–30 M ≈ **tens of
dollars, not hundreds** — a rounding error against six weeks of an engineer's time. The
budget line that actually matters is **TD review-minutes**, which is what the WIP cap,
curator-data-not-code pattern, and specs-first discipline all protect.

## 4. The packet lifecycle (one loop, always)

```
lead cuts packet (goal · owner_path · contracts · acceptance · inputs)
  → builder implements (files contract_gap + STOPS if blocked)
  → verifier runs the ladder verbatim (red rungs → back to builder, ONCE)
  → dangerous domain? critic REFUTER attacks (findings need input→wrong output)
  → TD reviews the diff (specs make it self-evident) → lead lands (ff-only)
  → ticket Log updated; board pushed
```

**Retry law:** a packet that fails verify **twice** returns to the lead for re-scoping —
agents don't thrash; scoping was wrong. **Dispute law:** builder disputes a critic finding
→ TD arbitrates with the failure scenario in hand; "plausible" loses to "demonstrated."

## 5. Compaction & handoff hygiene

Long sessions die; the board doesn't. The rules that make any session disposable:
- Every meaningful step pushes (small commits) and Logs (dated findings).
- A session nearing compaction writes a **handoff line** in the ticket Log: state, next
  step, open questions — then any fresh session resumes from the board alone.
- The lead re-reads ONLY: CLAUDE.md, the ticket, its contracts. No archaeology.

## 6. Metrics — is the crew actually working? (weekly retro, 15 min)

| Metric | Source | Healthy |
|---|---|---|
| Verify first-pass rate | ticket Logs | ≥ 60% (lower = packets under-specified) |
| Critic findings that were REAL (accepted) | Logs | > 0 weekly early (0 = review theater) · trending down by W5 |
| Defects found by soak vs. by humans | QA reports | soak-first (humans should find *fun* issues, not crashes) |
| Ticket cycle time vs. roadmap | board | within the week it was scheduled |
| contract_gap count | Logs | > 0 (0 = agents improvising across boundaries) — resolved < 24 h |
| TD review queue depth | — | ≤ 3 (the WIP cap holding) |

Retro output = contract amendments (the contracts are living law — a repeated finding
becomes a rule; a rule nobody hits by W4 gets reviewed for deletion).

## 7. When NOT to use the crew (the restraint list)

- Trivial one-file edits → lead does them directly (dispatch overhead > value).
- Cross-pod refactors → lead-session serial job, never a parallel fan-out.
- Exploration/spikes ("would X work?") → one throwaway session, findings to the Log,
  code discarded — spikes never land.
- Anything during W6 gold that isn't a cited defect → the parking lot.

## 8. Open items (decisions logged when made)

1. Hook wiring (`Tools/purity-gate` as pre-commit + PostToolUse) lands with BP00 wrappers;
   until then the verifier runs it — tracked in BP00's Log.
2. Parallel-session count above the cap requires a TD decision, logged.
3. Weekly retro slot: end of each gate playtest (same sitting, data fresh).
