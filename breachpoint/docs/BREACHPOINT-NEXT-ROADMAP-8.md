# ROADMAP 8 — TEAMS

**Cut:** 23 August 2026 by the cloud lead · **Status:** Wave 1 LANDED (written, not compiled)

## The one-line goal

BREACHPOINT is a **4v4** arena shooter and every line of code in it was free-for-all. R8 makes a
side exist, and makes the three systems that must obey one obey it.

## The founder's ruling this waits on — and the assumption I shipped instead

**Friendly fire is OFF**, behind `bFriendlyFire` in `[/Script/BreachpointNext.BNGameMode]`.
That is an assumption, stated, not a discovery: it is a design ruling and it is yours. Flipping the
key is the whole of it — no code path changes, no rebuild of anything, and the door's refusal is
the only place that reads it. **Teams themselves are not optional**: the mode sides everyone
whatever that key says.

## Wave 1 — identity, and the three systems that obey it (LANDED)

`Match/BNTeams.{h,cpp}` is the whole vocabulary: a NAMESPACE, not a component or a subsystem,
because the answer must be identical on the server, the killer's machine and an observer's, and the
only input is a replicated int32 every machine already has. Anything stateful there would be a
second source of truth for a fact that has one.

- **Identity** — `ABNPlayerState::TeamId`, replicated to EVERYONE (unlike the respawn stamp:
  every machine draws every player's side), with `OnTeamChanged` fired by hand on the authority and
  from the OnRep on clients — the discipline the score and killfeed feeds already use.
  `BNTeams::Unassigned` is a REAL state, not an error: a controller exists for a frame before the
  mode sides it, and a joining client's TeamId bunch can trail.
- **Assignment** — `ABNGameMode::AssignTeam`, from `GenericPlayerInitialization`: the one seam
  humans AND bots both pass through (an AIController never sees `OnPostLogin` — the same reason the
  death subscription lives there). Smaller side wins, ties to the lower index, so the first two
  joiners land opposite rather than stacking. Counted from the ROSTER, never a running tally: a
  tally drifts the first time someone leaves.
- **Damage** — one gate, at the one door, BEFORE the spec is built. An ally's shot moves no
  attribute, applies no RecentDamage window, and never reaches the capture that decides kill credit
  — the three ways a "harmless" hit would still have been felt. **Self-damage is never refused**:
  `Instigator == Target` is your own grenade, which is consequence, not friendly fire.
- **Bots** — `GetGenericTeamId` / `GetTeamAttitudeTowards` were written as explicit "teams-later
  seams" and now read the PlayerState. The shot is actually stopped in `IsValidTarget`, which its
  own comment already called "the single function that decides what counts as a target" — so one
  check covers sighting, re-targeting and every StateTree task, and perception keeps sensing
  everything (`DetectionByAffiliation` stays all-true on purpose).

**The two conservative answers point opposite ways, deliberately.** The door treats an unassigned
pair as NOT allies (refusing damage between two players nobody has sided yet would make them
invulnerable); the bot treats a stranger as hostile (a bot that reads a stranger as harmless walks
past a real enemy, while a wrongly-hostile read costs one shot at someone about to be sided).
`BNTeams::AreAllies` never guesses — it answers the narrow question and each caller picks the
conservative side its own failure mode needs.

## Wave 2 — the team as a SCORE (not started)

This is where the ripple is, which is why it is its own wave rather than bolted onto the above:

- `ABNGameState::Winner` is an `ABNPlayerState*`. A team win needs a winning TEAM, which changes
  the winner's TYPE — and R7's post-match banner, the scoreboard's winner mark and the killfeed's
  three wordings all read it.
- `FinishMatch(Killer)` and the score-limit check in `HandlePlayerDeath` are per-player today.
- The scoreboard should GROUP by team, and the killfeed should colour ally vs enemy — both need
  `TeamId` on the view rows (`FBNScoreRowView`, `FBNKillfeedViewEntry`), which is one field each
  and a director that already resolves everything else.

## Named gaps, not oversights

- **No rebalance.** A side is assigned once and kept for the match; a leaver leaves it uneven, and
  `EnsureBotFill` despawns the newest bot rather than the one on the larger side. Both are one
  function each, and both are match-flow decisions worth making deliberately.
- **No team spawns.** `ChoosePlayerStart` is untouched, so you can still spawn in front of an
  enemy. Wants either tagged PlayerStarts or an enemy-distance score.
- **Travel drops TeamId** — exactly as it already drops Kills and Deaths: BN overrides no
  `CopyProperties`. One override covers all three the day travel lands, and BN's in-place restart
  never exercises it.
- **No UI yet.** Nothing on screen says which side anyone is on. That is Wave 2's, with the score.
