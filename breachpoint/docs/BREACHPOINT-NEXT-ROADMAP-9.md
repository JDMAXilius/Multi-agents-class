# ROADMAP 9 — THE BOTS FEEL LIKE PEOPLE

**Cut:** 23 August 2026 by the cloud lead · **Status:** LANDED (written, not compiled)
**Numbering:** R8 was TEAMS, landed and then **reverted** at the founder's call — it changed
what solo PIE looks like while the HUD was being verified. Its history is intact at `a284ef5`;
reverting the revert restores it. R9 is the next thing, not a renumber.

## The one-line goal

Four faults found by reading the bot stack end to end after the founder's build. Three are
behaviour a player can see; one is a limitation R5 wrote down and R9 closes.

## 9.1 — a fleeing bot walked back toward the thing it was fleeing

`HasFreshLastKnownLocation()` disqualified itself when `GetCurrentTarget() != nullptr` — but
**Survive deliberately blanks `GetCurrentTarget()`** (R6 G2 2.2, how the brain steers the tree
without editing it). So the test could not tell *"no target because I lost them"* from *"no
target because I am running"*, and a hurt bot with a fresh memory entered **Search** and walked
to where its attacker was.

Roam already had this right: it flips to the point of interest FARTHEST from the threat when
Surviving. Search simply never got told. One condition, at the top of the freshness test.

## 9.2 — bots could not hear

A bot shot in the back re-scored its ambition and **did nothing else**. You could empty a
magazine into one and it would never turn around.

The attacker was already recorded: `FBNLastDamage` is captured at the one reaction point every
damage passes through, on the authority — which is where bots live. `OnRecentDamageTagChanged`
now reads it and stamps the attacker's position as the last-known threat, so **Search** sends the
bot to look, exactly as losing sight of a seen enemy does.

**A memory, not a target**, and that is the whole design: being hit should make a bot go and
hunt, not acquire a perfect lock on someone it has never seen. Perception still has to do the
seeing, and the reaction window still applies when it does.

`ClearCurrentTarget` now writes that memory through the same `RememberThreatAt` — one writer for
one fact.

## 9.3 — bots stood perfectly still while firing

The single loudest tell that a thing is not a person. `FBNStrafeTask` sidesteps during **Shoot**:
a companion task that returns Running forever beside Face Target, so **Fire Burst still decides
when the state ends**.

- Perpendicular to the line of fire only. Stepping toward or away would be closing or retreating,
  and those are Close's and Survive's jobs.
- **Safe only because `ABNCharacter` aims with the CONTROLLER** (`bUseControllerRotationYaw`
  true, `bOrientRotationToMovement` false): the body moves sideways while the aim stays on the
  target. On a character that orients to movement this task would spin the bot mid-burst — that
  is written on the task itself.
- Deterministic (§5): the opening side is seeded off the controller's identity and flipped each
  step, never drawn from the global stream. A failed step flips the side too — a bot with its
  back to a wall turns around rather than pressing into it.

**THIS ONE NEEDS THE EDITOR.** A new StateTree node does not appear in a compiled tree by
existing: `ST_BNBot` must be rebuilt by `Tools/bn/62_bot_assets.py` (the probe list there gained
`FBNStrafeTask`, so a stale build now stops the script instead of silently authoring a tree
without it). Until that rebuild, everything else in R9 works and bots simply do not strafe.

## 9.4 — R5's fill limitation, closed

`EnsureBotFill` returned early on any state but warmup, so a human joining after the start left
the lobby one wide for the whole match. Warmup still both fills and yields; a **live match may
only yield** — a bot materialising beside you mid-fight is worse than a seat over, and Lyra draws
the same line (remove on join, backfill between matches).

## What is still weak, and still true

- **No cover.** Close is a straight line at the enemy. Real cover wants EQS or tagged cover
  points, and it is the next real step in bot quality.
- **Sight only.** 9.2 gives bots a reaction to being HIT; they still cannot hear a firefight
  they are not part of, or footsteps. A hearing sense is a perception config change plus one
  handler, and it reuses 9.2's memory.
- **No squad sense.** Nothing coordinates two bots; each fights alone. Teams (R8, reverted) is
  the prerequisite for any of that.
