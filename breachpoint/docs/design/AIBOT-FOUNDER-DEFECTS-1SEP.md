# The six bot problems — diagnosis and the course of action

> Cut 1 Sep 2026 by the cloud lead from the founder's watch-list, after reading the code
> behind each one. Every "why" below is a file and a line, not a theory. Nothing here is
> fixed yet — this is the plan and the order, and it exists in the repo rather than in a
> transcript because a decision that lives only in chat is lost (CLAUDE.md).

**The headline:** four of the six are *bugs with a named line*. One is a **config
change** that costs nothing. One — target selection — is the only real architecture
gap, and it is the one the founder described twice from two directions (getting shot,
and switching targets), which is the tell that it matters most.

---

## F1 — Bots walk and run backwards

**What the founder sees.** Bots travel sideways and in reverse instead of turning to face
where they are going. Acceptable in a fight; wrong everywhere else.

**Why.** `ABNCharacter` is an FPS pawn: `bUseControllerRotationYaw = true`,
`bOrientRotationToMovement = false` (`BNCharacter.cpp:171,176`). The body's yaw comes
**entirely** from the control rotation. And `TickLocomotion`
(`AIBStateTreeTasks.cpp:302`) — the one function every mover calls — handles sprint and a
wedge jump and **never touches the control rotation at all**. So a bot walking a path
keeps facing wherever it last aimed, and the path takes the body sideways.

The old BN system has a "FACE THE WALK" block for exactly this
(`BNBotStateTreeTasks.cpp:1191`). The AIB module was written without it.

**The fix.** Steer the control rotation toward the travel direction inside
`TickLocomotion`, at a turn rate, and gate it on **aim ownership**:

- no target, or the target is not visible → face the walk, always
- target visible and inside the fight band → aim owns the yaw, backpedal is correct
- retreating/evading → aim owns the yaw (running away while facing the threat is the
  point)

This makes the rule "the bot faces where it is going **unless something is holding its
aim**", which is also how a human plays. One function, one new gate, no data.

**Watch for:** the host may gate sprint on forward input. If so, F1 also silently fixes
bots that were sprinting nowhere.

**Cost:** small. **Risk:** low. **Visible immediately.**

---

## F2 — Platforms: up, down, and across

**What the founder wants.** Bots should get between platforms by *jumping* and *dashing*,
drop down to a lower platform deliberately, and use the *grapple* for height — including
crossing to a platform with no path at all.

**Why it does not work today.** Three separate causes, and only the third is known:

1. **`BN_Climb` can only climb 0.70 m.** `Config/DefaultEngine.ini:347` —
   `JumpHeight=90`, `JumpMaxDepth=-70`. That is the pawn's real apex (BN32 measured
   0.90 m), so it is *honest*, but **BR_Spillway's tiers are 4 m apart** (BN37's own
   commit says "up is ramps only"). There are effectively **no generated climb links on
   the map the game now boots into.** Nav links cannot solve up. That is not a bug to
   fix — it is a fact that says the other two verbs have to carry it.
2. **Dash is not a traversal verb.** `Verb_Dash` is pressed in exactly two places
   (`AIBStateTreeTasks.cpp:1037` blast scatter, `:2008` combat footwork). Nothing ever
   dashes to *go somewhere*. The gap-crossing the founder describes does not exist.
3. **Grapple traversal exists and half-whiffs** — AIB19's open finding, now instrumented
   (route ids + REFUSED/SHORT split landed 1 Sep). One PIE match tells us which fix it
   needs.

**The fix — one traversal chooser, three verbs.** The right shape is NOT three
independent behaviours. It is a single "I want to be over there and the navmesh has no
path" decision that picks a verb by geometry:

| Situation | Verb |
|---|---|
| target platform ≤ 0.90 m up, ≤ ~2.5 m across | **jump** (already works; links cover it) |
| gap across, roughly level, within dash reach (~500 uu, measured not assumed) | **dash** — new |
| target platform above jump reach, a grapple route serves it | **grapple** (exists, AIB19) |
| target platform below, drop survivable | **step off** — `BN_Drop` covers 10 m down already, but the bot must be *willing*, which is a decision, not a link |

**Ordering inside F2.** Do the measurement first: a scratch gym that records the pawn's
real dash distance, jump reach and safe drop (this is **BN32's rig, already committed and
unrun**). Building a traversal chooser against guessed reach numbers is how we get another
"ramps to nowhere".

**Cost:** the largest of the six. **Risk:** medium — this is new behaviour, not a repair.
**Do it after F1 and F3, and only on measured numbers.**

---

## F3 — Being shot, and choosing a target  *(the real gap)*

The founder raised this as two complaints. It is one missing layer.

**What exists.** Getting shot *is* wired: `BNHealthComponent.cpp:44` →
`NoteDamageTaken` → a stimulus on the reaction clock → matured into **memory**
(`AIBBotController.cpp:482`, `AIBSensorium.cpp:64`). The comment is explicit about the
design: *"being shot makes a bot go and look, never lock on."* It is also a **bearing,
not a position**, so a sniper's exact perch is never handed over — that part is right and
should stay.

