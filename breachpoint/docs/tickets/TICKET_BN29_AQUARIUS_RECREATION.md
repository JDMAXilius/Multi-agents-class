# TICKET — BN29: Aquarius, one to one

> STATUS: OPEN — 29 Aug 2026, founder directive (verbatim intent): "learn from
> this very deeply. We're gonna make and recreate this level one to one." —
> Ketul Majmudar's Aquarius level-design analysis
> (uxdesign.cc/halo-infinite-multiplayer-level-design-series-aquarius-e90be2907496).
> This supersedes Arena 02 "THE LOCKS" (founder: "that level is completely
> wrong... forget that level"). The Locks' manifest stays in the repo as a dead
> branch of the ledger; nothing builds it.

## The goal

Recreate Halo Infinite's AQUARIUS as BREACHPOINT's arena, one to one at
blockout fidelity: every named space, both floors, every connection and path
split, every pad position — the geometry the analysis and the callout record
establish, as a manifest our proven pipeline builds. A study recreation for
the capstone (learning exercise, not shipped commercial content).

## What "one to one" can mean per data source (honesty ladder for geometry)

1. TOPOLOGY — established, complete (both research passes): base -> courtyard
   -> Bottom Mid -> courtyard; Top Mid bridge above with camo/OS; Hydro hall
   linking the Utility corners (shock rifle mid-hall); Planters-under-Pump
   linking the Refrigeration corners (shotgun-tier); stairs in each base
   toward Top Mid; two exits per base; exact mirror symmetry.
2. THE ARTICLE'S DESIGN REASONING — being mined now (all direct fetches are
   egress-blocked; a quote-mining agent is reconstructing it from search
   snippets; digest lands in this Log).
3. PROPORTIONS — nobody publishes Aquarius' dimensions. The precision ladder,
   best first:
   a. FOUNDER CHANNEL: screenshots of the article's diagrams and/or the HCS
      board into docs/design/reference/ (committed images are visible to the
      cloud) — traceable proportions.
   b. TERMINAL CHANNEL: walk Aquarius in Halo Infinite (custom game/Forge) and
      measure spaces with Forge's own units — real numbers, the true 1:1.
   c. DERIVED: until a/b, proportions derive from movement metrics (343's
      8/12/80 jump/clamber/grapple ratios, BR TTK ranges) with every number
      labeled DERIVED in the manifest. The build starts on (c) and upgrades.

## Build plan (the proven pipeline, unchanged)

- `Tools/blockout/gen_aquarius_manifest.py` -> `Content/Data/aquarius_manifest.json`
  (mirror as code, like Arena 02's generator; BREACHPOINT adaptations ONLY
  where the sandbox differs, each one logged: our power weapon set, our
  grapple in place of clamber where a ledge exceeds jump, spawn schema).
- `arena_plan.py --manifest` gates it (8 spawns/8 m/35 m/rocket node...).
  NOTE: Aquarius' own base-to-base line may exceed our 35 m GDD cap — if it
  does, that is a FOUNDER RULING to make (keep 1:1 and amend the GDD cap for
  this map, or clip the lane); flagged, not decided here.
- `gen_blueprint.py` -> REV B sheets; founder iterates; Figma stays parked
  until liked; terminal builds from the manifest.

## Done when

- [ ] Article digest in the Log (mining agent)
- [ ] Manifest emitted + validator verdict recorded (with the 35 m ruling if needed)
- [ ] Sheets reviewed by the founder
- [ ] Proportions upgraded to channel (a) or (b) evidence
- [ ] Terminal blockout + editor screenshots

## Log

### 29 Aug — the article, reconstructed (quote-mining agent; fetches all blocked)

Recovered as-surfaced (near-verbatim, attribution Ketul Majmudar, UX Collective,
Jan 5 2022 — second of a five-part series):

- LEVEL DESIGN GOAL: "an indoor small size symmetrical map focusing on mostly
  close range engagements for 4v4 or small team battles with no vehicle play
  included."
- SKELETON: "made of three major hallways and two open arenas, with one hallway
  going through the two arenas and the other two on either side of the arenas."
- PATH SPLITS, LEVELS: "Path splits leading to different levels give players
  moment to moment strategic choices which actually affect how they are going
  to engage the next enemy."
- PATH SPLITS, SAME LEVEL: "mainly strategically useful to players when they
  open into completely different spaces, allowing players to ensure it is not
  easy for enemies to chase them through an area and that they can move fights
  to desirable locations."
- VERTICALITY: classic Halo leaned on jumps/grav-lifts/jump-pads; Infinite's
  grapple and thruster "added a need for a lot more levels and platforms."
- SIGHT LINES: "The developers wanted to break up sight lines on the side
  hallways so that the player would keep moving rather than standing on one
  side of the hallway and shooting everything in sight."
- FIGURES: annotated top-downs sourced from thegamescabin.com (also egress-
  blocked; its walkthrough unfetchable).
- NOT RECOVERED (declared gaps): the article's weapons/spawn reasoning, the
  per-arena breakdown, intro/conclusion, figure captions, any numbers beyond
  the 3-hallway/2-arena counts.
- Sibling-article principles that cross-apply: refuge spaces small, low-
  ceilinged, ALWAYS >=2 exits (anti-camp, stays part of a loop); short sight
  lines force movement; landmarks triple-duty as wayfinding/callouts/cover.

DESIGN LAWS FOR THE RECREATION, extracted from the above: (1) three hallways +
two arenas is the skeleton, sacred; (2) every junction should offer a level
split where possible, and same-level splits must diverge into different
spaces; (3) side-hallway sightlines get broken deliberately; (4) the map is
indoor, small, close-range; (5) two exits minimum from every room.

### 29 Aug — manifest built at DERIVED proportions (channel c)

- Emitted by Tools/blockout/gen_aquarius_manifest.py -> Content/Data/
  aquarius_manifest.json. Every derived number carries its reasoning in the
  script; upgrade path to channels (a)/(b) unchanged.
- BREACHPOINT adaptations, each logged in the generator: the rocket takes
  Aquarius' power-up slot at Top Mid center (our slice's one power pickup);
  grapple anchors on Top Mid and Pump lips (our hook stands in for thruster
  mobility); our spawn schema (2 per base + 4 courtyard neutrals).
