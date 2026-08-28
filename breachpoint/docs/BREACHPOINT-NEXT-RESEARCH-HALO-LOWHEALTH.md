# RESEARCH — what a hurt bot should do, and what Halo actually does

**Cut:** 28 August 2026 · **Scope:** research only. No source touched, nothing implemented.
**Supplements:** `BREACHPOINT-NEXT-RESEARCH-HALO-COMBAT.md` (25 Aug), which read the Halo 2/3
campaign behaviour lists. This one is about the *hurt* half, and about Halo **Infinite**'s
multiplayer bots specifically — the thing BREACHPOINT copied the tier names from.

**Why now.** BN22 turned recharging shields back ON (`7afc0aff`). That single change makes the
central Halo combat loop available to this game for the first time — and our bots have no
concept of it. This is the read before anyone writes code.

---

## 1. What BREACHPOINT does today (read from source, 28 Aug)

| | |
|---|---|
| Wanting to flee | `Retreat.BaseUtility 1.2`, health curve inverted over `[0, 0.6]` — continuous pressure, no threshold |
| Under fire | `RecentDamageTakenNorm` curve starts at **0.35**, not 0 — *"merely being hurt is not yet a rout"* |
| Nerve | Phase-5 confidence suppresses Retreat; `Assess += (HealthNorm − 0.5) × 0.5` |
| Commit | 3s, so it cannot flicker at the boundary |
| The one hard interrupt | `HealthCliffNorm = 0.35`, fired on the **crossing** (`HealthNorm < 0.35 && Last >= 0.35`), voiding the commit. It does not pick Retreat; scoring still does |
| What Retreat *does* | flee ~900uu directly away from the threat, or a reachable reposition draw |

**Everything above is health-shaped. Nothing is shield-shaped, and nothing is opponent-shaped.**

---

## 2. What Halo does

### 2.1 The loop is the SHIELD, not health

Halo's rhythm is shield-break → break contact → recharge → re-engage. Griesemer's own framing of
why the game feels the way it does:

