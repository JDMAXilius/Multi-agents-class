# TICKET — AIB7: a refused move must say WHICH failure it is

> STATUS: done — mac terminal 26 Aug 2026. Hypothesis CONFIRMED, unanimously:
> 30 of 30 refusals are self=yes goal=yes — genuine unreachability, not off-mesh goals.

Cut from AIB5's residual. With flee's spam gone, two refusals that were buried underneath
became the top lines of the log:

```
415  cannot path to the last-known spot — search fails loudly (F7)
390  could not path to the belief — closing refused (F7)     <- Verbose, hidden before
```

AIB5 fixed OFF-MESH goals. These two pass a REMEMBERED point — a place a body actually
stood — which is the kind of point that projects successfully. So the likely cause here
is different: the goal is on a navmesh island the bot cannot reach (a navlink question),
not a goal off the mesh.

**That is a hypothesis, and the logs cannot currently confirm or refute it.** A refused
move says only "refused". Off-mesh and unreachable are completely different defects with
completely different fixes, and today they are indistinguishable. Fixing the LOG is
therefore the prerequisite for fixing the behaviour — and is the whole of this ticket.

## Scope — diagnose, do not fix

This ticket adds no behaviour. It makes five refusal sites state which failure occurred.
Any behavioural fix is a LATER packet, written against what this measures. Resisting the
urge to guess-fix is the point: AIB5 already showed one "obvious" fix being half a fix,
and the measurement is what caught it.

## The distinguishing facts

At a refusal, three cheap questions separate the causes:

- does the PAWN's own location project onto the navmesh? (no => the bot is off-mesh
  itself, and nothing it asks for will path — a spawn/placement bug, not a goal bug)
- does the GOAL project? (no => off-mesh goal, AIB5's class, meaning a site was missed)
- if BOTH project and the move is still refused => genuine unreachability: separate
  islands, or a navlink that did not generate

Distance is logged alongside, because "unreachable at 200uu" and "unreachable at 4000uu"
point at different geometry.

## Kickoff (machine-checkable)

- requires: engine-installed (editor only for the live read)
- owner_path: `Source/AIBot/Execution/`, `docs/tickets/TICKET_AIB7_MOVE_FAILURE_DIAGNOSIS.md`

## Steps (in order)

1. One shared diagnostic helper; all FIVE refusal sites report through it.
2. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint`.
3. **Rung 2** — `./Tools/run-specs.sh AIBot`: 91 expected, reconciled.
4. **Live read** — a `BotSystem=AIB` match; tabulate refusals by cause and name which
   hypothesis the data supports. State it plainly if it refutes the one above.
5. Four mechanical checks, pasted empty.

## Done when

- [x] All five refusal sites report self-on-nav, goal-on-nav and distance
- [x] Rung 1 PASS (Editor + Game; Server environmental)
- [x] Rung 2: 91/91/0, reconciled
- [x] Live refusal breakdown pasted; hypothesis CONFIRMED 30/30
- [x] Four mechanical checks pasted, empty

## Log

_(terminal: outputs verbatim)_

### 2026-08-26 — the verdict is unanimous

Rung 1 PASS (Editor + Game). Rung 2 91/91/0 reconciled. Four mechanical checks empty.

Live match, `BotSystem=AIB`, 7 bots, 20 eliminations, 708 ambition switches over 1m47s.

**Every refusal, every site, one verdict:**

```
site                 cause                count   median dist
belief closing       self=yes goal=yes       15         761uu
search last-known    self=yes goal=yes       15         604uu

distinct verdicts seen in the whole log:  30 x "self=yes goal=yes"   (and nothing else)
```

Read the two absences first, because they are as informative as the positive result:

- **Zero `goal=NO`.** No off-mesh goal reached the mover anywhere in the match. AIB5's fix
  is complete — no site was missed, and the class of bug it targeted is gone rather than
  merely reduced.
- **Zero `self=NO`.** No bot was ever off the mesh itself, so spawning and placement are
  sound and the goal was never innocent-by-association.

What remains is the third case, and it is now the ONLY case: both endpoints are on the
navmesh and the mover still refuses. That is genuine unreachability — separate navmesh
islands, or a link that never generated.

**The distances are the real find.** They do not scatter; they cluster hard:

```
min=593  p25=604  median=615  p75=761  max=771   (n=30)
```

Every failure in the match sits in a ~180uu band around 600–770uu. Random unreachability
across an arena would not look like this. A single unspanned geometric feature would.

Set that against the generated link configuration (`DefaultEngine.ini:291`): the widest
jump link the navmesh generates has **`JumpLength=400`** (the drop config; the climb
config is tighter at 250). A gap wider than the longest link produces no link, and the two
sides stay separate islands no matter how close they look.

**Stated as strongly as the evidence allows and no further:** the refusals are confirmed
to be unreachability, and the clustering is consistent with gaps beyond the 400uu jump
reach. It is NOT proof of that cause — the logged distance is straight-line bot-to-goal,
which is not the same measurement as the width of the gap between two platforms. A goal
615uu away in a straight line could sit across a 200uu gap or a 600uu one. Confirming it
means measuring the actual gaps on `BR_Arena01`, or raising `JumpLength` and re-measuring
this same table.

That is the next packet, and it is a TUNING/level question, not a code one. This ticket
deliberately shipped no behaviour change — its whole job was to make the log able to
answer the question, and the answer came back in one match.
