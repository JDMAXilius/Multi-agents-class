#!/usr/bin/env python3
"""
AIBOT PHASE 9 — the metrics harness (proof 3 made arithmetic).

Feed it one or more PIE/match logs; it counts the module's grep-able instrument lines
and judges them against the bot quality bars. COUNTABLE EVENTS, NEVER IMPRESSIONS —
and never one run: the AIB8/AIB9 lesson (two identical-config matches 39x apart) is
baked in, so single-log runs are stamped NOT-A-BASELINE and multi-log runs report
mean AND spread.

Usage:
    python3 Tools/aib/80_aib_metrics.py Saved/Logs/Match1.log [Match2.log ...]
    ... --json          machine-readable dump instead of the table
    ... --bar KEY=VAL   override a provisional bar (repeatable), e.g. --bar refusals_per_switch=0.8
    ... --baseline F    a prior --json dump (Tools/aib/baselines/) to gate refusals + kills/min against
    ... --selftest      run the inline synthetic log through the parser and assert every derived number

Verbosity note: acquisitions/ambitions/claims/tiers ride LogAIBot at Log verbosity
(default-on). Melee swings, grenade throws and strafe steps are Verbose — add
`-LogCmds="LogAIBot Verbose"` (or `Log LogAIBot Verbose` in console) to count them;
absent that, those rows honestly read "not captured", never 0.
"""

import argparse
import json
import math
import re
import statistics
import sys

# ---------------------------------------------------------------- the vocabulary
# One regex per instrument line, matching the exact formats compiled into the module
# (Plugins/AIBot/Source/AIBot). A format change there MUST break here loudly — that is a feature.
_NUM = r"-?[0-9.]+"
_VEC = r"\(" + _NUM + r",\s*" + _NUM + r",\s*" + _NUM + r"\)"
_AIB = r"AIBot: (?P<bot>\S+) t=(?P<t>" + _NUM + r") "