**What does not exist: target SELECTION.** `FAIBSensorium::Pump` keeps a single
`VisibleTarget` slot and the newest matured `SightGained` **overwrites it**
(`AIBSensorium.cpp:126`). There is no score, no comparison, no persistence, no switch
rule. Damage explicitly *cannot* set the target (`:151` — it only writes memory, and only
when there is no visible target).

So the founder is right on both counts, for one reason: **the bot has no opinion about
who its enemy is.** It has whoever it saw last.

**The fix — a target selection layer in the sensorium, scored and hysteretic.**
Score every believed enemy and hold the winner; switch only when a challenger clears a
margin. The terms the founder named, in their words:

- **proximity** — closer is more urgent
- **is shooting me** — recent damage from this actor is a large, decaying term. This is
  the direct answer to "it doesn't know who is shooting him", and it changes the closed
  ruling from *never lock on* to **may re-score, still may not teleport knowledge**: the
  attacker becomes a strong *candidate*, not an instant lock, and only within the sight
  envelope it could honestly have.
- **visible now** vs remembered — a live sight outscores a stale memory
- **persistence bonus for the current target** — this IS the hysteresis. The founder was
  explicit: *"I would try to be as persistent as possible, realistically, to kill this
  target."* The incumbent carries a bonus, so switching requires the challenger to be
  clearly better, not marginally.
- **the incumbent decays** when it is far, unseen, or has broken away — which is exactly
  *"if he managed to run away."*

**This is the FAIRPLAY-sensitive one.** Two rules must not bend: a target the bot could
not perceive may never be selected, and damage gives a **bearing**, not a position. The
score changes *who the bot prefers among enemies it already believes in* — it must never
manufacture belief.

**Cost:** medium, and it is the highest-value change on this list. **Risk:** medium —
needs its own headless spec (the sensorium already has one) and a critic pass against
FAIRPLAY before it goes near a match.

---

## F4 — Spartan kills the player too fast

**What the founder wants.** *"A little bit more fair. Just a little bit."* Not easy.

**Why it is happening.** `Config/DefaultGame.ini:370` — `BotTier=Spartan`, set by the
founder on 29 Aug. Spartan is **Expert on all six axes** with:

| Knob | Spartan | ODST | Marine |
|---|---|---|---|
| aim error at draw | 1.2° | 2.2° | 4.0° |
| settle time | 0.45 s | 0.70 s | 1.0 s |
| aim floor | 0.40° | 0.65° | ~1.1° |
| reaction | **rides the 0.20 s module floor** – 0.28 s | 0.22 – 0.34 s | 0.22 – 0.60 s |

At 1500 uu an Expert's floor is ~10 uu of error — effectively perfect — and it draws at
the fairness floor. That is not a bug. It is the top rung doing its job.

**The fix — one line first: `BotTier=ODST`.** Every difference between the rungs is data
in `AIBTiers.cpp`; nothing in the brain, the tree, or the tasks knows which tier it runs.
That is what makes flipping it safe, and it is fully reversible. ODST is still Skilled on
every axis — a real opponent, not a punching bag — with roughly **double the aim error, a
50% longer settle, and a reaction that no longer rides the floor.**

**Only if ODST still reads unfair** do we touch numbers, and then the honest knob is
`ReactionSecondsMin`/`Max` and the aim settle — never the perception radii, because
shrinking what a bot can *see* makes it stupid rather than fair.

**Cost:** one line. **Risk:** none. **Do this first — it is the cheapest real change on
the list and the founder can judge it in one match.**

---

## F5 — Bots end up holding nothing

**What the founder sees.** Bots standing around with no weapon.

**Why — and this one is a genuine dead end, not a slow reaction.** The scorer the founder
asked for **already exists and already does what they described**:
`AIBScoreWeaponAtRange` (`BNAIBAvatarAdapter.cpp:45`) scores every carried weapon as
damage × shot count × falloff × hit-fraction ÷ fire interval, penalises an empty
magazine, and returns the best. The word "shotgun" appears nowhere — a shotgun wins up
close because spread makes it win, which is the correct way to build it.

The failure is in the **cycling**, in two places:

1. **`bHasDistance` gates the entire swap block** (`AIBStateTreeTasks.cpp:782`). A bot
   with no target has no distance and therefore **never swaps at all**. A bot that
   spawned holding the null Unarmed slot stays unarmed until it sees someone.
2. **The cycle can strand on the Unarmed slot, permanently.** The carry contains a
   deliberate null slot. Cycling is capped at 5 presses. When the cap is hit the wheel
   stops wherever it landed — and if that is Unarmed, `IsBestWeaponForRange` returns
   `AIBWeaponCanFight(Current)` = **false** (`BNAIBAvatarAdapter.cpp:410`), so the
   "settled" reset at `:804` **never fires**, the budget never refills, and the bot is
   unarmed for the rest of its life.

