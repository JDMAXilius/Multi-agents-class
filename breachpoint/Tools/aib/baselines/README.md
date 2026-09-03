# AIBot baselines (AIB22)

Committed `--json` dumps of `Tools/aib/80_aib_metrics.py`, taken BEFORE a behaviour change so
the change can be gated against numbers, not impressions.

## Naming

`aib22-<map>-<date>.json` — e.g. `aib22-BR_Spillway-2026-09-03.json`. One file per map per
baseline day; the `<map>` is the level name without `/Game/Maps/`, the `<date>` is ISO.

## Producing one

Five headless runs minimum (the AIB8/AIB9 lesson: two identical-config matches measured 39x
apart; fewer than five and the script itself stamps NOT A BASELINE). The headless command line
is in `docs/tickets/TICKET_AIB22_PHASE11_EGRESS.md` (W-AUDIT member 3); logs go to
`Tools/Logs/aib22-base-<map>-<n>.log`, then:

    python3 Tools/aib/80_aib_metrics.py Tools/Logs/aib22-base-<map>-{1,2,3,4,5}.log --json \
        > Tools/aib/baselines/aib22-<map>-<date>.json

## Using one

    python3 Tools/aib/80_aib_metrics.py <new logs...> --baseline Tools/aib/baselines/aib22-<map>-<date>.json

Adds two gates: median per-bot `no_path_requests` <= 0.5 x baseline median, and lobby kills/min
>= baseline median - (baseline max - min). The file must be this script's own `--json` output
(it reads `lobby_spread`); a hand-edited baseline is a finding, never truth.

- 2026-09-02 (AIB22 step 2): `aib22-spillway-2026-09-02.json`, `aib22-arena01-2026-09-02.json` — 5 x 2 maps, `-game -windowed` fallback (the `-server` form ends the match at frame 0), 7 bots + local player, ScoreLimit=7 ended 3 of 10 early. Rung 3.

## The `stuck_seconds` split, and what every baseline here says about it (2026-09-03)

`stuck_seconds` is now reported three ways — `_crowded`, `_wedged`, `_uncaused` — which
partition it exactly (the self-test asserts the sum). The cause comes from `still=` on the
module's `stall over` line, added the same day.

**Every baseline in this folder predates that field.** So in `v4` through `v7`, and in both
`2026-09-02` pairs, `stuck_seconds_crowded` and `stuck_seconds_wedged` read **0 and the
whole total sits in `_uncaused`** — because the cause was never written down, not because
no bot was ever crowd-braked. Reading a committed baseline's `0` crowded as "separation was
not involved" inverts the finding it exists to test. Only a run from v8 forward can be split.

Why the split exists: `stuck_seconds` jumped 4-6x at v6 (Spillway 19.7 -> 85.9, Arena01
14.4 -> 82.2), and v6 is the first release in which crowd avoidance actually engaged —
before it, a bot whose crowd manager was not ready at possession ran with separation off
for its whole life. Progress in the stall detector is pure displacement, so a crowd brake
feeds the clock exactly as a wall does; that is deliberate (Phase 13), and it means
`stuck_seconds` is a no-ground-gained counter, not a wedge counter. `idle_seconds` exempts
the same brake. The split is what tells the two apart. Full argument: the 2026-09-03 Log in
`docs/tickets/TICKET_AIB22_PHASE11_EGRESS.md`.