RX = {
    "acquire":   re.compile(r"AIBot: (?P<bot>\S+) acquired (?P<target>\S+) after (?P<latency>[0-9.]+)s reaction\."),
    "ambition":  re.compile(r"AIBot: (?P<bot>\S+) ambition -> (?P<want>\S+) \((?P<score>[0-9.-]+)\) over (?P<runner>\S+) \((?P<rscore>[0-9.-]+)\)(?P<interrupt> \[interrupt\])?"),
    "tier":      re.compile(r"AIBot: (?P<bot>\S+) resolved tier (?P<tier>\S+) \(Mv (?P<mv>\d) Aim (?P<aim>\d) Gr (?P<gr>\d) Me (?P<me>\d) Cf (?P<cf>\d) Tw (?P<tw>\d), react (?P<rmin>[0-9.]+)-(?P<rmax>[0-9.]+)\)\."),
    "possess":   re.compile(r"AIBot: (?P<bot>\S+) possessed (?P<pawn>\S+), avatar door open\."),
    "claim_grant":   re.compile(r"AIBot: claim GRANTED to (?P<bot>\S+) \(kind (?P<kind>\S+)\)\."),
    "claim_deny":    re.compile(r"AIBot: claim DENIED to (?P<bot>\S+) \(kind (?P<kind>\S+)"),
    "claim_release": re.compile(r"AIBot: claims RELEASED for (?P<bot>\S+) \(life over\)\."),
    "mode_regs": re.compile(r"AIBot: (?P<bot>\S+) registered (?P<count>\d+) mode ambition\(s\) from the provider\."),
    # F7 failure voices — every category the module can speak.
    "f7":        re.compile(r"AIBot: (?P<bot>\S+) (?P<what>cannot path to the last-known spot|cannot path to the objective|cannot reach the last-known spot|cannot reach the objective|flee path REFUSED|flee stalled|wants Retreat with no threat point|won a mode want but)"),
    "unserved":  re.compile(r"AIBot: (?P<bot>\S+) wants '(?P<want>[^']+)' and NO branch serves it"),
    # AIB16's suppression (Verbose): each line is one branch-failure report reaching
    # arbitration. A handful per match = a world problem being coped with; hundreds
    # against few F7s = the holdoff is not holding. 1:1 with failures when healthy.
    "suppressed": re.compile(r"AIBot: (?P<bot>\S+) branch for (?P<want>\S+) failed — suppressing"),
    # Wiring-class warnings: any hit is a finding, whatever the count.
    "wiring":    re.compile(r"AIBot: (?:\S+ )?(dropped a damage-(?:taken|dealt) note|dropped a blast warning|possessed on a non-authority|lost its avatar door|exited a fire state holding|tried to claim a PAWN-backed|asked for unknown tier|BotStateTree '.*' failed to load|RegisterProviders refused|claim refused on a client)"),
    # Verbose-only (counted when captured; reported as absent otherwise).
    "swing":     re.compile(r"AIBot: (?P<bot>\S+) swung at (?P<dist>[0-9.]+)uu"),
    "throw":     re.compile(r"AIBot: (?P<bot>\S+) threw \(call (?P<call>\d)\)"),
    "throttled_throw": re.compile(r"AIBot: (?P<bot>\S+) recognised a grenade moment"),
    "denial_throw":    re.compile(r"AIBot: (?P<bot>\S+) denied the remembered spot"),
    # The terminal's AIB10 strafe instrument (Verbose): executed legs with arc length,
    # and the silent gate holds that were the old "too short" report's whole story.
    # RE-INSTRUMENTED 26 Aug: the held line now fires once per SPELL (edge), not per
    # tick — hold counts from logs before that date are frame counts and must never be
    # compared against these. The opportunity-back line closes each spell with its
    # duration, so denied time is summable.
    "strafe_leg":  re.compile(r"AIBot: (?P<bot>\S+) strafe leg — (?P<arc>[0-9.]+)uu of arc at range (?P<range>[0-9.]+)uu"),
    "strafe_hold": re.compile(r"AIBot: (?P<bot>\S+) strafe held — outside the engaged radius"),
    "strafe_back": re.compile(r"AIBot: (?P<bot>\S+) strafe opportunity back — (?P<seconds>[0-9.]+)s outside \((?P<reason>[^)]+)\)\."),
    # AIB18 (Verbose): the bot raised its sights — the ADS discipline's countable event.
    "ads_in": re.compile(r"AIBot: (?P<bot>\S+) aimed in at (?P<range>[0-9.]+)uu\."),
    # AIB17 (Verbose): a teammate's gunfire heard (once per spell) and an idle wander
    # leg biased toward it — the convergence behavior's two countable halves.
    "ally_heard":      re.compile(r"AIBot: (?P<bot>\S+) heard the team's fight — (?P<dist>[0-9.]+)uu away\."),
    "wander_to_fight": re.compile(r"AIBot: (?P<bot>\S+) wandering toward the team's fight\."),
    # AIB9 step 2/3 (rides every self=NO refusal): WHERE the off-mesh bot is and WHAT
    # MOMENT it is in. age < 2s reads as a spawn problem, falling=yes as mid-air (the
    # projector cannot land a body in flight), a fresh lastHit as knockback, and none
    # of the above as geometry the nav bounds miss.
    "offmesh_self": re.compile(
        r"off-mesh self at \((?P<x>-?[0-9.]+), (?P<y>-?[0-9.]+), (?P<z>-?[0-9.]+)\) "
        r"age=(?P<age>[0-9.]+)s falling=(?P<falling>yes|no) velZ=(?P<velz>-?[0-9.]+) "
        r"lastHit=(?P<lasthit>[0-9.]+s|never)"),
    # BN15 teams (LogBN, not the module's log): the three countable events the roadmap's
    # proofs rest on. Formats transcribed from BNGameMode.cpp (assignment, team-kill
    # denial) and BNDamage.cpp (the FF gate's Verbose refusal — add -LogCmds="LogBN
    # Verbose" to capture it). All zero in an FFA log, which IS the OFF-regression check.
    "team_assign":      re.compile(r"BNGameMode: (?P<player>.+) assigned to team (?P<team>\d+)\."),
    "ff_refused":       re.compile(r"BNDamage: friendly fire refused — (?P<attacker>.+) -> (?P<victim>.+) \((?P<damage>[0-9.]+)\)\."),
    "team_kill_denied": re.compile(r"BNGameMode: team kill — (?P<killer>.+) -> (?P<victim>.+), no credit\."),
    # BN20: a press that reached the ASC and found no spec, now carrying its separating
    # facts (dead = the corpse window, alive = a grant race). The done-when bar is this
    # count at ~0 across five matches.
    "no_grant": re.compile(r"BNASC: input tag (?P<tag>\S+) reached the ASC but NO granted ability carries it[^.]*\. dead=(?P<dead>yes|no) avatar=(?P<avatar>\S+)"),
    # AIB22 step 1 (W-AUDIT member 3 spec): the five egress metrics, each carrying t= World
    # seconds so idle/stall/sweep sum to SECONDS. The dash is U+2014 in the module; ASCII
    # '-' accepted so a hand-typed or transcoded log still parses.
    "move_refused":  re.compile(_AIB + r"move REFUSED goal=" + _VEC + r"(?P<text>.*)"),
    "stall_over":    re.compile(_AIB + r"stall over [—-] (?P<seconds>" + _NUM + r")s at " + _VEC + r" goal=" + _VEC
                                + r" jumped=(?P<jumped>yes|no) resolved=(?P<resolved>moved|abandoned)"),
    "sweep_over":    re.compile(_AIB + r"sweep over [—-] (?P<seconds>" + _NUM + r")s, moved (?P<moved>" + _NUM + r")uu, state=(?P<state>Search|Roam)"),
    "idle_over":     re.compile(_AIB + r"idle over [—-] (?P<seconds>" + _NUM + r")s state=(?P<state>\S+) tactic=(?P<tactic>Hold|Reload|StrafeHold|none)"),
    "island_egress": re.compile(_AIB + r"island egress [—-] via (?P<via>drop|link|jump|grapple) from " + _VEC + r" after (?P<seconds>" + _NUM + r")s stranded"),
    # AIB23 (Phase 12): the target-claim board and the team report, each on the t= prefix.
    # GRANTED/RELEASED bound a holder's live interval (pile-up); DENIED's arrow names what
    # the refused bot did instead; the report is the shared-belief broadcast.
    "target_grant":   re.compile(_AIB + r"target claim GRANTED on (?P<target>\S+) \((?P<k>\d+)/(?P<cap>\d+)\)"),
    "target_deny":    re.compile(_AIB + r"target claim DENIED on (?P<target>\S+) \((?P<k>\d+)/(?P<cap>\d+)\) -> (?P<then>\S+)"),
    "target_release": re.compile(_AIB + r"target claim RELEASED on (?P<target>\S+) reason=(?P<reason>ttl|exit|death|unpossess)"),
    "team_report":    re.compile(_AIB + r"team report (?P<target>\S+) at " + _VEC + r" seen_t=(?P<seen_t>" + _NUM + r") from (?P<ally>\S+)"),
    # Kills/min inputs (LogBN, formats from BNGameMode.cpp): one line per credited kill,
    # and the travel-URL TimeLimit that the headless protocol always passes.
    "kill":       re.compile(r"BNGameMode: (?P<killer>.+) eliminated (?P<victim>.+) with '(?P<source>[^']*)'\. \(.+: (?P<kills>\d+) kills\)"),
    "time_limit": re.compile(r"BNGameMode: TimeLimit=(?P<seconds>\d+)s from the travel URL\."),
}