**The fix.**

- **Never rest on nothing.** If the held weapon cannot fight and *anything* carried can,
  the cycle must continue regardless of the press cap. The cap exists to stop a bot whose
  whole loadout is dry from spinning forever — that case is "nothing carried can fight",
  which is a different test than the one being used.
- **Swap without a target.** Drop the `bHasDistance` gate for the no-weapon case: a bot
  holding nothing should draw *something* while it walks, at a default range, not wait to
  be shot at.
- **Then the founder's ladder falls out of the existing scorer for free** — shotgun
  close, rifle mid, pistol as the fallback, because that is what the numbers already say.
  If it does not, the fix is the **weapon table**, not the bot.
- **Dry means melee or grenade.** Both policies exist (`AIBMeleePolicy`,
  `AIBGrenadePolicy`). What is missing is the *link*: "no weapon can fight" should raise
  melee's and the grenade's standing, not leave the bot passive. Grenades stay on their
  own recognition ladder (opener / finisher / area) — the founder's "evasive and
  surprising scenarios, maximum damage" is what that ladder already encodes.

**Cost:** small — two conditions and one link. **Risk:** low. **High visible payoff.**

---

## The order, and why

| # | Work | Cost | Why here |
|---|---|---|---|
| 1 | **F4 — `BotTier=ODST`** | one line | Free, reversible, and the founder can judge it in one match. Do it before anything else so every later observation is made against fair bots. |
| 2 | **F5 — never hold nothing** | small | A bot with no weapon makes every other behaviour unobservable. Fix the dead end before measuring anything. |
| 3 | **F1 — face the walk** | small | Immediately visible, low risk, and it changes how every other behaviour *reads* in a match. |
| 4 | **F3 — target selection** | medium | The real gap, and the one with a FAIRPLAY surface. Needs a spec and a critic pass. Highest value. |
| 5 | **BN32's rig — measure the pawn** | small, unrun | Already committed. Nothing in F2 should be built on guessed reach numbers. |
| 6 | **F2 — the traversal chooser** | large | New behaviour. Built last, on measured numbers, with AIB19's whiff diagnosis in hand. |

**One honest note on all of it.** 1, 2, 3 and 5 are cloud-writable today. 4 needs a spec
the cloud can also write. **None of them are proven until someone runs a match** — the
cloud has no engine, and every claim below the PIE rung is "written", not "works".

---

# WHAT LANDED — 1 Sep, all six, in the recommended order

Founder: *"Do all that."* Every item below is **WRITTEN, NOT COMPILED** — the cloud has
no engine. Terminal owes rung 1 on all three targets plus four new headless suites.

| # | Item | What landed | New suite |
|---|---|---|---|
| 1 | F4 tier | `BotTier=ODST` + the reasoning table in the ini | — |
| 2 | F5 weapons | `HasUsableWeapon()` on the door; `FAIBWeaponPolicy::Decide` extracted; targetless swap at a default range; dry hand widens melee RANGE (not reaction) | `AIBot.Sim.WeaponPolicy` |
| 3 | F1 facing | facing-travel in `TickLocomotion`, suppressed by a YAW CLAIM taken by every aimer | — |
| 4 | F3 targets | `FAIBTargetCandidate` table + `FAIBTargetPolicy` scoring with two hysteresis knobs; damage can now name a target; SWITCHED log line | `AIBot.Sim.TargetPolicy` |
| 5 | BN32 rig | ticket now names the **four numbers F2 is blocked on**, three of them [thin] | — |
| 6 | F2 traversal | `FAIBTraversalPolicy` chooser (drop/jump/dash/grapple), wired to the stall path | `AIBot.Sim.TraversalPolicy` |

Plus `AIBot.Sim.AvatarBelt` from AIB21 M1 earlier the same day — four new suites in total.

## Three things the founder should know, stated plainly

**1. F2 is landed but deliberately timid.** Three of its four reach constants are
`[thin]` — `DashReachUU` in particular rests on *a comment* that says the dash "covers
~500uu". The chooser commits at 80% of every reach and refuses anything it is unsure of,
because a refused crossing costs a wander and a wrong one costs a life. **It will get
noticeably braver the moment BN32's rig is run**, and that is a data change, not a code
change. Until then, expect bots to cross the obvious gaps and decline the marginal ones.

**2. The grapple is not offered by the stall chooser.** It needs the aim-then-press
machine that only `MoveToPOI`'s traverse owns, and pressing it unaimed is the futile
press the module bans. The chooser still *answers* Grapple and logs it — that line is how
we learn the traverse should have been armed for a route it was not. Combined with AIB19's
new REFUSED/SHORT diagnosis, one PIE match now tells us everything about the grapple.

**3. Nothing here is proven.** Four suites and a lot of reasoning, but the honest rung is
"written". Two regressions were caught in this very session by reading against the
existing fairness pins — the juke window and the per-actor gain stamp — which is exactly
the class of thing a compile does not catch and a match does. The next real step is a
match, not more code.

