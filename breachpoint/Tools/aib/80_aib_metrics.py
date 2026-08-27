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

Verbosity note: acquisitions/ambitions/claims/tiers ride LogAIBot at Log verbosity
(default-on). Melee swings, grenade throws and strafe steps are Verbose — add
`-LogCmds="LogAIBot Verbose"` (or `Log LogAIBot Verbose` in console) to count them;
absent that, those rows honestly read "not captured", never 0.
"""

import argparse
import json
import re
import statistics
import sys

# ---------------------------------------------------------------- the vocabulary
# One regex per instrument line, matching the exact formats compiled into the module
# (Plugins/AIBot/Source/AIBot). A format change there MUST break here loudly — that is a feature.
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
}

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
}


def parse_log(path):
    counts = {key: [] for key in RX}
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            for key, rx in RX.items():
                match = rx.search(line)
                if match:
                    counts[key].append(match.groupdict())
    return counts


def per_match_summary(counts):
    latencies = [float(hit["latency"]) for hit in counts["acquire"]]
    switches = len(counts["ambition"])
    interrupts = sum(1 for hit in counts["ambition"] if hit.get("interrupt"))
    tiers = {}
    for hit in counts["tier"]:
        tiers.setdefault(hit["tier"], set()).add(hit["bot"])
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
    }


def spread(values):
    real = [value for value in values if value is not None]
    if not real:
        return None
    return {
        "mean": statistics.mean(real),
        "min": min(real),
        "max": max(real),
        "n": len(real),
    }


def judge(matches, bars):
    verdicts = []

    def bar_line(name, ok, detail):
        verdicts.append(("PASS" if ok else "FAIL", name, detail))

    floors = [m["latency_min"] for m in matches if m["latency_min"] is not None]
    if floors:
        worst = min(floors)
        bar_line("F1 reaction floor", worst >= bars["min_reaction_floor"] - 0.0005,
                 f"fastest acquisition {worst:.3f}s vs floor {bars['min_reaction_floor']:.2f}s (HARD bar)")

    for name, key in (("unserved wants", "unserved_wants"),
                      ("wiring warnings", "wiring_warnings"),
                      ("FFA claim grants", "claim_grants")):
        bar_key = "ffa_claim_grants" if key == "claim_grants" else key
        worst = max(m[key] for m in matches)
        bar_line(name, worst <= bars[bar_key],
                 f"worst match {worst} vs bar {bars[bar_key]} (per match)")

    rps = spread([m["refusals_per_switch"] for m in matches])
    if rps:
        bar_line("move refusals per switch", rps["mean"] <= bars["refusals_per_switch"],
                 f"mean {rps['mean']:.2f} (spread {rps['min']:.2f}..{rps['max']:.2f}, n={rps['n']}) "
                 f"vs PROVISIONAL bar {bars['refusals_per_switch']}")
    return verdicts


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("logs", nargs="+", help="one or more UE log files")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--bar", action="append", default=[], metavar="KEY=VAL")
    args = parser.parse_args()

    bars = dict(DEFAULT_BARS)
    for override in args.bar:
        key, _, raw = override.partition("=")
        if key not in bars:
            sys.exit(f"unknown bar '{key}' — known: {', '.join(sorted(bars))}")
        bars[key] = type(bars[key])(float(raw)) if isinstance(bars[key], float) else int(raw)

    matches = [per_match_summary(parse_log(path)) for path in args.logs]
    baseline = len(matches) >= bars["min_logs_for_baseline"]
    verdicts = judge(matches, bars)

    if args.json:
        print(json.dumps({"matches": matches, "verdicts": verdicts, "baseline": baseline}, indent=2))
        return

    print("=== AIBot metrics ===")
    for path, match in zip(args.logs, matches):
        print(f"\n-- {path}")
        for key, value in match.items():
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

    print("\n=== lobby spread (across logs) ===")
    for key in ("latency_mean", "refusals_per_switch", "ambition_switches"):
        agg = spread([m[key] for m in matches])
        if agg:
            print(f"   {key:22}: mean {agg['mean']:.3f}  min {agg['min']:.3f}  max {agg['max']:.3f}  n={agg['n']}")

    print("\n=== bars ===")
    if not baseline:
        print(f"   NOT A BASELINE: {len(matches)} log(s) < {bars['min_logs_for_baseline']} — "
              f"the AIB8 lesson (identical configs measured 39x apart). Verdicts below are "
              f"indicative only; do not paste them as a claim.")
    for verdict, name, detail in verdicts:
        print(f"   [{verdict}] {name}: {detail}")


if __name__ == "__main__":
    main()