# The per-bot metrics derived from the five AIB22 lines (seconds are sums per bot per match).
BOT_METRICS = ("no_path_requests", "stuck_seconds", "max_stall_seconds", "sweep_seconds",
               "idle_seconds", "idle_seconds_tactical", "island_egress_count",
               # AIB23: ttl-release -> re-grant on the same target inside THRASH_WINDOW_SECONDS,
               # and what a DENIED bot did instead.
               "claim_thrash", "denied_roam", "denied_engage_anyway")
THRASH_WINDOW_SECONDS = 6.0
CLAIM_TTL_SECONDS = 5.0   # AIB23 module constant (no ini lookup); --ttl overrides

# F1's floor is a module constant (AIB::MinReactionSeconds). Restated here as a
# checked number: if the module's floor moves, this bar must be moved WITH it.
F1_FLOOR_SECONDS = 0.20

# ------------------------------------------------------- provisional quality bars
# PROVISIONAL until the founder tunes them against real five-match baselines — every
# judgment line below is stamped with the bar it used so the table argues its case.
DEFAULT_BARS = {
    # Hard fairness bar (F1): no acquisition may beat the module floor. Not tunable
    # downward in good faith — listed so a violation prints the number that broke it.
    "min_reaction_floor": F1_FLOOR_SECONDS,
    # Mean move-refusals per ambition switch, lobby-wide (AIB9's instrument). The
    # measured healthy spread at JumpLength 400 was 0.04..1.67 across matches.
    "refusals_per_switch": 1.0,
    # Unserved-want warnings per match (a want no branch serves is an authoring bug).
    "unserved_wants": 0,
    # Wiring-class warnings per match.
    "wiring_warnings": 0,
    # FFA: the claims board must stay dormant (grants per match).
    "ffa_claim_grants": 0,
    # Baseline discipline: below this many logs, no PASS/FAIL is issued at all.
    "min_logs_for_baseline": 5,
    # AIB22 egress gates (W-AUDIT member 3). idle/sweep are HARD; the stall pair PROVISIONAL.
    "idle_seconds": 0.0,            # per bot per match, tactic=none only
    "sweep_seconds": 0.0,           # per bot per match
    "stuck_seconds_per_bot": 10.0,  # sum of stall-over seconds, worst bot
    "max_stall_seconds": 3.0,       # longest single stall, any bot
    "refusal_ratio_vs_baseline": 0.5,  # median no_path_requests per bot <= this x baseline median
    # AIB23 target-claim gates: pile-up HARD, thrash PROVISIONAL.
    "target_pileup_count": 0,     # 1-s buckets with more holders than cap, per match
    "claim_thrash_per_bot": 2,    # ttl release -> re-grant within the window, worst bot
}


def parse_lines(lines):
    counts = {key: [] for key in RX}
    for line in lines:
        for key, rx in RX.items():
            match = rx.search(line)
            if match:
                counts[key].append(match.groupdict())
    return counts


