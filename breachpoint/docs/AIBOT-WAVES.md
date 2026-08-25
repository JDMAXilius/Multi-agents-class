# AIBOT — Wave dispatch doctrine (how the crew fans out)

> Standing doctrine, 25 Aug 2026, by founder directive. A **wave** is one dispatch of
> several agents in parallel, ending at a **barrier** where the lead merges results before
> anything else moves. Waves change wall-clock, never ownership: every artifact still has
> exactly ONE writer, and nothing in this document weakens that law.

## The one rule that keeps waves safe

**Reads parallelize. Writes serialize — unless the packet names disjoint file sets in
advance.** A read-only agent cannot collide with anything, so audit and review waves are
always safe at any width. Build waves are the exception that must be EARNED: allowed only
when the wave's packet lists each writer's exact files, the lists share nothing, and any
shared header landed in a serial step BEFORE the wave. A collision discovered mid-wave
aborts the wave — it is a packet-authoring failure, logged as such.

## Wave types

| Type | Width | Agents | Barrier output |
|---|---|---|---|
| **W-AUDIT** | 2–4 | read-only, each with ONE question | one merged findings list, contradictions named |
| **W-REVIEW** | 4 | aib-critic × its four attack surfaces, one surface per pass | findings merged and ranked by severity; a `high` from ANY pass blocks, exactly as one serial pass would |
| **W-BUILD** | 2–4 | aib-builder instances with disjoint, pre-named file lists | diffs merged serially by the lead; boundary grep runs on the UNION |
| **W-VERIFY** | 2–3 | aib-verifier protocol splits (specs ∥ log-count protocols) | one verdict per protocol, no protocol half-run |

**aib-editor is wave-exempt, permanently.** Editor state is global — one live editor, one
mutable world. Editor tickets run serial, always. This is not a candidate for revision.

## The merge is a real step, not a formality

Every wave ends with the lead (cloud or terminal session) doing three things before the
next dispatch: (1) merge results and de-duplicate findings that two passes both caught;
(2) name contradictions between passes explicitly — two auditors disagreeing is a finding
in itself; (3) re-run the mechanical checks on the merged whole (boundary grep on the union
of a build wave's diffs, both-directions probe check after any node change). A wave whose
merge step was skipped is treated as not run.

## The wave map, phase by phase

| Phase | Wave | Composition |
|---|---|---|
| pre-0 | W-AUDIT ×3 | seam audit: BN input-press path · ASC/ammo read-backs · BNGameMode spawn seam |
| 0 | serial | scaffold is one writer's job (aib-builder), adapter is bn-builder's — two packets, sequenced |
| 1–2 | W-REVIEW ×4 after each landing | containment · fairness · utility pathologies · server-only |
| 3 | serial build → W-VERIFY ×2 | executor lands serial; then specs ∥ first PIE protocol |
| 4 | serial header → **W-BUILD ×4** → W-REVIEW ×4 | the showcase: skill policy header lands first, then Movement/Aim/Grenade/Melee policies in parallel — four files, four writers, zero overlap by construction |
| 5 | serial build → W-REVIEW ×4 | confidence touches the engine — one writer |
| 6 | W-AUDIT ×2 → serial | mode-seam read (BN + module side) before the provider lands |
| 7 | serial → W-VERIFY ×2 | claims land serial; specs ∥ two-bot pickup protocol |
| 8 | W-BUILD ×2 → W-VERIFY ×4 | tiers data ∥ gameplay debugger (disjoint); then one verify pass per tier |
| 9 | W-VERIFY ×3 | A/B match telemetry · fairness latency samples · humanness observations |
| 10 | serial | extraction is one careful move, not a race |

## Dispatch mechanics (for whichever lead runs the wave)

- All of a wave's agents go out in ONE dispatch (a single multi-agent send), so they truly
  run concurrently — a wave issued one-by-one is just slow serial work.
- Each wave member gets a packet that names: its ONE question or file list, what it may
  NOT touch, and the exact shape of its return (findings list / diff / counts).
- Bounded rounds still apply after the merge (default 3, R13 severity gates unchanged).
- Waves nest inside the ticket loop, not around it: claim → waves → merge → Log → push.
  The ticket Log records each wave's composition and merged outcome, so a later session
  can see what fanned out and why.
