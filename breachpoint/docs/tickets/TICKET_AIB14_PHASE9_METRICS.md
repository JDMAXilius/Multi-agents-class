# TICKET — AIB14: Phase 9 — 4v4 matches, telemetry vs the bars, tuning

> STATUS: in-progress — mac terminal 26 Aug 2026 (b2d315f).
> Original: open — cut 26 Aug 2026 by the cloud lead. TERMINAL WORK: this is the
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

### 2026-08-26 — config (a) Marine FFA, five matches: ALL BARS PASS

`Tools/aib/80_aib_metrics.py` over 5 logs, run 1 with `-LogCmds="LogAIBot Verbose"`.

```
=== lobby spread (across logs) ===
   latency_mean          : mean 0.382  min 0.380  max 0.386  n=5
   refusals_per_switch   : mean 0.415  min 0.010  max 1.348  n=5
   ambition_switches     : mean 2591.400  min 2213.000  max 2979.000  n=5

=== bars ===
   [PASS] F1 reaction floor: fastest acquisition 0.221s vs floor 0.20s (HARD bar)
   [PASS] unserved wants: worst match 0 vs bar 0 (per match)
   [PASS] wiring warnings: worst match 0 vs bar 0 (per match)
   [PASS] FFA claim grants: worst match 0 vs bar 0 (per match)
   [PASS] move refusals per switch: mean 0.42 (spread 0.01..1.35, n=5) vs PROVISIONAL bar 1.0
```

**The refusals number is the headline, and it validates AIB9 on five matches.** The
pre-fix five-match baseline was mean **9.54**, range 1.06–35.60 — failing the provisional
bar of 1.0 by nine times. After the drop-cap fix: mean **0.42**, range 0.01–1.35. A 23x
improvement, and the bar it used to fail it now clears with room. That is much stronger
evidence for the stranded-on-the-roof diagnosis than the three runs AIB9 shipped on.

Latency is tight to a degree worth noting: 0.380–0.386 across five independent matches.

**Provisional bar verdict — move refusals ≤1.0/switch: CONFIRMED, keep the number.** The
mean (0.42) sits comfortably under it while the spread's top (1.35) still crosses it, so
the bar is doing its job: loose enough to pass a healthy build, tight enough that a bad
match is still visible. No re-proposal.

#### Two findings from the Verbose run

**1. ~~The strafe is gated out 99.5% of the time.~~ — RETRACTED, see below.**

```
strafe_legs        : 60
strafe_holds       : 10,930
strafe_mean_arc_uu : 209.6
```

AIB10's arc geometry works — a mean arc of 209uu is real lateral movement, and no leg
walks the bot out of its own gate any more.

**RETRACTION (26 Aug, after the cloud's AIB10 instrument commit f36d4a27).** The "182
holds per leg" ratio is INVALID and I should not have reported it as a finding. I wrote
the gate-hold log myself and made it fire EVERY TICK, while the leg line fires ONCE PER
LEG. 10,930:60 therefore divides frames by legs — two different units. It cannot
distinguish a bot denied 95% of its fight from one denied 50% at a higher frame rate, and
the "99.5% gated out" headline does not follow from it at all.

What survives: the arc geometry works (209uu mean arc, no leg leaves the gate), and 60
legs across a five-match sample is genuinely few. Whether that is the gate's fault remains
UNMEASURED. The cloud's replacement instrument treats the hold as a SPELL with edge state
— one line when a visible-target bot leaves the gate, one with the DURATION when it ends,
with a reason — which counts seconds denied instead of frames. That is the right unit, and
pre-instrument logs (this one included) are explicitly not comparable to it. The founder's original report ("the strafe is way
too short") is only half addressed: the STEP is fixed, the OPPORTUNITY is not. AIB10 stays
open with this as its measurement.

**2. `BotTier=Spartan` has never applied to an AIB bot.** That key lives under
`[/Script/BreachpointNext.BNBotController]` — BN's controller, inert while `BotSystem=AIB`.
`[/Script/AIBot.AIBBotController]` had **no tier key at all**, so every AIB bot has run the
C++ default (Marine) since the switch. Nothing was broken — Marine is the shipped default
and config (a) wanted exactly that — but the founder's 25-Aug aggression tuning never
reached these bots, and the two keys are indistinguishable in a diff. Added the key to
AIB's section with the trap written down.

Other counters, run 1: 635 acquisitions, 2314 switches, 58 interrupts, 9 melee swings,
30 grenade throws (6 throttled), 0 claim grants/denies/releases, 14 bots all Marine.

### 2026-08-26 — config (b) Recruit x8 and Spartan x8: the ladder is real

Five matches each, `BotTier` set in `[/Script/AIBot.AIBBotController]` (the key this
ticket had to add — see config (a)'s second finding).

```
RECRUIT x8   latency_mean : mean 0.515  min 0.508  max 0.526  n=5   fastest 0.342s
MARINE       latency_mean : mean 0.382  min 0.380  max 0.386  n=5   fastest 0.221s
SPARTAN x8   latency_mean : mean 0.291  min 0.289  max 0.293  n=5   fastest 0.201s
```

**Distinctness bar: PASS, with better than 2x margin.** Recruit − Spartan =
**0.224s** against a bar of ≥0.10s. Three tiers, monotone, tight within each — Phase 8's
tiers are genuinely wired and genuinely different. The provisional bar needs no
re-proposal; if anything it is generous, and I am NOT proposing to tighten it on one
config's evidence.

All four HARD bars PASS on both tiers. Refusals stay far under the provisional bar
(Recruit 0.04, Spartan 0.12) — a harder bot moves more and refuses more, and both are an
order of magnitude inside 1.0.

#### THE FLOOR IS LOAD-BEARING, and that is the finding

Spartan's fastest acquisition across five matches is **0.201s** against R11's **0.200s**
floor. One millisecond. The floor is not a formality for this tier — it is actively
clamping every fast draw, and it is the only thing keeping Spartan legal.

That matters because this project has already shipped that exact breach once: setting
`BotTier=Spartan` on the BN side put reactions at 0.08–0.16s, under the floor, and it was
caught after landing. The clamp added then is what this measurement now shows holding on
the AIB side, on the first run where an AIB bot has ever actually BEEN Spartan.

Read it as a standing risk, not a solved problem: any change that bypasses the draw-point
clamp, or any tier row authored faster than Spartan, breaches R11 silently. The bar
catches it only because the harness reports the FASTEST acquisition rather than the mean —
Spartan's mean (0.291s) is comfortably legal and would hide it.

**Recruit outlier, noted:** one of the five Recruit matches logged only 300 ambition
switches against a 1628 mean (min 300, max 2348). Not investigated. The other four sit
between ~1900 and 2348, so the mean is dragged by a single short match.
