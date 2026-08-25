# FAIRPLAY — the no-cheat contract (law, not guidance)

> The bot's success must come from decisions a human could have made with information a
> human could have had. Every law below is testable, carries a name for findings to cite,
> and binds every file in this module. aib-critic attacks against this list; aib-verifier
> samples against it in every PIE session. Halo Infinite's fairness doctrine is the source:
> a bot that cheats teaches nothing and reads as broken.

- **F1 — The reaction floor.** No decision path responds to a stimulus faster than
  `AIB::MinReactionSeconds` (0.20). It is a module CONSTANT, not a tier knob — tiers draw
  latencies ABOVE it. Any code path around the reaction clock is a `high` finding.
- **F2 — Stimuli mature, never teleport.** Sight, sound, damage, and incoming projectiles
  all enter the sensorium's clock and are invisible to the brain until matured. A grenade
  the bot never perceived is a grenade it cannot dodge — explosives get no side channel.
- **F3 — The envelope is the world.** The brain sees only what the sensorium admits:
  sight cone + range, hearing range, matured memory. World queries (pickups, objectives)
  are bounded by the same envelope or by information a human HUD would show (scores,
  objective state) — never by omniscient iteration over actors. The 25 Aug never-idle
  wallhack is the standing counterexample.
- **F4 — Aim drifts, never snaps.** Aim error is a moving offset corrected over time
  (re-aim), not a per-shot dice roll on a perfect solution. Target switches restart the
  drift. Sub-tick flick corrections are a finding.
- **F5 — Memory decays.** Last-known positions age out; a bot that lost you must search,
  not track. Freshness windows are tier data, but infinite memory is banned at every tier.
- **F6 — Verbs only.** The bot acts through the same verb surface a player's input reaches:
  press, release, move, look. No teleports, no direct velocity writes, no state the input
  path could not have produced.
- **F7 — Failure is visible.** When the bot cannot (path fails, verb refused), it behaves
  observably — repath, reposition, give up — never silently retries at frame rate. A stuck
  bot that looks stuck is honest; a stuck bot burning CPU invisibly is a defect twice.

Amendments to this file are founder rulings, dated, appended — never silent edits.
