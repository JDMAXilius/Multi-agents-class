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

- **F8 — Raw perception is quarantined.** The engine's perception component is reachable
  through public inherited API (`GetPerceptionComponent`, `HasActiveStimulus`,
  `GetCurrentlyPerceivedActors`) that no module code can make private — so the law is a
  grep, not a hope: outside `Core/AIBBotController.cpp`'s own wiring, NOTHING in this
  module may name those symbols. Check:
  `grep -rn "GetPerceptionComponent\|HasActiveStimulus\|GetCurrentlyPerceivedActors" Source/AIBot/ --include=*.h --include=*.cpp | grep -v AIBBotController.cpp`
  returns nothing. (W-REVIEW F1-A: the sibling module already walks through this exact
  door 20 files away — the precedent is live, the quarantine is the answer.)

Amendments to this file are founder rulings, dated, appended — never silent edits.

---
**Amendment, 26 Aug 2026 (W-REVIEW P2 H1):** the visible target's position is a TRACKED
BELIEF — re-sampled once per sensorium pump while sight is current, frozen the moment a
loss is noted, and read by everything downstream from that one site. Live actor reads at
think rate are banned. The residual pre-report window (the engine's sight sense not yet
reporting an occlusion) is bounded by engine internals this module cannot see; ACCEPTED,
revisit if bots visibly track through walls.

**Amendment, 26 Aug 2026 (W-REVIEW P3, fairness HIGH):** a sight GAIN whose actor has a
NOTED loss at-or-after the gain's event time is SUPERSEDED — it lands as memory at the
gained spot, never as current sight. The loss ledger is written at note time, per actor,
so it survives both a loss that matures before its own gain (latency draws invert on
~1 in 6 short peeks) and a loss the clock's pending cap dropped. Without this, a 100ms
peek could mature into live wall-tracking for the whole engine-perception max age, and
the forgotten-path loss then laundered the tracked position into search memory. Pinned
by the "supersedes a gain whose loss drew the faster reaction" sensorium spec.

**Amendment, 26 Aug 2026 (W-REVIEW P2 H2):** enemy vitals are NOT a perceivable fact.
No live health float crosses the envelope at any range or occlusion — a human reads a
silhouette, not a number. Target vitals stay UNKNOWN until an estimate derived from
damage the bot itself dealt (matured) lands; `GetHealthNormOf` on the avatar door is an
adapter utility the facts builder must not call for live targets.

**Amendment, 26 Aug 2026 (W-REVIEW P2 M6):** `RecentDamageTakenNorm`/`DealtNorm` are
SELF facts, sourced through the avatar door only, UN-MATURED BY DESIGN — feeling your
own health drop is instant for a human. What must still mature is WHO and WHERE: the
direction and identity of damage remain sensorium-only, on the clock. Banned shapes:
"I'm being hit" becoming "he's over there" faster than the clock; per-hit magnitudes
read from game damage events bypassing the door; any read of the shooter's weapon.

**Amendment, 26 Aug 2026 (W-REVIEW P2 M4):** the blast fuse is currently ground truth,
recomputed each think — inhumanly consistent dodges at exactly T-2.5s. OPEN: needs a
founder ruling between one fuse-noise draw at maturation (stored, not recomputed) or a
dated acceptance. Until ruled, tuning must not rely on fuse-perfect timing.

**Amendment, 26 Aug 2026 (later the same day — the ruling above, CLOSED):** option ONE —
one fuse-noise draw per blast, taken where the warning enters the reaction clock and
STORED in the stimulus payload; every later ask reads the same estimate (F4's
anti-dice-roll law, applied to the ear). The envelope is asymmetric by design
(`AIB::BlastFuseNoiseEarlySeconds` 0.35 / `LateSeconds` 0.15): erring early reads as a
panicky human and costs nothing; erring late occasionally eats the blast, which is what
retires the tell. Two spec pins (envelope + no-reroll). Closed under the founder's
roadmap-completion directive on the cloud lead's recommendation; reopenable by ruling.
ALSO wired the same day: `CanEvadeBlast` gained its promised caller — a Novice grenade
competence never receives the matured blast warning at all (the capability gate the
policy documented but nothing consulted; giving LESS information is always fair).

**Amendment, 25 Aug 2026 (W-REVIEW F-2.2):** UObject lifetime is authoritative for
memory validity — a memory of a destroyed actor reads as no memory, the instant the
actor dies, anywhere on the map. This IS an omniscience channel (a bot stops searching a
corner because its quarry died elsewhere, unperceived) and it is ACCEPTED: the
alternative — bots searching for the dead — reads as broken faster than the leak reads
as unfair, and no position or health information flows through it. Revisit if bots ever
visibly abandon a search at the moment of an unseen kill.