def parse_log(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return parse_lines(handle)


def per_bot_summary(counts):
    """AIB22: the five metrics per bot. Every possessed/tiered bot gets a row (zeros count
    in the medians); bots seen only on the five lines are added as they appear."""
    bots = {}

    def row(name):
        return bots.setdefault(name, {key: 0 for key in BOT_METRICS})

    for key in ("possess", "tier", "target_grant"):
        for hit in counts[key]:
            row(hit["bot"])
    for hit in counts["move_refused"]:
        row(hit["bot"])["no_path_requests"] += 1
    for hit in counts["stall_over"]:
        seconds = float(hit["seconds"])
        bot = row(hit["bot"])
        bot["stuck_seconds"] += seconds
        bot["max_stall_seconds"] = max(bot["max_stall_seconds"], seconds)
    for hit in counts["sweep_over"]:
        row(hit["bot"])["sweep_seconds"] += float(hit["seconds"])
    for hit in counts["idle_over"]:
        key = "idle_seconds" if hit["tactic"] == "none" else "idle_seconds_tactical"
        row(hit["bot"])[key] += float(hit["seconds"])
    for hit in counts["island_egress"]:
        row(hit["bot"])["island_egress_count"] += 1
    for hit in counts["target_deny"]:
        if hit["then"] == "roam":
            row(hit["bot"])["denied_roam"] += 1
        elif hit["then"] == "engage-anyway":
            row(hit["bot"])["denied_engage_anyway"] += 1
    # Thrash: each reason=ttl release counts once if the same bot re-takes the same
    # target within the window (0 < dt <= window).
    grants = {}
    for hit in counts["target_grant"]:
        grants.setdefault((hit["bot"], hit["target"]), []).append(float(hit["t"]))
    for hit in counts["target_release"]:
        if hit["reason"] != "ttl":
            continue
        released = float(hit["t"])
        if any(0 < t - released <= THRASH_WINDOW_SECONDS for t in grants.get((hit["bot"], hit["target"]), ())):
            row(hit["bot"])["claim_thrash"] += 1
    for bot in bots.values():
        for key in BOT_METRICS:
            if isinstance(bot[key], float):
                bot[key] = round(bot[key], 3)
    return dict(sorted(bots.items()))


def target_pileups(counts, ttl):
    """AIB23: count 1-second buckets in which more bots hold a live claim on one target
    than the cap allows. A claim lives from GRANT until RELEASE or last GRANT + ttl,
    whichever is first; a repeat GRANT by the same holder renews the expiry. Cap is the
    `(k/cap)` field of the last GRANT seen, 2 if none.
    ponytail: grouped by target only — in 4v4 one target is enemy to exactly one
    alliance, so holders of one target are always one team; key on (team, target) if
    FFA ever runs the claim board."""
    events = sorted([(float(h["t"]), "grant", h["bot"], h["target"], int(h["cap"])) for h in counts["target_grant"]]
                    + [(float(h["t"]), "release", h["bot"], h["target"], None) for h in counts["target_release"]],
                    key=lambda e: e[0])  # stable: same-t lines keep log order
    live, closed, cap = {}, [], None      # live[(target, bot)] = [start, expiry]
    for t, kind, bot, target, line_cap in events:
        key = (target, bot)
        if kind == "grant":
            cap = line_cap
            if key in live and live[key][1] > t:
                live[key][1] = t + ttl
                continue
            if key in live:
                closed.append((target, *live.pop(key)))
            live[key] = [t, t + ttl]
        elif key in live:
            start, expiry = live.pop(key)
            closed.append((target, start, min(t, expiry)))
    closed += [(target, start, expiry) for (target, _), (start, expiry) in live.items()]
    cap = cap or 2
    holders = {}
    for target, start, end in closed:          # [start, end) covers bucket b iff start < b+1 and end > b
        for bucket in range(math.floor(start), math.ceil(end)):
            holders[(target, bucket)] = holders.get((target, bucket), 0) + 1
    return sum(1 for n in holders.values() if n > cap), cap


def per_match_summary(counts, ttl=CLAIM_TTL_SECONDS):
    latencies = [float(hit["latency"]) for hit in counts["acquire"]]
    switches = len(counts["ambition"])
    interrupts = sum(1 for hit in counts["ambition"] if hit.get("interrupt"))
    tiers = {}
    for hit in counts["tier"]:
        tiers.setdefault(hit["tier"], set()).add(hit["bot"])
    bots = per_bot_summary(counts)
    # ponytail: match length = the URL TimeLimit, else the last t= seen on an AIB22 line.
    # A score-limit early end overstates minutes; add a "match over" t= line if that matters.
    t_seen = [float(hit["t"]) for key in ("move_refused", "stall_over", "sweep_over", "idle_over", "island_egress",
                                          "target_grant", "target_deny", "target_release", "team_report")
              for hit in counts[key]]
    match_seconds = (float(counts["time_limit"][-1]["seconds"]) if counts["time_limit"]
                     else max(t_seen) if t_seen else None)
    pileups, cap = target_pileups(counts, ttl)
    return {
        "acquisitions": len(latencies),
        "latency_mean": statistics.mean(latencies) if latencies else None,
        "latency_median": statistics.median(latencies) if latencies else None,
        "latency_min": min(latencies) if latencies else None,
        "ambition_switches": switches,
        "interrupts": interrupts,
        "f7_failures": len(counts["f7"]),
        "refusals_per_switch": (len(counts["f7"]) / switches) if switches else None,
        "unserved_wants": len(counts["unserved"]),
        "suppressed_wants": len(counts["suppressed"]) or None,  # Verbose-only
        "wiring_warnings": len(counts["wiring"]),
        "claim_grants": len(counts["claim_grant"]),
        "claim_denies": len(counts["claim_deny"]),
        "claim_releases": len(counts["claim_release"]),
        # AIB23 target claims — distinct from the Phase 7 slot-claim keys above (those feed the FFA gate).
        "target_claim_grants": len(counts["target_grant"]),
        "target_claim_denies": len(counts["target_deny"]),
        "target_claim_releases": len(counts["target_release"]),
        "team_reports": len(counts["team_report"]),
        "target_pileup_count": pileups,
        "claim_ttl": ttl,
        "claim_cap": cap,
        "tiers": {tier: sorted(bots) for tier, bots in tiers.items()},
        "swings": len(counts["swing"]) or None,          # None = likely not captured
        "throws": len(counts["throw"]) or None,
        "throttled_throws": len(counts["throttled_throw"]) or None,
        "denial_throws": len(counts["denial_throw"]) or None,
        "strafe_legs": len(counts["strafe_leg"]) or None,
        "strafe_holds": len(counts["strafe_hold"]) or None,   # SPELLS post-26-Aug, frames before
        "strafe_mean_arc_uu": (statistics.mean(float(hit["arc"]) for hit in counts["strafe_leg"])
            if counts["strafe_leg"] else None),
        # AIB10 opportunity, honestly: total seconds a strafe-capable bot stood denied by
        # the gate, and how each spell ended. The decision number is denied seconds vs
        # stepped legs — not the old frames-over-legs ratio.
        "strafe_denied_seconds": (round(sum(float(hit["seconds"]) for hit in counts["strafe_back"]), 1)
            if counts["strafe_back"] else None),
        "ads_ins": len(counts["ads_in"]) or None,        # Verbose-only
        "ads_mean_range_uu": (round(statistics.mean(float(hit["range"]) for hit in counts["ads_in"]), 0)
            if counts["ads_in"] else None),
        "ally_fights_heard": len(counts["ally_heard"]) or None,        # Verbose-only
        "wanders_to_fight": len(counts["wander_to_fight"]) or None,    # Verbose-only
        "strafe_spell_ends": ({reason: sum(1 for hit in counts["strafe_back"] if hit["reason"] == reason)
            for reason in sorted({hit["reason"] for hit in counts["strafe_back"]})}
            if counts["strafe_back"] else None),
        # AIB9: off-mesh moments, pre-bucketed by the candidate causes the ticket names.
        # The buckets overlap on purpose (a falling fresh spawn is both) — they are
        # correlates to weigh, not a partition to sum.
        "offmesh_self": len(counts["offmesh_self"]) or None,
        "offmesh_moments": ({
            "fresh_spawn_lt2s": sum(1 for hit in counts["offmesh_self"] if float(hit["age"]) < 2.0),
            "falling": sum(1 for hit in counts["offmesh_self"] if hit["falling"] == "yes"),
            "hit_within_1s": sum(1 for hit in counts["offmesh_self"]
                if hit["lasthit"] != "never" and float(hit["lasthit"].rstrip("s")) < 1.0),
        } if counts["offmesh_self"] else None),
        # BN15 teams. team_populations proves the 4/4 balance claim from the assignment
        # lines alone; all three stay None in an FFA log (the OFF gate reads that as PASS).
        "team_assignments": len(counts["team_assign"]) or None,
        "team_populations": ({team: sum(1 for hit in counts["team_assign"] if hit["team"] == team)
            for team in sorted({hit["team"] for hit in counts["team_assign"]})}
            if counts["team_assign"] else None),
        "ff_refused": len(counts["ff_refused"]) or None,     # Verbose-only
        "team_kills_denied": len(counts["team_kill_denied"]) or None,
        # BN20: no-grant presses split by the dead flag (corpse window vs grant race)
        # and by tag, so the 308 stops being one anonymous number.
        "no_grant_presses": len(counts["no_grant"]) or None,
        "no_grant_split": ({
            "dead": sum(1 for hit in counts["no_grant"] if hit["dead"] == "yes"),
            "alive": sum(1 for hit in counts["no_grant"] if hit["dead"] == "no"),
            "by_tag": {tag: sum(1 for hit in counts["no_grant"] if hit["tag"] == tag)
                for tag in sorted({hit["tag"] for hit in counts["no_grant"]})},
        } if counts["no_grant"] else None),
        # AIB22: per-bot egress metrics, their across-bot spread, and lobby kills/min.
        "per_bot": bots,
        "bot_spread": {key: spread([bot[key] for bot in bots.values()]) for key in BOT_METRICS},
        "kills": len(counts["kill"]),
        "match_seconds": match_seconds,
        "kills_per_min": (round(len(counts["kill"]) / (match_seconds / 60.0), 3) if match_seconds else None),
    }


# Lobby-wide keys whose across-match spread (mean/median/min/max) is reported and dumped;
# the BOT_METRICS ride as the median-across-bots of each match.
LOBBY_KEYS = ("latency_mean", "refusals_per_switch", "ambition_switches", "kills_per_min") + BOT_METRICS


def lobby_spread(matches):
    out = {}
    for key in LOBBY_KEYS:
        if key in BOT_METRICS:
            values = [(m["bot_spread"][key] or {}).get("median") for m in matches]
        else:
            values = [m[key] for m in matches]
        out[key] = spread(values)
    return out


def spread(values):
    real = [value for value in values if value is not None]
    if not real:
        return None
    return {
        "mean": statistics.mean(real),
        "median": statistics.median(real),
        "min": min(real),
        "max": max(real),
        "n": len(real),
    }


def judge(matches, bars, baseline=None):
    verdicts = []

    def bar_line(name, ok, detail):
        verdicts.append(("PASS" if ok else "FAIL", name, detail))

    floors = [m["latency_min"] for m in matches if m["latency_min"] is not None]
    if floors:
        worst = min(floors)
        bar_line("F1 reaction floor", worst >= bars["min_reaction_floor"] - 0.0005,
                 f"fastest acquisition {worst:.3f}s vs floor {bars['min_reaction_floor']:.2f}s (HARD bar)")

    for name, key, bar_key, kind in (("unserved wants", "unserved_wants", "unserved_wants", ""),
                                     ("wiring warnings", "wiring_warnings", "wiring_warnings", ""),
                                     ("FFA claim grants", "claim_grants", "ffa_claim_grants", ""),
                                     ("target pile-up buckets", "target_pileup_count", "target_pileup_count", "HARD ")):
        worst = max(m[key] for m in matches)
        bar_line(name, worst <= bars[bar_key],
                 f"worst match {worst} vs {kind}bar {bars[bar_key]} (per match)")

    rps = spread([m["refusals_per_switch"] for m in matches])
    if rps:
        bar_line("move refusals per switch", rps["mean"] <= bars["refusals_per_switch"],
                 f"mean {rps['mean']:.2f} (spread {rps['min']:.2f}..{rps['max']:.2f}, n={rps['n']}) "
                 f"vs PROVISIONAL bar {bars['refusals_per_switch']}")

    # AIB22 gates — worst bot across every match, stamped HARD or PROVISIONAL.
    def worst_bot(key):
        rows = [(bot[key], name, i + 1) for i, m in enumerate(matches) for name, bot in m["per_bot"].items()]
        return max(rows) if rows else None

    for name, key, bar_key, kind in (("idle seconds (tactic=none)", "idle_seconds", "idle_seconds", "HARD"),
                                     ("sweep seconds", "sweep_seconds", "sweep_seconds", "HARD"),
                                     ("stuck seconds per bot", "stuck_seconds", "stuck_seconds_per_bot", "PROVISIONAL"),
                                     ("longest single stall", "max_stall_seconds", "max_stall_seconds", "PROVISIONAL"),
                                     ("claim thrash per bot", "claim_thrash", "claim_thrash_per_bot", "PROVISIONAL")):
        worst = worst_bot(key)
        if worst is None:
            continue
        value, bot, match_no = worst
        bar_line(name, value <= bars[bar_key],
                 f"worst {value} ({bot}, log {match_no}) vs {kind} bar {bars[bar_key]} (per bot per match)")

    if baseline is not None:
        current = lobby_spread(matches)
        base = baseline["lobby_spread"]
        cur_ref, base_ref = current["no_path_requests"], base["no_path_requests"]
        if cur_ref and base_ref:
            ceiling = bars["refusal_ratio_vs_baseline"] * base_ref["median"]
            bar_line("move refusals vs baseline", cur_ref["median"] <= ceiling,
                     f"median per-bot no_path_requests {cur_ref['median']:.2f} vs {bars['refusal_ratio_vs_baseline']} x "
                     f"baseline median {base_ref['median']:.2f} = {ceiling:.2f} (n={cur_ref['n']} vs baseline n={base_ref['n']})")
        cur_kpm, base_kpm = current["kills_per_min"], base["kills_per_min"]
        if cur_kpm and base_kpm:
            floor = base_kpm["median"] - (base_kpm["max"] - base_kpm["min"])
            bar_line("kills/min vs baseline", cur_kpm["median"] >= floor,
                     f"median {cur_kpm['median']:.3f} vs baseline median {base_kpm['median']:.3f} - spread "
                     f"{base_kpm['max'] - base_kpm['min']:.3f} = floor {floor:.3f}")
    return verdicts


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("logs", nargs="*", help="one or more UE log files")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--bar", action="append", default=[], metavar="KEY=VAL")
    parser.add_argument("--baseline", metavar="JSON", help="a prior --json dump to gate refusals and kills/min against")
    parser.add_argument("--ttl", type=float, default=CLAIM_TTL_SECONDS,
                        help="AIB23 target-claim TTL seconds (module constant; expiry when no RELEASE is logged)")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    if not args.logs:
        parser.error("no logs given (or use --selftest)")

    bars = dict(DEFAULT_BARS)
    for override in args.bar:
        key, _, raw = override.partition("=")
        if key not in bars:
            sys.exit(f"unknown bar '{key}' — known: {', '.join(sorted(bars))}")
        bars[key] = type(bars[key])(float(raw)) if isinstance(bars[key], float) else int(raw)

    reference = None
    if args.baseline:
        with open(args.baseline, "r", encoding="utf-8") as handle:
            reference = json.load(handle)
        if "lobby_spread" not in reference:
            sys.exit(f"{args.baseline} has no 'lobby_spread' — baselines are this script's own --json output")

    matches = [per_match_summary(parse_log(path), args.ttl) for path in args.logs]
    baseline = len(matches) >= bars["min_logs_for_baseline"]
    verdicts = judge(matches, bars, reference)

    if args.json:
        print(json.dumps({"logs": args.logs, "matches": matches, "lobby_spread": lobby_spread(matches),
                          "verdicts": verdicts, "baseline": baseline}, indent=2))
        return

    print("=== AIBot metrics ===")
    for path, match in zip(args.logs, matches):
        print(f"\n-- {path}")
        for key, value in match.items():
            if key in ("per_bot", "bot_spread"):
                continue
            if value is None:
                value = ("not captured (Verbose off?)"
                    if key in ("swings", "throws", "throttled_throws", "denial_throws",
                        "strafe_legs", "strafe_holds", "strafe_mean_arc_uu",
                        "strafe_denied_seconds", "strafe_spell_ends", "ff_refused",
                        "offmesh_self", "offmesh_moments", "ads_ins", "ads_mean_range_uu",
                        "ally_fights_heard", "wanders_to_fight")
                    else "none (FFA?)" if key in ("team_assignments", "team_populations",
                        "team_kills_denied")
                    else "n/a")
            print(f"   {key:22}: {value}")
        if match["per_bot"]:
            print(f"   {'per bot':22}: " + " ".join(f"{key[:9]:>9}" for key in BOT_METRICS))
            for bot, row in match["per_bot"].items():
                print(f"     {bot[:20]:20}: " + " ".join(f"{row[key]:>9}" for key in BOT_METRICS))

    print("\n=== lobby spread (across logs; bot metrics = median across bots per log) ===")
    for key, agg in lobby_spread(matches).items():
        if agg:
            print(f"   {key:22}: mean {agg['mean']:.3f}  median {agg['median']:.3f}  min {agg['min']:.3f}  max {agg['max']:.3f}  n={agg['n']}")

    print("\n=== bars ===")
    if not baseline:
        print(f"   NOT A BASELINE: {len(matches)} log(s) < {bars['min_logs_for_baseline']} — "
              f"the AIB8 lesson (identical configs measured 39x apart). Verdicts below are "
              f"indicative only; do not paste them as a claim.")
    for verdict, name, detail in verdicts:
        print(f"   [{verdict}] {name}: {detail}")


# ------------------------------------------------------------------- self-test
# Twenty synthetic lines in the exact module formats (one ASCII dash on purpose), with
# every derived number asserted. `--selftest` is the check that the regexes still match
# the spec; a module format change must break THIS before it silently zeroes a baseline.
SELFTEST_LOG = """\
AIBot: Alpha possessed BP_Pawn_1, avatar door open.
AIBot: Bravo possessed BP_Pawn_2, avatar door open.
BNGameMode: TimeLimit=300s from the travel URL.
AIBot: Alpha t=12.5 move REFUSED goal=(100,200,50) no path to goal
AIBot: Alpha t=13.0 move REFUSED goal=(100,200,50) belief target off-mesh
AIBot: Bravo t=14.25 move REFUSED goal=(-1,-2.5,3) projection failed
AIBot: Alpha t=20.0 stall over — 2.5s at (1,2,3) goal=(4,5,6) jumped=yes resolved=moved
AIBot: Alpha t=40.0 stall over - 4.0s at (1,2,3) goal=(4,5,6) jumped=no resolved=abandoned
AIBot: Bravo t=41.0 stall over — 1.5s at (0,0,0) goal=(9,9,9) jumped=no resolved=moved
AIBot: Alpha t=50.0 sweep over — 3.0s, moved 120.5uu, state=Search
AIBot: Bravo t=51.0 sweep over — 1.25s, moved 0uu, state=Roam
AIBot: Alpha t=60.0 idle over — 2.0s state=Roam tactic=none
AIBot: Alpha t=70.0 idle over — 1.5s state=Engage tactic=Hold
AIBot: Bravo t=71.0 idle over — 0.5s state=Engage tactic=Reload
AIBot: Bravo t=72.0 idle over — 3.0s state=Search tactic=none
AIBot: Alpha t=80.0 island egress — via drop from (10,20,30) after 4.5s stranded
AIBot: Bravo t=81.0 island egress — via grapple from (10,20,30) after 2.0s stranded
BNGameMode: Alpha eliminated Bravo with 'Rifle'. (Alpha: 1 kills)
BNGameMode: Bravo eliminated Alpha with 'Melee'. (Bravo: 1 kills)
AIBot: Alpha acquired Bravo after 0.35s reaction.
"""

# AIB23: cap 2, ttl 5. Alpha/Bravo hold Enemy1; Charlie is denied twice, then GRANTED at
# 104.5 while both still hold -> bucket 104 has three holders (the one pile-up). Alpha's
# ttl release at 105 then re-grant at 108 (dt 3) is the one thrash; Bravo's death release
# and re-grant at 110 is neither. One team report.
AIB23_LOG = """\
AIBot: Alpha t=100.0 target claim GRANTED on Enemy1 (1/2)
AIBot: Bravo t=100.5 target claim GRANTED on Enemy1 (2/2)
AIBot: Charlie t=101.0 target claim DENIED on Enemy1 (2/2) -> roam
AIBot: Charlie t=102.0 target claim DENIED on Enemy1 (2/2) -> engage-anyway
AIBot: Charlie t=104.5 target claim GRANTED on Enemy1 (3/2)
AIBot: Alpha t=105.0 target claim RELEASED on Enemy1 reason=ttl
AIBot: Bravo t=106.0 target claim RELEASED on Enemy1 reason=death
AIBot: Alpha t=108.0 target claim GRANTED on Enemy1 (2/2)
AIBot: Charlie t=109.0 target claim RELEASED on Enemy1 reason=exit
AIBot: Bravo t=110.0 target claim GRANTED on Enemy1 (2/2)
AIBot: Delta t=110.5 team report Enemy1 at (10,20,30) seen_t=109.8 from Bravo
"""

AIB23_CLEAN_LOG = """\
AIBot: Alpha t=100.0 target claim GRANTED on Enemy1 (1/2)
AIBot: Alpha t=105.0 target claim RELEASED on Enemy1 reason=ttl
AIBot: Alpha t=112.0 target claim GRANTED on Enemy1 (1/2)
"""


def selftest():
    lines = SELFTEST_LOG.splitlines()
    assert len(lines) == 20, len(lines)
    counts = parse_lines(lines)
    hits = {key: len(counts[key]) for key in ("move_refused", "stall_over", "sweep_over", "idle_over",
                                              "island_egress", "kill", "time_limit", "possess", "acquire", "f7")}
    assert hits == {"move_refused": 3, "stall_over": 3, "sweep_over": 2, "idle_over": 4, "island_egress": 2,
                    "kill": 2, "time_limit": 1, "possess": 2, "acquire": 1, "f7": 0}, hits
    assert counts["stall_over"][1]["resolved"] == "abandoned" and counts["stall_over"][0]["jumped"] == "yes"
    assert counts["island_egress"][1]["via"] == "grapple" and counts["sweep_over"][0]["moved"] == "120.5"
    assert counts["move_refused"][0]["text"].strip() == "no path to goal"

    match = per_match_summary(counts)
    expect = {
        "Alpha": {"no_path_requests": 2, "stuck_seconds": 6.5, "max_stall_seconds": 4.0, "sweep_seconds": 3.0,
                  "idle_seconds": 2.0, "idle_seconds_tactical": 1.5, "island_egress_count": 1,
                  "claim_thrash": 0, "denied_roam": 0, "denied_engage_anyway": 0},
        "Bravo": {"no_path_requests": 1, "stuck_seconds": 1.5, "max_stall_seconds": 1.5, "sweep_seconds": 1.25,
                  "idle_seconds": 3.0, "idle_seconds_tactical": 0.5, "island_egress_count": 1,
                  "claim_thrash": 0, "denied_roam": 0, "denied_engage_anyway": 0},
    }
    assert match["per_bot"] == expect, match["per_bot"]
    assert match["kills"] == 2 and match["match_seconds"] == 300.0 and match["kills_per_min"] == 0.4, match
    assert match["bot_spread"]["no_path_requests"]["median"] == 1.5
    assert match["bot_spread"]["stuck_seconds"] == {"mean": 4.0, "median": 4.0, "min": 1.5, "max": 6.5, "n": 2}
    assert match["f7_failures"] == 0 and match["acquisitions"] == 1

    lobby = lobby_spread([match, match])
    assert lobby["kills_per_min"]["median"] == 0.4 and lobby["idle_seconds"]["median"] == 2.5, lobby

    verdicts = {name: verdict for verdict, name, _ in judge([match], DEFAULT_BARS, {"lobby_spread": lobby})}
    assert verdicts == {
        "F1 reaction floor": "PASS", "unserved wants": "PASS", "wiring warnings": "PASS", "FFA claim grants": "PASS",
        "idle seconds (tactic=none)": "FAIL", "sweep seconds": "FAIL",
        "stuck seconds per bot": "PASS", "longest single stall": "FAIL",
        "target pile-up buckets": "PASS", "claim thrash per bot": "PASS",
        "move refusals vs baseline": "FAIL",   # 1.5 > 0.5 x 1.5
        "kills/min vs baseline": "PASS",       # 0.4 >= 0.4 - 0
    }, verdicts
    assert not any("baseline" in name for _, name, _ in judge([match], DEFAULT_BARS))  # no baseline -> no baseline gates

    # AIB23 target claims.
    c23 = parse_lines(AIB23_LOG.splitlines())
    assert len(AIB23_LOG.splitlines()) == 11
    assert (len(c23["target_grant"]), len(c23["target_deny"]), len(c23["target_release"]), len(c23["team_report"])) == (5, 2, 3, 1)
    assert c23["team_report"][0]["ally"] == "Bravo" and c23["team_report"][0]["seen_t"] == "109.8"
    m23 = per_match_summary(c23)
    assert m23["target_pileup_count"] == 1 and m23["claim_cap"] == 2 and m23["claim_ttl"] == 5.0, m23
    assert m23["team_reports"] == 1 and m23["target_claim_grants"] == 5 and m23["match_seconds"] == 110.5, m23
    assert m23["per_bot"]["Alpha"]["claim_thrash"] == 1 and m23["per_bot"]["Bravo"]["claim_thrash"] == 0, m23["per_bot"]
    assert m23["per_bot"]["Charlie"]["denied_roam"] == 1 and m23["per_bot"]["Charlie"]["denied_engage_anyway"] == 1
    assert m23["claim_grants"] == 0  # Phase 7 slot-claim keys untouched by target claims
    v23 = {name: verdict for verdict, name, _ in judge([m23], DEFAULT_BARS)}
    assert v23["target pile-up buckets"] == "FAIL" and v23["claim thrash per bot"] == "PASS", v23
    clean = per_match_summary(parse_lines(AIB23_CLEAN_LOG.splitlines()))
    assert clean["target_pileup_count"] == 0 and clean["per_bot"]["Alpha"]["claim_thrash"] == 0, clean

    for line in judge([match], DEFAULT_BARS, {"lobby_spread": lobby}):
        print(f"   [{line[0]}] {line[1]}: {line[2]}")
    print(f"SELFTEST PASS: 20 lines, {sum(hits.values())} hits, per-bot {json.dumps(match['per_bot'])}, "
          f"kills/min {match['kills_per_min']}")
    print(f"SELFTEST PASS (AIB23): 11 lines, pileup {m23['target_pileup_count']}, "
          f"thrash Alpha {m23['per_bot']['Alpha']['claim_thrash']}, Charlie denied roam/engage-anyway "
          f"{m23['per_bot']['Charlie']['denied_roam']}/{m23['per_bot']['Charlie']['denied_engage_anyway']}, "
          f"team_reports {m23['team_reports']}; clean pileup {clean['target_pileup_count']} thrash "
          f"{clean['per_bot']['Alpha']['claim_thrash']}")


if __name__ == "__main__":
    main()
