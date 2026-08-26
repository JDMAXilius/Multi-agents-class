# TICKET — AIB14: Phase 9 — 4v4 matches, telemetry vs the bars, tuning

> STATUS: open — cut 26 Aug 2026 by the cloud lead. TERMINAL WORK: this is the
> measurement phase — the module code is written; what Phase 9 produces is NUMBERS
> against docs/BREACHPOINT-QUALITY-BARS.md §7, and tuning tickets for whatever misses.

## What landed (cloud)

- `Tools/aib/80_aib_metrics.py` — the harness: parses N match logs, reports per-match
  counters (acquisitions + latency stats, ambition switches + interrupts, F7 failures
  and refusals-per-switch, claims, tiers, wiring warnings; Verbose-only rows honestly
  read "not captured" instead of 0), lobby spread across logs, and PASS/FAIL against
  the bars. Refuses to call <5 logs a baseline (the AIB8 lesson is enforced, not
  remembered). `--bar key=value` overrides; `--json` for machines. Smoke-tested against
  a synthetic log in this session (violations correctly FAIL).
- `docs/BREACHPOINT-QUALITY-BARS.md` §7 — the bot bars: four HARD (F1 floor, zero
  unserved wants, zero wiring warnings, zero FFA claim grants), two PROVISIONAL
  (refusals ≤1.0/switch; Recruit-vs-Spartan latency delta ≥0.10s). Bars move only by
  founder ruling, and the script's defaults move WITH the doc.

## Protocol (terminal)

1. Prereqs: AIB11 (tree rebuild — MANDATORY after the rename), AIB12, AIB13 landed.
2. Per config, FIVE matches minimum, one log each (`Saved/Logs/`, renamed per run).
   Configs, in order:
   a. Marine 4v4 FFA (the shipped default) — the anchor baseline.
   b. Recruit ×8 and Spartan ×8 — the distinctness bar's two ends.
   c. Hill ON (AIB11's ini) at Marine — objective-mode telemetry rides the same lines.
3. `python3 Tools/aib/80_aib_metrics.py Saved/Logs/RunA_*.log` per config; paste the
   full output (per-match + spread + bars) in this Log, verbatim.
4. For melee/grenade counters add `-LogCmds="LogAIBot Verbose"` to ONE run per config
   (Verbose logs are heavy; one is enough to prove the counters move).
5. Any FAILED bar → one line here naming it, then either a tuning proposal (numbers,
   not vibes) or a defect ticket. PROVISIONAL bar misses are tuning conversations;
   HARD bar misses are defects, full stop.
6. The 4v4 acceptance run (proof 2, the A/B): one mixed match BN bots vs AIB bots on
   the arena — the harness reports the AIB half; BN bots have no instrument lines,
   which is itself the observable. Impressions ("reads more human") go in the Log as
   impressions, clearly labeled — the numbers carry the claim.

## Done when

- [ ] Five-match baselines pasted for configs a, b, c
- [ ] All four HARD bars green across every config (or defect tickets cut)
- [ ] The distinctness bar measured Recruit-vs-Spartan with the delta stated
- [ ] Provisional bars confirmed or re-proposed with numbers (founder ruling to move)
- [ ] The mixed A/B match run once, numbers + labeled impressions pasted

## Log

_(terminal: outputs verbatim)_
