# RESEARCH — Halo's combat AI, and the seven things BN should actually steal

**Cut:** 25 August 2026 · **Scope:** research only, nothing implemented, no source touched.
**Reads against:** `BREACHPOINT-AI-BOTS.md` §1 (the four Halo lessons already in doctrine),
`docs/BREACHPOINT-NEXT-RESEARCH-AI-BOTS.md`, `ROADMAP-9`, `ROADMAP-10`,
`docs/tickets/TICKET_BN14_AI_AUDIT_FINDINGS.md`, and the closed rulings R11 / R12 / R28.

**Why this file exists.** The doctrine's Halo section is four paragraphs written from memory of
Damian Isla's GDC material. R9 and R10 then shipped behaviour described as "Halo's shape"
(`bEvadesBlasts`, the tier names, cover) without anyone having read the primary sources. This is
the read. It is longer than the doctrine section it supplements and it **contradicts it in two
places** — both flagged inline.

**A word on `Recruit / Marine / ODST / Spartan`.** Those are not generic tier names. They are
**Halo Infinite's four multiplayer bot difficulties, verbatim**
([343, *Inside Infinite* August 2021](https://www.halowaypoint.com/en-us/news/inside-infinite-august-2021)).
BN already copied the ladder; §2 checks whether it copied what the rungs *mean*.

---

## 1. What Halo actually does

### 1.1 The frame: three loops, and who owns each one

The GDC 2002 deck splits combat by timescale before it says anything about algorithms
([Butcher & Griesemer, *The Illusion of Intelligence*, GDC 2002](https://www.jmeiners.com/shamans/papers/ai/the_illusion_of_intelligence.pdf) ·
[GDC Vault](https://www.gdcvault.com/play/1022590/Creating-the-Illusion-of-Intelligence)):

| | Design Responsibilities | Code Responsibilities |
|---|---|---|
| Scope | **3 minute** | **30 second** |
| Owns | Racial personalities · Strategic purpose | Intelligent decisions · Instant reactions |

The famous "30 seconds of fun" line is the same split seen from the player's side, and Griesemer
has spent a decade correcting how it is quoted
([Engadget, 2011](https://www.engadget.com/2011-07-14-half-minute-halo-an-interview-with-jaime-griesemer.html)):

> "Everyone uses it to mean exactly the opposite of what I meant when I said it! … the point of
> the whole quote goes back to the AI talk, where you have a **3-second loop inside of a 30-second
> loop inside of a 3-minute loop** that is always different."

- **3 s** — the exchange: shoot / nade / melee / dodge. Owned by the sandbox.
- **30 s** — *"where to stand, when to shoot, when to dive away from a grenade."* Owned by the AI.
- **3 min** — *"when to send reinforcements, when to retreat, encounter tactics."* Owned by the
  mission designer; in Halo 3 this layer literally *is* the Objectives task tree (§1.6).

Closing slide of the 2002 deck: **"Combat Behavior is where Design and Code overlap."**

### 1.2 The three design goals, and the two things they threw away

The deck states the goals as **Intelligible · Interactive · Unpredictable**, and it is more useful
for what it *discards* than for what it builds:

- **"Discarded: Hidden States."** The listed replacements are *"Language, Posture, Gesture, Focus
  of Attention"* — and *"Communication of Intent"*. Legibility is delivered by the **body**, with
  dialogue as one channel among four.
- **"Discarded: Randomness."** Unpredictability is bought as *"Unpredictable player → Unpredictable
  situations → Unpredictable reactions"*, plus *"Analog Reactions: Position, Timing"*. Halo's
  variety is **reactive, not stochastic**. This matters for BN: the arena's variety budget is
  supposed to come from the fight, not from an RNG stream.
- **"No cheating."** The perception slide reads: *"Individual Knowledge Model. Discarded: Complete
  Model. **No cheating.** Vision, Hearing, Touch, ESP. Selective Memory … **Can be fooled.**"*

Isla restated the same point six years later ([Develop 2008, reported by Game
Developer](https://www.gamedeveloper.com/game-platforms/in-depth-bungie-on-eight-years-of-i-halo-i-ai)):
*"Each AI has an internal model of each target, and that model can be wrong. This allows the AI to
be surprised by you, and this is very fun."*

### 1.3 The finding that should unsettle us: **"Smarter = Tougher, Tougher = Smarter"**

Same AI code, two playtests, only enemy toughness changed (2002 deck, verbatim table):

| | Weak enemies | Tough enemies |
|---|---|---|
| "About right" | 52% | **92%** |
| "Very intelligent" | 8% | **43%** |
| "Not intelligent" | 20% | **0%** |

Bungie's headline lesson is that **perceived intelligence is bought with durability**. §4 explains
why BN is structurally barred from spending that currency.

### 1.4 Space: firing points are the whole trick

> "Solution: **Firing Points**. Weighted and selected: line of sight · distance to target ·
> proximity of cover · friends and enemies · vehicles, grenades, etc. Senses environment by
> multiple ray-casting. **Need a discrete answer to a continuous problem.**" (2002 deck)

Halo 2 formalises this into **Zones → Areas → firing points**, plus designer **Hints** for
knowledge *"too expensive (or impossible) to derive at run-time"* — cover, concealment, search,
sniping, climbable, hang-down-and-shoot, no-travel
([343/MS, *Halo 2 AI Engineering Outline*](https://learn.microsoft.com/en-us/halo-master-chief-collection/h2/ai/aiengineeringoutline)).
Notably, Halo 2 also generates firing points at runtime around **dynamic** obstacles only; static
ones stay hand-placed.

### 1.5 The behaviour layer: prioritised, binary, interruptible

Isla's Halo 2 architecture ([*Handling Complexity in the Halo 2 AI*, GDC
2005](https://www.gamedeveloper.com/programming/gdc-2005-proceeding-handling-complexity-in-the-i-halo-2-i-ai)):

- ~50 behaviours in a **DAG**, decided by a **prioritised list** — designer-ordered children, first
  *relevant* one runs, higher siblings interrupt on a later tick.
- **Relevancy is BINARY, not float utility** — explicitly chosen to avoid the un-tunable
  "dozens of floats" problem.
- **Impulses**: free-floating, duration-less triggers that redirect execution into another branch.
- **Stimulus behaviours**: an event injects a behaviour for 1–2 s so the reaction still passes
  through the normal priority hierarchy rather than bypassing it.
- **Memory in four flavours**, of which the load-bearing one is **props** — one per potential
  target, holding perceptual history *and* per-object-per-behaviour memory (*have I already
  searched for this one? when did I last melee it?*). Props are what the AI **believes** and are
  deliberately allowed to be wrong.
- **Styles**: designer allow/deny lists over behaviours (defensive forbids charge and search;
  aggressive disables self-preservation).

The shipped behaviour list is public and is the single densest artifact in this entire research
([343/MS, *Halo 2 AI Behavior List*](https://learn.microsoft.com/en-us/halo-master-chief-collection/h2/ai/aibehaviorlist)).
The entries BN cares about, verbatim:

- **Suppressing fire** (PRESEARCH) — *"stand and shoot at the last place you saw target. If the
  target moves, this behavior will finish quickly. If not, it will last a longish time, **giving
  the illusion of pinning the target down**."*
- **Grenade uncover** (PRESEARCH) — *"throw a grenade to flush the target from behind cover
  (**only after we've just lost sight of them**)."*
- **The SEARCH ladder** — `Uncover` (move to a firing point from which I *expect* to see them) →
  `Investigate` (go where I think they are) → `Pursuit` (not there; pick another hidden point) →
  `Postsearch` (give up; stand in the open near where we lost them).
- **Cover-peek** — *"lean out from behind wall and fire… **Duck back behind cover immediately if
  I'm hit**."* Sibling: **Group-emerge**, wait so we all leave cover together.
- **Charge when cornered** — *"If I have nowhere else to go, turn around and run melee-charge."*
- **Unreachable-enemy-cover** — *"If I'm being sniped by an enemy from beyond my maximum combat
  range, take cover."*
- **Scary-target-cover / Scary target retreat** — gated on the target being *"very scary"*.
  Scariness is a real number per character: Grunt 0, Elite 4, Brute Chieftain 6, Master Chief 7,
  **Hunter 16, Shielded Hunter 20**
  ([343/MS, *Halo 3 Leadership*](https://learn.microsoft.com/en-us/halo-master-chief-collection/h3/ai/leadership)).
- **Leader dead retreat** — *"**GRUNTS ONLY.** An elite died, and there are no more elites around
  me. Flee!"* Siblings: `Peer dead retreat`, `Danger retreat`, `Proximity retreat`, `Surprise
  retreat`, `Overheated weapon retreat`.
- **Danger** is a first-class numeric emotion, debuggable with `ai_render_emotions 1`.

Awareness is a ladder, not a boolean
([343/MS, *Halo 3 Combat Status*](https://learn.microsoft.com/en-us/halo-master-chief-collection/h3/ai/combatstatus)):
`asleep → idle → alert → active → uninspected → definite → certain → visible → clear_los → dangerous`.

### 1.6 Roles, not difficulties — and morale as the readable layer

Each species is a **different tactical problem**, expressed in the 2002 deck as *"Each race has a
**Black Box** for action selection · Grunts flee easily · Elites seek cover if hurt · Jackals carry
shields."* Halo 3 makes the break behaviour explicit per species (Leadership doc, verbatim):

- **Brutes → *Scatter***: flee to cover, or charge and melee if close; re-emerge in a few seconds.
- **Grunts → *Flee***: *"turn tail and flee for their lives … in the absence of cover they will
  just continue bumbling around wildly. Once they are no longer threatened they will calm down."*
- **Jackals → *Huddle***: crouch behind shields around a central point — *"a single well placed
  grenade or a quick meleeing spree will sort everything out."*

Leadership is two numbers (`Leadership Rank`, `Scariness`) plus a role: the leader takes rear
firing positions, followers *"form up in front of the leader, spreading out in a fan"*, and the
leader **formalises the combat cycle** — a pause before engaging, an order to attack, an order to
search, and *"to **uncover the target using a barrage of grenades**."*

Halo 3's Objectives system ([Isla, *Building a Better Battle*, GDC
2008](https://web.cs.wpi.edu/~rich/courses/imgd4000-b12/lectures/halo3.pdf) ·
[GDC Vault](https://gdcvault.com/play/497/Building-a-Better-Battle-HALO)) turns the 3-minute layer
into a declarative task tree — *"Pour squads in at the top… **Basically, it's a plinko machine**"* —
after Halo 2's explicit-transition FSM tool hit *"n² complexity"*. Its **Leadership case study** is
exactly two mechanisms: a *leader filter* on the core task, and a **task "broken" state** in which
*"Task does not allow redistribution in or out while broken"* and *"NPCs have broken behaviors."*

Isla's framing of the whole thing: *"The dance is about **the illusion of strategic intelligence**
… **Designer provides the strategic intelligence.**"*

### 1.7 The golden triangle — weakly sourced, and it doesn't matter

I could not find a first-party Bungie design document naming a "golden triangle". The closest is
Lars Bakken (Bungie, Halo 3 MP designer) quoted second-hand: *"the golden three things of Halo,
which are weapons, grenades, and melee"*
([Halopedia](https://www.halopedia.org/Weapons_(gameplay))). **Treat the term as community
vocabulary.** The *design content* is far better sourced from the behaviour list, where the AI
carries a counter for each vertex: `Grenade impulse` / `Dive impulse` / `Evasion impulse` (nade),
`Melee charge` / `Proximity melee` / `Charge when cornered` / `Stuck-with-plasma-berserk` (melee),
`Fight` / `Cover-peek` / `Suppressing fire` (gun). **The triangle is only a triangle because the AI
answers all three sides.** That is the transferable claim, not the slogan.

### 1.8 Difficulty: what the knob actually turns

Halo's difficulty has never been health-and-damage
([343/MS, *Halo 3 Difficulty Levels*](https://learn.microsoft.com/en-us/halo-master-chief-collection/h3/ai/difficultylevels)).
Four tag homes, and the `globals` field list is published in full:

- **Ranged fire:** `projectile error`, `burst error`, **`new target delay`** (the reaction knob),
  `burst separation`, `target tracking`, `target leading`, **`melee delay base`** (*"added to all
  melee attacks, **even when berserk**"*).
- **Grenades:** `grenade chance scale`, and `grenade timer scale` — *"delay period between grenades
  thrown from **the same encounter**"*. **Throttled at encounter scope, not per actor.**
- **Placement:** `major upgrade (normal/few/many)` — *"fraction of actors upgraded to their major
  variant"*. Difficulty **rerolls the roster upward**; a Major has different armour *and* *"may
  have different behavior properties (more likely to charge, etc)"*.
- **Task flags:** *"Tasks can be enabled and disabled depending on the difficulty level, meaning
  squads will occupy different areas, be more aggressive, timid, etc."* Difficulty is wired
  straight into the Objectives tree.

The single most useful sentence in the whole difficulty doc, on accuracy:

> "The most important thing about AI accuracy is **how the firing parameters blend from inaccurate
> to accurate at different rates** depending on difficulty level."

**Halo does not raise a hit chance. It changes how fast an accuracy ramp converges.** §3 ranks this
as BN's highest-value missing lever, and §4.5 explains why BN needs it *more* than Halo did.

Halo Infinite adds a token economy on top ([Halopedia,
*Difficulty*](https://www.halopedia.org/Difficulty) — **[S]**, community-derived from the editing
kits, carries its own accuracy disclaimer): **Global Kung Fu → Max Attackers** (a cap on how many
NPCs may attack the player at once), **Threat Tokens** (max high / max low / max engagement),
**Grenade Tokens** (max, min, clump threshold, cooldown), **Effectiveness Tokens**. Halopedia
attributes the shape to DOOM (2016). Co-op widens the token caps rather than the population cap;
Forge caps population separately at **32 active AI**
([343, *Forge AI Toolkit*](https://www.halowaypoint.com/news/forge-overview-season-5)).

**Not verifiable:** no source I could reach publishes the actual four-column numeric `globals`
table for any Halo title, Infinite included. Any "Legendary = 2× damage" claim is unsourced.
Equally, **no Bungie/343 statement exists asserting what difficulty deliberately leaves
unchanged.** The reading that the *behaviour tree, styles, objectives, perception model and firing
point selection* are off the difficulty knob is **INFERRED** from the tag layout plus the
"Smarter = Tougher" finding — it is a strong inference, but it is an inference.

### 1.9 What Halo Infinite changed

**Campaign** ([343, *Inside Infinite* October 2021](https://www.halowaypoint.com/news/inside-infinite-october-2021)),
Steve Dyck, Character & Combat Director:

- Roles inverted at the top of the roster: **Brutes are the backbone and they charge**, where
  Elites take cover. Elites were slimmed to read as *"fast, agile, and intelligent"*.
- The open world's real cost, stated plainly: *"The more open nature of the combat also meant our
  engineering team had to do a lot of work on behaviors and systems to account for **360 degrees of
  combat**."*
- *"Smarter AI will upgrade their weapon, when possible, by grabbing a better weapon off a rack."*
- Sandbox symmetry: *"Those new Fusion Coils the player can grab and throw can also be thrown by
  Brutes."*

**Legibility** is where Infinite genuinely advanced the state of the art, and it is almost all
*presentation*, not AI:

- **Armour as the health bar.** Brute plating *"deteriorates from sustained hits"*, head protection
  must be shot off before headshots register, and shields show as *"a notable sheen"* rather than a
  flash. Rank is encoded in armour colour and coverage — threat tier is readable before the first
  shot. ([Halopedia, *Jiralhanae/Gameplay*](https://www.halopedia.org/Jiralhanae/Gameplay) — **[S]**)
- **A per-frame audio threat mix.** Chase Thompson (Lead Audio Technical Designer): a system that
  *"detects all gun sounds frame by frame, and prioritizes them in a threat order to decide output
  sound volume for each gun"* — shots aimed **at you** are mixed louder, so players *"ascertain the
  appropriate threat to make the right action."* Sotaro Tojima adds that Halo 4/5's
  maximum-detail approach **failed** and *"we had to reduce sound density dramatically."*
  ([343, *Inside Infinite* March 2021](https://www.halowaypoint.com/news/inside-infinite-march-2021))
- **Barks announce the next action, not the mood** — Staten's cited example is a Grunt shouting
  *"Tossing a flare!"*

**Multiplayer PvP bots** ([343, *Inside Infinite* August 2021](https://www.halowaypoint.com/en-us/news/inside-infinite-august-2021))
are the closest analogue to BN that exists in a shipped game, and 343 was unusually candid:

- **Architecture is utility AI.** Brie Chin-Deyerle (Senior Lead Gameplay Engineer): *"We've broken
  down all the high-level actions for the Bots (like running the objective, getting a new weapon,
  engaging in combat, etc.). We then assign each of those actions a value that's based on a number
  of inputs, weighing each one a little differently, and then we choose what the optimal action to
  take at a given time is."* Note the irony: **Bungie's PvE AI rejected float utility (§1.5); 343's
  PvP bots are built on it.** BN's ambition layer is on the 343 side of that line.
  ([Kotaku](https://kotaku.com/why-halo-infinite-s-bots-act-so-much-like-people-1847511140)
  describes behaviour trees and a verb-at-a-time build order — likely the execution layer under the
  scorer; flagging the discrepancy rather than resolving it.)
- **The no-cheat law.** Sara Stern (Multiplayer Bots Designer): *"They need to move like players,
  shoot like players, use equipment like players, and so on… It's why we avoid allowing Bots to
  'cheat' by using information that players don't have access to."*
- **The tiers, by what they change:** Recruit *"don't react quickly in firefights"* · Marine
  comparable to a comfortable player, *"haven't mastered strafing"* · ODST *"react well to player
  movement"* + aggressive equipment use · Spartan, plus experimental **inter-bot communication
  about gameplay events** (shipped disabled). Movement scales as a ladder of verbs (Ilana Franklin):
  *"Lower difficulty Bots focus on strafing, medium difficulties can jump but won't crouch as
  often, higher skilled Bots can do both."*
- **Ceiling-first tuning** (Stern): *"we try to get the Bots to perform at a high skill level so we
  can identify what the 'ceiling' is for that behavior. We then reduce how effective they are for
  lower difficulties."*
- **Per-weapon aim points** (Franklin): *"They'll aim the sniper rifle at their target's head, but
  try to shoot rockets at their target's feet."* Hollis Lehv on grenades: *"If a grenade type
  bounces, Bots will throw it slightly in front of the target's feet… they do consider the ways
  different grenades bounce and by how much."*
- **Memory:** on losing contact, bots *"will remember where they last saw you and try to hunt you
  down."*
- **The shipped failures, named by 343.** Stern: *"They were predictable off initial spawn and often
  ignored important map pickups. These behaviors made it hard for Bots to compete against players
  who had fully stocked up on power weapons and equipment."* Chin-Deyerle: *"We did notice the
  Spartans did feel a little easier than the ODSTs"* — **the tier curve inverted at the top** — and
  *"seeing how many creative ways groups of humans could exploit certain Bot behaviors."* Fix list
  included stopping bots from pack-hunting the same weapon.
- **Integration was the sleeper cost** (Chin-Deyerle): *"getting the Bots to show up in the game
  like players. They show up in the back button scoreboard, earn medals, have MMR, customizations…
  it was a very long road."* BN got this free by making bots `ABNPlayerState` holders.

**Could not verify, and therefore not used below:** any 343 GDC talk on Infinite's combat AI; any
Infinite-specific leader-death / squad-collapse system; enemy nameplates or health bars on
high-value targets; Infinite's architectural lineage from the Halo 3 objectives system. **Any claim
that "Infinite still runs Isla's system" is unsourced.** What is defensible is that the *observable
behaviour vocabulary* is unbroken CE→Infinite; the *implementation* lineage is undocumented past
Halo 3.

---

## 2. What BN already has that matches

Read out of `Source/BreachpointNext/AI/` on 25 Aug, and the ground moved under this table while it
was being written. **`ROADMAP-10` opens with a blocker — `ST_BNBot.uasset` stale at 22 Aug, so Cover,
Evade and Strafe were compiled C++ that no bot ran. That asset is now stamped 25 Aug 17:49, and
`BNBotController` / `BNBotStateTreeTasks` were being edited at 17:44–17:51 as this was read.** So
`TICKET_BN10_BOT_ASSETS` appears to have been closed today by the lead.

Rows marked ⚠ below are the ones whose *runtime* status turned over in that window: the C++ behind
them is what this document read, and whether the shipped tree now selects them should be confirmed
from the rebuild's read-back rather than from here. Nothing in §3's ranking depends on it — but
nothing in §3 should be authored into that tree without re-reading it first.

| Halo mechanism | BN's equivalent | Verdict |
|---|---|---|
| Prioritised behaviour list, first relevant child wins | `Engage` as an ordered selector: Rearm → Arm → Nade → Knife → Cover → Close → Shoot, each with enter conditions, every completion landing back on `Root` to force a full re-selection | **Match, and the Root-landing rule is better than a naive GotoState.** `BNBotAuthoring.cpp:83-95` documents exactly why |
| Impulse that outranks everything (`Dive impulse`) | `Evade` sits **above** `Engage` with no health and no target condition | ⚠ **Match in C++.** R10.4's stated reasoning — *"a grenade at your feet outranks having a target"* — is Halo's impulse ordering rediscovered |
| "No cheating" perception, can be fooled | Sight + hearing perception; BN14 removed a genuine wallhack (`FindNearestValidEnemy` iterating every pawn with no range and no LOS) | **Match, recently.** It was broken until this week |
| Props: last-seen position, deliberately stale | `RememberThreatAt` / `GetLastKnownThreatLocation` / `HasFreshLastKnownLocation`, written by three sources (sight loss, being shot, hearing a noise) through one writer | **Match, and the "one writer for one fact" discipline is the right shape** |
| `Investigate` (go where I think they are) | `Search` state + `FBNSearchLastKnownTask` | **Match** — but it is the *only* rung of Halo's four-rung ladder BN has (§3.1) |
| Hearing as a place, never a target | R10.2: every shot and every blast reports itself; hearing range longer than sight | **Match, and well-reasoned.** *"You hear a fight through a wall you cannot see through"* is the same asymmetry Halo's `hearing_distance` buys |
| `new target delay` (reaction) | `ReactionSecondsMin/Max`, drawn once per acquisition, quantised, clamped at 200 ms | **Match with a hard floor Halo does not have** (R11) |
| Analog reactions: position, timing | `FBNStrafeTask` — perpendicular sidesteps, deterministic side, juke every Nth | ⚠ Written, not compiled in |
| `Charge when cornered` sibling: refuse to press into a wall | Strafe flips side on a refused step; Evade FAILS when cornered and lets the tree carry on | **Partial** — BN gives up gracefully where Halo turns and charges (§3.7) |
| `Danger cover impulse` (hurt + under fire → cover) | `FBNShouldTakeCoverCondition`: health < 60% **AND** `State.Combat.RecentDamage` **AND** off a controller-owned cooldown | ⚠ **Match on the trigger.** The rosette-of-8 navmesh samples traced on the weapon channel is a defensible small-scale substitute for firing points |
| Species roles | Four weapons with per-section damage rows | **The only BN-legal analogue** — see §4.2 |
| Halo Infinite's four bot tiers | `Recruit / Marine / ODST / Spartan` on `DT_BNBotTuning`, nine numbers moving together | **Names match. Contents partly match, partly violate R28** — see §4.4 |
| Bots are real players (scoreboard, medals, MMR) | `bWantsPlayerState = true`; bots ride `ABNPlayerState`, score, killfeed, respawn | **Match, and BN got for free the thing 343 called "a very long road"** |

**Two places this contradicts the standing doctrine**, recorded rather than argued in chat:

1. `BREACHPOINT-AI-BOTS.md` §1 lesson 1 says *"Halo 2 popularized the behavior tree — but the BT
   was the execution layer"* and that desire *"came from separate systems."* Isla's actual 2005
   paper describes the **opposite**: relevancy is **binary and inside the DAG**, chosen explicitly
   *to avoid* a float-utility desire layer. Halo's separate layer is **spatial and strategic**
   (orders, zones, objectives), not a utility scorer. BN's ambition layer is Infinite-bot shaped
   (§1.9), not Bungie shaped. That is fine — but the citation is wrong, and the honest version is
   *"we do what 343's PvP bots do, not what Bungie's PvE AI did."*
2. R12's text names *"break-off on shield-crack"* as *"the same signature move players learn from
   Halo."* **BN's shields are off** and BN has no health regen, so that move cannot exist here and
   the example in a closed ruling is now stale (§4.3).

---

## 3. What BN is missing, ranked by value per line of code

Ranked on *behaviour a player can see, divided by diff size*. Everything here stacks behind
`TICKET_BN10_BOT_ASSETS` — nothing new should be authored into a tree that is already four packets
behind.

### 3.1 — Aim at a PLACE. (The one primitive that unlocks three Halo behaviours.)

**The gap.** Every BN task that points the bot points it at an *actor*: `FBNFaceTargetTask` takes
`GetCurrentTarget()`, `JitteredFocalPoint` takes `Target->GetActorLocation()`, `FBNCanThrowGrenade
Condition` requires `HasLineOfSightToTarget()`, and `Shoot` carries `FBNHasLineOfSightCondition`.
**A BN bot cannot aim at anywhere it cannot currently see a body.** That single fact blocks the
three most legible behaviours in the entire Halo behaviour list:

- **Grenade uncover** — *"throw a grenade to flush the target from behind cover (only after we've
  just lost sight of them)"*. This is the most readable act an FPS AI can perform: the player is
  behind a wall, safe, and a grenade lands next to them. It converts BN's `Search` from a walk into
  a threat.
- **Suppressing fire** — *"stand and shoot at the last place you saw target… giving the illusion of
  pinning the target down."* Bungie says out loud that this is an *illusion*; it costs one burst
  aimed at a vector.
- **`Uncover`**, rung 1 of the search ladder — move to a point from which I *expect* to see them,
  rather than to where they were. (Higher cost; needs the cover rosette inverted. Defer.)

**The change.** A `FaceLocation`-shaped task (Epic's `Variant_Shooter` ships one, per the existing
research doc) plus an override on the grenade condition and the burst that reads
`GetLastKnownThreatLocation()` when there is no live LOS. `HasFreshLastKnownLocation()` already
exists and already carries the staleness rule. The grenade ability aims through
`GetPlayerViewPoint`, so pointing the controller *is* aiming — no new grenade code.

**Why it is #1.** One primitive, three behaviours, all of them things a player narrates out loud.
Straight R12: legible before optimal.

### 3.2 — Grenade budget at the ENCOUNTER, not the actor

**The gap.** `FBNCanThrowGrenadeCondition` checks a per-bot `Cooldown.Grenade` and a per-bot pouch.
With `TargetPlayers=8` there is nothing stopping four bots nading the same player in the same
second. Halo throttles this at encounter scope on purpose — `grenade timer scale` is *"delay period
between grenades thrown from **the same encounter**"* — and Infinite promoted it to a whole token
pool with a **clump threshold**.

**The change.** One timestamp on the GameState (or GameMode), one `Max(...)` in the condition.
Roughly ten lines. **This is the cheapest item on the page and it removes a failure mode BN will
hit the first time eight bots run.** BN already has the precedent: R10.2 made a shotgun's pellets
one noise for exactly this "don't multiply the same event" reason.

**One question it forces**, since BN has no encounters: is the budget global to the match, or
per-victim? Per-victim is the honest translation (*"don't clump on one player"*) and is the same
one timestamp, on the target's PlayerState. §5.

### 3.3 — The accuracy RAMP, replacing the fixed cone

**The gap.** `JitteredFocalPoint` draws inside a fixed `AimErrorDegrees` cone and re-draws every
`ReaimSeconds`. The error never converges. A player who holds still is no more likely to be hit at
second four than at second one; a player who dodges gains nothing measurable.

**Halo's version:** *"how the firing parameters blend from inaccurate to accurate at different
rates depending on difficulty level."* The knob is **convergence rate**, not hit chance.

**The change.** Scale the cone by a term that decays with time-on-target and **resets on LOS
break, on target change, and on the bot being hit**. One extra field on `FBNBotTuningRow`, one
multiplier in `JitteredFocalPoint`, one reset alongside the existing `SecondsUntilReaim` bookkeeping.

**Why it matters more to BN than to Halo.** R11 puts a 200 ms floor under reaction time, which is
Halo's most-used tier lever (`new target delay`) — so BN can only ever scale reaction *upward* from
human. That removes most of the range from the lever Halo leans on hardest, and the ramp is the
obvious replacement: it is **fully R28-legal** because it is a lever a player can read and learn to
beat (*break line of sight and he loses his zero*), and it makes the existing R10.1 note — *"a
LONGER redraw is easier to fight, not harder"* — into a real mechanic instead of a comment.

### 3.4 — Posture as the state channel ("Discarded: Hidden States")

**The gap.** A BN bot's *only* outward signal is where its gun points. Ambition, cover, search and
evade are invisible. That is the exact thing the 2002 deck discards, and R12 exists to forbid it.

**The change, and why it is nearly free.** BN already has the verbs *and* the replicated tags:
`SetCrouching` and `SetSprinting` exist on the controller, `State.Movement.Crouching` /
`Sprinting` are already applied by the same abilities a human's keys drive, and the animation
already reads them. Nobody has spent them as *signals*:

- **Crouch on arrival in cover** — the universal "I am hiding" pose. `FBNTakeCoverTask::Tick`
  already knows `bArrived`.
- **Sprint while searching, walk while suppressing** — the difference between "hunting" and
  "pinning" becomes visible at 40 m.
- **`UBNGA_LeanLeft/Right` exist, hold `State.Lean.*`, and no bot has ever pressed either.**
  Halo's `Cover-peek` is *"lean out from behind wall and fire… duck back immediately if I'm hit"* —
  BN has the ability, the tag and the input already built. **INFERRED:** bots are granted the
  default ability set (R9.5 established this for `Input.Jump`), so lean is probably already in
  their grant; that is one grep for whoever picks this up.

**Deliberately NOT barks.** Halo's own numbers are 5,147 recorded lines and the deck labels combat
dialogue *"Used for flavor only"*; Infinite's audio director says the maximum-detail approach
**failed**. BN has no VO pipeline and no VO budget. Posture is the channel the 2002 deck lists
first and the only one BN can afford.

### 3.5 — Per-weapon aim point

**The gap.** `JitteredFocalPoint` aims at `Target->GetActorLocation()` — the actor origin, roughly
the pelvis — for every weapon in the game. BN already models per-section damage
(`HeadshotMultiplier`, `TorsoMultiplier`, …) on `FBNWeaponRow`, so the *reward* for aiming
differently exists and no bot ever collects it.

**The change.** One column on `FBNWeaponRow` (an aim-point offset, or a section name), one lookup
in `JitteredFocalPoint`. Infinite's exact shape: *"aim the sniper rifle at their target's head, but
try to shoot rockets at their target's feet."*

**Why it earns its place.** It is the smallest change that gives BN's four weapons four different
*threat profiles* — which, per §4.2, is the only role system BN is allowed to have.

### 3.6 — Cover that fights, and reload discipline

**The gap.** `FBNTakeCoverTask` moves to a point, holds, and leaves. It never shoots. And `Rearm`
sits **above** `Cover` in `Engage`, so a bot reloads in the open and *then* considers hiding —
backwards, and already BN14's ranked item #5.

**The change.** Reorder two `AddChildState` calls; then a peek/burst/duck loop inside the hold,
using §3.4's lean. Halo's `Cover-peek` and `Group-emerge` are the reference. Medium size, high
visibility — cover that never returns fire reads as a bot running away, not a bot using the map.

### 3.7 — Charge when cornered

**The gap.** BN's terminal states are all *give up*: Evade fails when cornered, cover fails when
none is found, Survive flees. Halo's is *"If I have nowhere else to go, turn around and run
melee-charge."*

**The change.** One condition — Survive **and** cover search failed **and** low health — routing to
`Knife`. Ten lines against existing states. Its value is legibility of a specific kind: it gives a
losing bot a *readable last act* instead of a walk toward a wall.

### 3.8 — Target SELECTION, with a scariness number

Already BN14's ranked #4. Halo's mechanised form is the `Scariness` column (Grunt 0 … Shielded
Hunter 20) feeding `Scary-target-cover`. BN's honest FFA translation is *held weapon + current
health + distance* — a bot should fear the player holding the strongest weapon, and should finish
the one who is nearly dead. Larger diff, and it is worth stating that Halo's version is a **static
per-character constant**, not a runtime evaluation; BN's would have to be runtime. Ranked below the
one-line items on purpose.

### 3.9 — The 3-minute layer, gated on content not code

BN has the 3-second loop (GAS verbs) and the 30-second loop (StateTree). It has **no 3-minute
layer**, and 343 named this as their bots' worst shipped failure: *"predictable off initial spawn
and often ignored important map pickups."* `ABNPointOfInterest` is a name and a radius; Roam picks
by distance. Weighting POIs (and giving them respawn timers) is small code — but it means nothing
until the arena has things worth wanting. **This is a level-content dependency, not an AI packet**,
and it should not be scheduled as one.

---

## 4. What does NOT transfer, and why

This section is the point of the document. Each item is something a reasonable person would
propose after reading §1, and each is wrong for BN.

### 4.1 Morale, leader death, and squad break — **structurally impossible**

Halo's single most-quoted AI behaviour is *"An elite died, and there are no more elites around me.
Flee!"*, and Halo 3 generalises it into a task **"broken" state**. Every version of it needs two
things BN does not have: **a squad**, and **a rank within it**. BN is FFA — R8's teams landed and
were reverted at the founder's call, and `GetGenericTeamId` returns 255 for everyone with every
other pawn Hostile. **There is no leader to kill and no peer to be demoralised by.**

The tempting FFA residue — *"a bot that watched two people die nearby becomes cautious"* — is not
the same mechanism and should not be built. Halo's version is legible because the player **caused
it and can see the cause**: you shot the Elite, the Grunts ran. A bot that turns timid because of
deaths it merely witnessed is a bot whose state change the player cannot attribute to anything they
did. **That is precisely the illegibility R12 forbids**, and it would read as random cowardice.
Teams are the prerequisite for every squad behaviour in Halo's playbook, and reverting R8 correctly
put all of them out of scope.

### 4.2 The species/role system — **BN is a mirror, not a menagerie**

Grunt / Elite / Jackal / Brute / Hunter is a **role** system, not a difficulty system, and it works
because the roles are **written on the bodies**: a Jackal has a shield you can see, a Hunter has
`Scariness 20` and armour plating, a Grunt is small and panics. The player reads the role at a
glance and picks a tool.

**BN is a mirror-match arena.** Every fighter is an `ABNCharacter` with the same health, the same
four weapons, the same abilities, the same silhouette. Porting "roles" as *behavioural* archetypes
— the rusher, the camper, the flanker — puts a difference into the world that **has no visible
carrier**, which fails R12 exactly the way R28 says a per-tier sight radius fails it: two
identical-looking bots behaving differently, with nothing on screen explaining why.

**The one legal analogue is the weapon in the bot's hands.** It is visible, the player already reads
it, BN already models it per-row, and §3.5 is the change that makes it mean something. *"Halo's role
system, ported honestly to BN, is: bots commit to a weapon identity and fight the range that weapon
wants."* Anything beyond that needs a visible carrier first.

### 4.3 Break-off on shield-crack — **the economy it rides on does not exist**

Halo's retreat loop is a **shield economy**: shields crack → break contact → recharge → return.
`Low-shield-retreat` and `Danger cover impulse` both key on it. R10.3's write-up already noticed
half of this — *"what Halo Infinite gets free from its shield economy"* — and correctly said BN's
trigger has to be spoken out loud instead.

The other half has not been said: **BN has no regeneration of any kind.** A BN bot that breaks off
does not come back stronger; it comes back with exactly the health it left with, having spent the
trip not shooting. **Retreating in BN is strictly dominated by fighting**, which is why R9's move of
`InterruptBelowHealthNorm` from 0.35 to 0.15 was right, and why importing Halo's retreat thresholds
would make bots worse *and* less legible. R12's citation of shield-crack break-off as BN's signature
move is stale and should be amended when the ruling is next touched.

### 4.4 Per-tier perception — **closed by R28, and currently in violation**

Infinite's bots gate perception by tier (higher tiers *"can actually make use of the game's radar"*
— [Kotaku](https://kotaku.com/why-halo-infinite-s-bots-act-so-much-like-people-1847511140), **[S]**),
and Bungie's `actor` tag has carried `max_vision_distance` / `peripheral_vision_angle` per race
since Halo 1 ([c20](https://c20.reclaimers.net/h1/tags/actor/), **[S]**).

**BN may not do this.** R28: *"`sight_radius_m` and `sight_fov_deg` are identical across all tiers
and stay that way"*, for three stated reasons ending in the mechanical one — sight radius is also
the normaliser for `dist_to_target_norm`, so varying it makes identical geometry produce different
facts per tier.

**This document proposes nothing per-tier-perceptual, and it also does not get to look away from
the fact that R10.1 already shipped it**: sight 900/1200/1500/1800 and FOV 55/70/85/100 across the
four tiers, with `RescoreBrain` normalising by `GetTuning().SightRadius`. BN14 has this open as a
HIGH needing a founder ruling. **Everything ranked in §3 is deliberately chosen to work regardless
of how that ruling goes** — the ramp (§3.3), posture (§3.4), aim points (§3.5) and the grenade
budget (§3.2) are all levers a player can read, which is the R28 test.

### 4.5 Halo's reaction-time range — **half of it is below BN's floor**

`new target delay` is Halo's most-used difficulty scalar and it has no floor; Infinite's Recruit is
*defined* by not reacting quickly, and the top tier is defined by reacting fast. R11 makes 200 ms a
**law**, clamped at `ABNBotController::DrawReactionSeconds` after BN14 found Spartan shipping at
0.08–0.16 s. So BN can only scale reaction *upward from human*, and the top of Halo's range is
permanently unavailable.

**Consequence, stated so nobody re-derives it later:** BN's tier ladder has to buy its top end
somewhere other than reaction speed. §3.3's ramp, §3.4's movement-verb ladder (Infinite's own
*"lower bots strafe, medium jump, higher do both"*), and §3.6's cover discipline are where that
range has to come from. **Proposing a faster Spartan is proposing to reargue R11.**

### 4.6 "Smarter = Tougher" — **the currency Bungie spends is not BN's to spend**

The 2002 playtest table is the strongest empirical claim in this entire research: **buy perceived
intelligence with durability.** In a PvE campaign a tougher Elite is a better fight. In BN, "tougher
bot" means *a bot with more health than the human standing next to it*, in a mirror-match FFA where
the killfeed and scoreboard make every asymmetry visible. GDD §2.8's "no privileged state" and R28
both close it. **Note it, admire it, do not spend it.**

### 4.7 Designer-authored firing points, hint volumes, and the objectives task tree

Halo's spatial intelligence is *authored* — zones, areas, hand-placed firing points, and thirteen
kinds of hint. Halo 3's plinko machine exists to solve a problem Isla names precisely: Halo 2's
explicit-transition tool hit *"n² complexity"* across **many** encounters authored by **many**
designers. BN has one arena and one designer. R10.3's rosette of eight navmesh samples traced on
the weapon channel is the correctly-sized answer, and its own justification is right: EQS *"would
ask it more expensively and no more truthfully."* **Do not build an objectives system.** The
authored-space lesson BN should take is the cheap half — `ABNPointOfInterest` is BN's hint volume,
and §3.9 is what it is missing.

### 4.8 Max Attackers / engagement tokens — **PARTIAL, and a design question not a code question**

Infinite's "Global Kung Fu" cap on simultaneous attackers is a genuine, cheap, DOOM-proven answer to
360° combat, and BN with eight fighters in one arena will hit the crowding it solves. But in **PvE**
it is a pacing device, and in **PvP** it is bots agreeing not to shoot you — a protection no human
opponent extends. It reads as the bots pulling punches the moment a player notices it. §3.2's
grenade budget is the version that survives the objection (a grenade *clump* is an unfair
combination, not an unfair number of opponents); a general attacker cap is a founder call. §5.

### 4.9 Combat dialogue and the audio threat mix — **right, but not this discipline's**

Infinite's per-frame gun-sound threat prioritisation is the best-documented legibility system in
this research and it is genuinely reusable. It is also **entirely player-side audio**: it changes
the mix, not a single AI decision. It belongs to an audio ticket. Filed here so it is not lost, and
explicitly **not** ranked in §3 — putting it in an AI packet would be the sort of scope smear the
owner-path law exists to stop.

### 4.10 "Discarded: Randomness" — an inversion worth reading twice

Halo rejected RNG-driven variety because *reactive* variety was better and free. BN's §5
determinism law points the same direction — and BN currently **fails it**: BN14 found seeds are
`GetTypeHash(this)` (a pointer hash) and `GFrameCounter`, the header concedes "stable within a run,
not across one" while the class comment claims determinism, and `Breachpoint.Bots.*` — the suite R11
and DoD #9 both name as the pin — **does not exist**. Halo's line is not an argument against BN's
aim cone; it is an argument that BN's variety should come from §3.1's flushing and §3.3's ramp
rather than from a wider cone, and that the seeds should be fixed before either lands.

---

## 5. Open questions — founder calls, not packets

1. **R28 vs R10.1 (blocking, already HIGH in BN14).** Per-tier sight and FOV shipped against a
   closed ruling. Either R10.1 is in violation or R28 needs a dated amendment. Nothing in §3 depends
   on the answer, by design — but no further tier work should land until it is answered.
2. **What is "the encounter" for a grenade budget in an FFA?** Match-global, or per-victim? §3.2
   recommends per-victim (one timestamp on the target's PlayerState) as the honest translation of
   *"clumping"*. Founder's call on which.
3. **Is a simultaneous-attacker cap fairness or handicap?** §4.8. In PvE it is pacing; in a mirror
   PvP arena it is bots declining to shoot. This is a game-feel decision, not an engineering one.
4. **Does R12's shield-crack example get amended?** §4.3 — the move it names cannot exist while
   shields are off. Rulings are judged against, never re-litigated inside a packet, so this needs a
   dated amendment or an explicit "leave it, it's illustrative".
5. **Ceiling-first or floor-first tuning?** 343's method is *build the bot at maximum skill, measure
   the ceiling, then subtract for lower tiers.* BN built Marine first (the founder's arena tuning)
   and scaled both directions from it. Marine-first is defensible and it is what preserved the
   arena's measured sight numbers — but it means **nobody has ever seen what a BN bot can do at full
   strength**, so nobody knows whether Spartan is near a ceiling or nowhere close. Infinite shipped
   with its tier curve **inverted at the top** (Spartan easier than ODST) and caught it only with
   telemetry. BN has no telemetry and no `Breachpoint.Bots.*` suite.
6. **Does the 3-minute layer exist at all in BN?** §3.9 is gated on the arena having things worth
   wanting. Until there are pickups or power positions, POI weighting is code with nothing to weigh.

---

## Source ledger

**Primary (Bungie/343 employee, GDC talk, or official docs).**
[Butcher & Griesemer, GDC 2002 deck (PDF)](https://www.jmeiners.com/shamans/papers/ai/the_illusion_of_intelligence.pdf) ·
[GDC Vault 1022590](https://www.gdcvault.com/play/1022590/Creating-the-Illusion-of-Intelligence) ·
[Isla, *Handling Complexity in the Halo 2 AI*, GDC 2005](https://www.gamedeveloper.com/programming/gdc-2005-proceeding-handling-complexity-in-the-i-halo-2-i-ai) ·
[GDC2005Isla audio](https://archive.org/details/GDC2005Isla) ·
[Isla, *Building a Better Battle*, GDC 2008 slides](https://web.cs.wpi.edu/~rich/courses/imgd4000-b12/lectures/halo3.pdf) ·
[GDC Vault 497](https://gdcvault.com/play/497/Building-a-Better-Battle-HALO) ·
[Isla at Develop 2008, reported](https://www.gamedeveloper.com/game-platforms/in-depth-bungie-on-eight-years-of-i-halo-i-ai) ·
[Griesemer, GDC 2010, *Design in Detail*](https://gdcvault.com/play/1012211/Design-in-Detail-Changing-the) ·
[Griesemer interview, Engadget 2011](https://www.engadget.com/2011-07-14-half-minute-halo-an-interview-with-jaime-griesemer.html) ·
343/Microsoft republished Bungie internal docs:
[H2 AI Behavior List](https://learn.microsoft.com/en-us/halo-master-chief-collection/h2/ai/aibehaviorlist) ·
[H2 AI Engineering Outline](https://learn.microsoft.com/en-us/halo-master-chief-collection/h2/ai/aiengineeringoutline) ·
[H3 Leadership](https://learn.microsoft.com/en-us/halo-master-chief-collection/h3/ai/leadership) ·
[H3 Combat Status](https://learn.microsoft.com/en-us/halo-master-chief-collection/h3/ai/combatstatus) ·
[H3 Difficulty Levels](https://learn.microsoft.com/en-us/halo-master-chief-collection/h3/ai/difficultylevels) ·
[Inside Infinite, Jan 2021 (sandbox)](https://www.halowaypoint.com/news/inside-infinite-january-2021) ·
[Inside Infinite, Mar 2021 (audio)](https://www.halowaypoint.com/news/inside-infinite-march-2021) ·
[Inside Infinite, Aug 2021 (**the bots**)](https://www.halowaypoint.com/en-us/news/inside-infinite-august-2021) ·
[Inside Infinite, Oct 2021 (characters & combat)](https://www.halowaypoint.com/news/inside-infinite-october-2021) ·
[Forge AI Toolkit, S5](https://www.halowaypoint.com/news/forge-overview-season-5).

**Secondary.**
[Halopedia — Difficulty](https://www.halopedia.org/Difficulty) (community-derived from editing kits;
carries its own accuracy disclaimer — the source for Infinite's token taxonomy) ·
[Halopedia — Jiralhanae/Gameplay](https://www.halopedia.org/Jiralhanae/Gameplay) ·
[Halopedia — Weapons (gameplay)](https://www.halopedia.org/Weapons_(gameplay)) (the Bakken "golden
three things" quote) ·
[c20 — Halo 1 actor tag](https://c20.reclaimers.net/h1/tags/actor/) ·
[c20 — Halo 3 AI](https://c20.reclaimers.net/h3/engine/ai/) ·
[Kotaku on Infinite's bots](https://kotaku.com/why-halo-infinite-s-bots-act-so-much-like-people-1847511140) ·
[Game Informer on Infinite's bots](https://gameinformer.com/preview/2021/11/15/how-halo-infinites-bots-became-so-ruthless-and-helped-343-develop-multiplayer) ·
[Thompson, *The Encounter Design of Halo 3*](https://www.gamedeveloper.com/design/combat-evolved-the-encounter-design-of-halo-3).

**Explicitly NOT verified — do not cite these as fact anywhere downstream.**
Numeric per-difficulty `globals` values for any Halo title, Infinite included · any Bungie/343
statement of what difficulty *deliberately* leaves unchanged (§1.8's invariant list is **INFERRED**)
· "golden triangle" as first-party Bungie terminology · Halo Infinite's AI architectural lineage
from Isla's systems · any Infinite-specific leader-death / squad-collapse behaviour · enemy
nameplates or health bars on Infinite high-value targets · any 343 GDC talk on Infinite combat AI
(343's GDC 2022 slate was rendering and world-building, not AI).