> "If you charge in guns blazing, the AI pushes back hard, but if your shields go down and you run
> for cover, **it backs off and lets you catch your breath**. That give and take, that dance that
> the game is doing with the player, is what gives Halo its flavor."
> — [Engadget, *Half-Minute Halo: An Interview with Jaime Griesemer*](https://www.engadget.com/2011-07-14-half-minute-halo-an-interview-with-jaime-griesemer/)

Two separate ideas live in that sentence, and BN has neither:

1. **A hurt combatant breaks contact to recharge.** Not "flees because it is dying" — *withdraws
   because withdrawal is how you get your shields back*. It is a round trip with an intended
   return, not a rout.
2. **The AI reads the OPPONENT's state and eases off.** Halo's AI backs off when the *player* is
   hurt. That is a deliberate pacing gift, and it is the opposite of what a naive utility bot does
   (a wounded enemy is the highest-value target, so press).

### 2.2 Infinite's bots are utility-scored, like ours

343 on the multiplayer bots — the same architecture BREACHPOINT already runs:

> "We've broken down all the high-level actions for the Bots (like running the objective, getting a
> new weapon, engaging in combat, etc.). We then assign each of those actions a value that's based
> on a number of inputs, weighing each one a little differently, and then we choose what the
> optimal action to take at a given time is."
> — Brie Chin-Deyerle, [343, *Inside Infinite* August 2021](https://www.halowaypoint.com/en-us/news/inside-infinite-august-2021)

And the tiers we copied are behavioural, not numeric:

- **Recruit** — *"they know how to perform each combat action, but they don't react quickly"*
- **Marine** — *"comfortable playing Halo, but haven't quite figured out the best way to strafe"*
- **ODST** — *"react well to player movement and know how to use equipment more aggressively"*
- **Spartan** — the top rung

Note what separates them: **reaction speed, strafe quality, and equipment aggression** — not
damage or health. R11's reaction floor and R28's "differ only on levers a player can read" are
already the right shape. *Retreat threshold is not on that list*, which is an argument against
making low-health behaviour a tier lever.

### 2.3 Halo 2/3 name the retreat behaviours individually

From the shipped behaviour list ([343/MS, *Halo 2 AI Behavior List*](https://learn.microsoft.com/en-us/halo-master-chief-collection/h2/ai/aibehaviorlist)),
already quoted in the 25 Aug research — the hurt-relevant entries:

- **Cover-peek** — *"lean out from behind wall and fire… **Duck back behind cover immediately if
  I'm hit**."* A damage-driven micro-retreat, measured in a second, not a disengage.
- **Charge when cornered** — *"If I have nowhere else to go, turn around and run melee-charge."*
- **Unreachable-enemy-cover** — take cover when sniped from beyond your own weapon's range.
- **Scary-target-cover / retreat** — gated on target "scariness", a per-character number.
- A whole family: `Leader dead retreat`, `Peer dead retreat`, `Danger retreat`,
  `Proximity retreat`, `Surprise retreat`, `Overheated weapon retreat`.

**Danger is a first-class numeric emotion**, debuggable in-engine (`ai_render_emotions 1`).

The lesson is not "add six retreat behaviours". It is that Halo treats *retreat* as a family of
distinct causes with distinct recoveries, where BN has exactly one cause (health) and one
recovery (run 900uu away).

---

## 3. The five gaps, most valuable first

**G1 — Retreat has no shield concept, and shields are now on.** The Retreat curve reads
`HealthNorm`. In a shielded game the meaningful event is the **shield breaking**, which is the
moment a player is one burst from death and the moment Halo expects a disengage. A bot at 100
health and 0 shields is in mortal danger and scores Retreat at *zero*. This is the single biggest
mismatch between our model and the game we now ship.

**G2 — Withdrawal has no intent to return.** `FleeFromBelief` runs 900uu away and the branch ends.
Nothing waits for the shield to refill, and nothing re-engages when it does. Halo's disengage is a
round trip; ours is a one-way exit. With BN22's recharge-after-quiet, *hiding until shields
recharge* is a real strategy the bots cannot express.

**G3 — Nothing reads the opponent's state.** Confidence reads own health, momentum, weapon
fitness, visible enemy count — never whether the enemy is hurt. So bots cannot press a wounded
opponent, and cannot perform Halo's signature easing-off either. `FAIBFacts` has no
`TargetHealthNorm`, and the AIBot roadmap's own fact list names *"opponent health"* as intended.

**G4 — No micro-retreat.** Between "fight" and "flee 900uu" there is nothing. Halo's cover-peek
ducks back **immediately on being hit** and returns in a second. Ours changes ambition, pays a 3s
commit, and leaves the fight.

**G5 — One cause, one recovery.** Halo distinguishes danger/proximity/peer-dead/overheat. BN's
single health curve cannot express "I am fine but I am alone", which is the most common real
reason to fall back in a 4v4.

---

## 4. What I would actually build, in order

**① `ShieldNorm` as a fact, and a Retreat consideration on it.** Cheapest, largest effect, and it
uses machinery that already exists — `IAIBAvatarInterface` already exposes `GetHealthNorm()`; add
its sibling. Curve it steeper than health: a broken shield should want to break contact *hard*,
because that is the Halo rhythm. Note R11/R28: this is not a tier lever, it applies to every rung.

**② Make the shield cliff an interrupt, exactly as the health cliff is.** `HealthCliffNorm` already
proves the pattern, including the edge-not-state discipline (*a state-shaped interrupt makes the
bot dither through the most legible moment of the fight*). A shield-break edge is the more Halo-
correct trigger of the two.

**③ Give Retreat an intended return.** Not a new ambition — a *terminating condition* on the
existing one: hold the withdrawal until shields are recharging or recharged, then let scoring pull
the bot back. This is what turns a rout into a Halo disengage, and BN22's recharge window is the
natural clock.

**④ Add `TargetHealthNorm` / `TargetShieldNorm` to the facts.** Then Engage can press a wounded
target, and — if the founder wants Halo's pacing gift rather than pure optimality — a *deliberate*
easing-off curve can be argued from the Griesemer quote. Flag: that is a **design ruling**, not a
technical one. Optimal play says finish the wounded player; Halo says sometimes let them breathe.

**⑤ A micro-retreat verb, last.** Duck/break-line-of-sight on being hit, sub-second, without
changing ambition. This is the most work and the least certain, and it interacts with the strafe
gate (AIB10) which is itself still unmeasured.

---

## 5. What this research does NOT establish

- **No primary source describes Infinite's bots' shield-break behaviour directly.** 343 published
  the utility architecture and the tier descriptions; the disengage rhythm above is Griesemer on
  *campaign* AI, and the Halo 2/3 behaviour lists are campaign too. The inference that Infinite's
  MP bots do the same is *reasonable and unproven*. Anyone implementing ① should expect to tune
  against playtests, not against a spec.
- **The Forge AI toolkit is campaign units, not the MP bot brain.** Its ~75 nodes expose
  spawning, teams, movement zones and patrols — not aggression or retreat parameters. It is not
  the window into the bot model it first appears to be.
- **Nothing here is measured against our own build.** Retreat currently fires ~37 times per match
  against thousands of Roam/Engage switches, and its flee task was the worst-performing mover in
  the tree before AIB5 (267 refusals per Retreat ambition). Low-health behaviour is the least
  observed branch we have. **Instrument it before tuning it** — the AIB8 lesson (one match cannot
  tell 0.04 from 1.67) applies with full force to a branch this rare.

---

## Sources

- [Engadget — *Half-Minute Halo: An Interview with Jaime Griesemer*](https://www.engadget.com/2011-07-14-half-minute-halo-an-interview-with-jaime-griesemer/)
- [343 Industries — *Inside Infinite*, August 2021](https://www.halowaypoint.com/en-us/news/inside-infinite-august-2021)
- [343/Microsoft — *Halo 2 AI Behavior List*](https://learn.microsoft.com/en-us/halo-master-chief-collection/h2/ai/aibehaviorlist)
- [343/Microsoft — *Halo 3 Leadership*](https://learn.microsoft.com/en-us/halo-master-chief-collection/h3/ai/leadership)
- [Halo Support — *Shields in Halo Infinite*](https://support.halowaypoint.com/hc/en-us/articles/24284987877524-Shields-in-Halo-Infinite)
- [343 — *Forge Overview, Season 5*](https://www.halowaypoint.com/news/forge-overview-season-5)
