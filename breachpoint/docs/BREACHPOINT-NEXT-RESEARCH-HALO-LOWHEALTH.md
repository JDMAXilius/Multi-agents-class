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

---

## Log — 28 Aug: fix ① landed, and the fighting retreat that came with it

**① ShieldNorm — landed, but NOT in the shape this doc proposed.**

The doc said "add a Retreat consideration on ShieldNorm, curved steeper than health."
Writing it exposed why that cannot work: `UAIBAmbitionEngine::Rescore` multiplies every
consideration into the base utility. A multiplicative term can only pull a want DOWN. So

- it could never have RAISED Retreat at full health — the case the whole fix exists for.
  `Hurt` on `HealthNorm` is already 0.0 there, and 0 × anything is 0; and
- whatever it scored on a FULL shield would have taxed every health-driven retreat in a
  shieldless game. At the drafted 0.15 that was an 85% cut to behaviour that works today.

What landed instead: `Hurt` reads a new `EAIBFactSelector::VitalityNorm` =
`min(HealthNorm, ShieldNorm)` — the more depleted of the two layers is the one you are
dying through. Same Halo intent, one selector, no second term.

"Steeper than health" falls out rather than being authored: a shield swings 1→0 in one
burst while health decays across a fight, so the same curve reads as a spike on the shield
and a slope on health.

- `IAIBAvatarInterface::GetShieldNorm()` — 1 when unknowable AND 1 when `MaxShield <= 0`.
  That second clause is the safety: **`BNGE_InitVitals` sets MaxShield to 0 today** (shields
  paused, 13 Aug), so `min()` collapses to `HealthNorm` and live behaviour is unchanged by
  construction. This arms itself the day shields come back on.
- Pinned by two specs: the shield-break rhythm (intact → fight, broken → break contact,
  recharged → fight again) and shieldless neutrality (a full shield adds nothing of its own,
  proven on SCORES, not on who won one rescore).

**A spec of mine was wrong first, twice, and the engine was right both times.** The first
draft asserted a hurt bot retreats with no recent damage — but `UnderFire` gates Retreat at
0.35 when nothing has shot you lately ("merely being hurt is not yet a rout"), which is
deliberate. A shield does not break in silence, so the spec now sets damage history and
tests the state that can actually occur.

**② (unplanned, from the founder's question) — Retreat could not shoot back.**

Retreat's branch was `Sentinel + Flee` and nothing else: the bot turned its back and jogged
away without a shot, a free kill for whoever had just stripped it. Halo's spartans backpedal
firing. Retreat now also runs `FaceBelief` + `FireWhenAble` beside the flee (a state runs all
its tasks; the mover still owns where the body goes).

`FaceBelief` needed a new `bRequireTarget` flag, default true. It FAILS on lost visibility —
which is how Engage ends on a lost belief, and which in Retreat would collapse the branch at
the exact moment a retreat is WORKING. `FireWhenAble` needed no flag; it already gates every
press on visibility and fails only when the avatar door closes.

ST_AIBBot rebuilt through the editor (`Tools/aib/70_aib_assets.py build`): Retreat 2 → 4
tasks, compile OK, save OK.

**Known gap, named not fixed:** `Mode` is now the only branch that can hold while an enemy
is visible and still not return fire (Engage is never suppressed — `NoteAmbitionFailed` has
exactly one caller, `FAIBMoveToObjectiveTask` on an unreachable objective — so there is no
defenceless-Engage path). Mode's `SweepLook` already owns control rotation, so adding aim
there is a rotation-arbitration decision, not a task addition. Founder's call.

Ladder: rung 1 both targets Succeeded · rung 2 AIBot **121/0**, Breachpoint **126 started,
3 failing** — the same three pre-existing legacy failures, unchanged. Not run: PIE, and no
multiplayer claim is made for any of this.

---

## Log — 28 Aug: Retreat becomes DEFEND

Founder: *"for the ai bot low health do not just make them crouch and just doing nothing or
retreating. make them just defend mode where they crouch jump, evade, fire at you —
everything they do on attacking mode but in defence mode."*

**What Retreat was.** Even after the fighting-retreat fix it was still a rout: `Flee` runs to
a goal, SUCCEEDS on arrival, the branch re-selects, and it picks a new goal and runs again.
A bot that ran until the want expired. The only crouching in the module is the reload crouch
(`bCrouchedToReload`) — bots crouch because they reload often, not because they are hurt.

**What it is now: two halves separated by ONE number.** `DEFEND_RANGE_UU = 700`.

- Closer than 700 → `Flee` owns the legs and breaks contact. A defensive fight at knife range
  is just dying slower.
- At/beyond 700 with the threat VISIBLE → `Flee` stands down and `Strafe` owns the legs. The
  bot circles, jukes, hops and shoots — Engage's own footwork, in the retreat branch.
- Out of sight → `Flee` again. You cannot fight what you cannot see.

Retreat went 2 tasks → 5, the same count as Engage: Sentinel, Flee, FaceBelief, FireWhenAble,
Strafe.

**Why one number and not two tuned ones.** This file's own rule: *two movers issuing in one
tick cancel per tick*. `Flee.DefendRangeUU` and `Strafe.MinRangeUU` are the same constant read
twice, exactly as `MoveNearBelief` and `Strafe` already share `FightRangeUU`. If they ever
disagreed, the overlap band would have both movers issuing and the bot would stand vibrating —
which is precisely the "doing nothing" being complained about.

Two details that would each have re-created the bug:

- The stand-down returns **Running, never Succeeded**. Succeeding re-selects the branch and
  picks a fresh flee goal — the running-away loop again.
- `StopMovement` fires **once on the edge**, not per tick. Per tick it would cancel the strafe
  step every frame and root the bot in place.

**The evasive hop.** Jump was previously used for exactly one thing: unsticking a wedge stall.
It now also fires on a defending bot's JUKE — the leg that reverses direction. Riding the juke
means it inherits `JukeChance`'s capability gate for free (0.00/0.00/0.25/0.50), so a Novice
cannot hop and no new tier lever is introduced (R28). Gated on `MinRangeUU > 0`, so **Engage's
footwork is byte-identical** and every strafe measurement in the AIB tickets stays comparable.

**Not done, deliberately:** a literal crouch-jump. Crouch is a toggle here and is already
spent on the reload; adding deliberate crouching to a firefight risks re-creating the earlier
"they crouch a lot" complaint. The hop is the evasive half and is the part that breaks a
tracking player's aim.

Ladder: rung 1 Succeeded · rung 2 AIBot **123/0** (2 new) · ST_AIBBot rebuilt, Retreat 5 tasks,
compile OK. **Not PIE-verified** — no eyes-on claim for how it reads in a live fight.

### Measured, 28 Aug — one headless match, BR_Arena01

Founder: *"right now is still the same."* The tree read-back proves the asset is applied
(`DefendRangeUU=700`, `MinRangeUU=700`, `FightRangeUU=3000` on Retreat; Engage untouched at
`0 / 900`). So the question was never "did it land" — it was "does it ever run".

| | count |
|---|---|
| Engage | 154 |
| Roam | 144 |
| Search | 34 |
| Mode.Rally | 27 |
| **Retreat** | **12** |
| **DEFEND stand-downs** | **9** |
| defensive hops | **0** |
| eliminations | 9 |

**The defend band works.** 9 of 12 Retreats reached it and held, logged at 701, 774 and 859uu
— the 700 floor doing exactly what it was built to do. Those bots stopped running and fought.

**The hop cannot fire at this tier, by design.** The match ran Marine — `Mv 1`, Movement
level *Trained* — and `JukeChance` is `0.00 / 0.00 / 0.25 / 0.50`, i.e. zero below Skilled.
The hop rides the juke precisely so it would inherit that gate without adding a tier lever
(R28), and the gate is now doing its job: a Trained bot never jukes, so it never hops. Against
Marine bots the evasive jump is structurally unreachable.

**Retreat is rare: 12 of 371 ambitions, 3.2%.** So even working perfectly, defend mode shows
in roughly one engagement in thirty. That is the real reason it reads as "the same" in play —
not that it is broken, but that it is seldom seen.

### A/B measured, 28 Aug — 4 runs after the hop + appetite change

| run | switches | Retreat | DEFEND stand-downs | hops |
|---|---|---|---|---|
| baseline (before) | 371 | 12 | 9 | **0** |
| after 1 | 343 | 11 | 9 | 1 |
| after 2 | 310 | 15 | 14 | 1 |
| after 3 | 302 | 12 | 16 | 1 |
| after 4 | 311 | 12 | 10 | 0 |
| **after mean** | 317 | **12.5** | **12.3** | **0.75** |

**The hop is fixed structurally, and is still rare.** It went from *impossible* to *possible*
at Marine — that part is settled, and it is the part that was actually broken. But ~1 per match
means a player will seldom witness it. The limiter is not `HopChance` (0.30 at Trained); it is
that a defending bot only gets a handful of stepping legs before the fight resolves. Raising
the chance further would make each defence bouncier without making defences more common.

**The retreat appetite change did NOT move the rate, and that is the honest result.** 12 → 12.5
mean is noise. The scores demonstrably rose — Retreat now wins at 1.35 and 1.14 where it
previously topped out lower — but winning bigger is not winning *more often*. Where it loses it
loses to a strong Engage (0.77 vs 0.90, 0.56 vs 0.64), and closing that would take another
20–30%, at which point Retreat starts winning fights it should not.

**Stand-downs rose 9 → 12.3 (+36%).** So the same number of retreats now more often turn into
a defensive stand rather than a run. That is the change that actually landed.

**The real limiter looks like time-at-low-health, not utility.** One match: 127 damage events,
4 deaths, and health readings clustered at the extremes. Bots go from healthy to dead without
lingering in the band where retreating is the right call. If more retreating is genuinely
wanted, the lever is probably TTK or health regen pacing — not the ambition curve. That is a
game-feel decision, not a bot-tuning one, so it is not taken here.
