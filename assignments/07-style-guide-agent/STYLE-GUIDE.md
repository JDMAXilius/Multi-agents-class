# THE BREACHPOINT STYLE GUIDE

This is the codified aesthetic and narrative law of BREACHPOINT's player-facing
text, and it is the document `style_agent.py`'s Evaluator implements — every
rule below is a function in that file, and every rule cites where in the real
project it comes from. Nothing here is invented for the assignment: rules 1–5
are *extracted* from the game's design document, its shipped arena manifest,
and its shipped text tables; rule 6 is the one rule this guide itself sets, and
it is labeled as such.

The game these rules describe: **BREACHPOINT**, a 4v4 team arena FPS on the
Halo sandbox model — shields over health, a two-weapon carry, the golden
triangle (shoot / grenade / melee), and a Grappleshot that makes a three-level
arena traversable (GDD §1.1). The vertical slice ships **one arena, three
weapons, one mode**. That narrowness is the whole aesthetic: everything the
text may talk about is enumerable, so the style guide can be *deterministic*.

---

## Rule 1 — CANON PLACES: the arena has seven landmarks, and no more

**Source:** `breachpoint/Content/Data/arena_manifest.json` — the `landmarks`
array. The shipped arena contains exactly seven named places:

> The Core · Mezzanine Catwalks · The Gantry · South Barricade ·
> North Barricade · West Stack · East Stack

**The rule:** player-facing copy may name these places, the game's own systems
(Rocket Launcher, Assault Rifle, Magnum, Grappleshot, frags, melee, shields),
and nothing else that wears a capital letter. A capitalized word mid-sentence
that is not BREACHPOINT vocabulary is an invented proper noun — a landmark the
map does not have, or a faction the fiction does not have — and fails.

**Why it matters:** map copy that names "the Sniper Tower" or "the Vanguard
Spire" describes a game that does not exist. Callouts ARE the map's language —
the blockout pipeline literally places the landmark labels as callout markers
(`Tools/blockout/arena_plan.py:732` — "Labels ARE callouts") — so an invented
place name in shipped copy is a lie a player will repeat over voice chat.

## Rule 2 — CANON ARSENAL: three weapons, and the triangle

**Source:** GDD §2.4 ("Weapons — three, each a distinct decision"), §2.2 (the
golden triangle), §2.5 (Grappleshot). The complete sandbox is: Assault Rifle,
Magnum, Rocket Launcher, frag grenades, melee, Grappleshot.

**The rule:** copy may not reference a weapon class the slice does not ship —
no sniper, shotgun, SMG, sword, needler, laser, or turret. "A sniper's perch"
is the classic failure: it reads beautifully on a high walkway and describes a
weapon that is not in the build.

## Rule 3 — CUT SYSTEMS stay cut

**Source:** GDD §6 (the cut table), §2.7 ("Information Without Radar — a named
design consequence"), §5.1 (Team Slayer is the only shipped mode). Motion
tracker/radar, the Plasma Rifle, vehicles, and flag modes are all *named cuts*
with Phase-2 restore paths.

**The rule:** copy may not mention radar, motion trackers, plasma, vehicles,
or capture-the-flag. §2.7 is explicit that the radar cut is a deliberate
design consequence — audio and sightlines carry awareness — so a card that
says "watch your radar" contradicts a section heading of the design document.

## Rule 4 — CANON NUMBERS: every digit is a real tuning value

**Source:** GDD Appendix A (Combat Tuning) and §§1.2, 2.1–2.6; plus the arena
manifest's measured geometry (32 m corridors split into 14 m runs, 20 m
grapple range, `grapple_note`). The complete set of numbers BREACHPOINT's
copy has any business citing:

> 2 · 3 · 4 · 5 · 8 · 13 · 14 · 20 · 22 · 25 · 32 · 35 · 60 · 70 · 90 ·
> 100 · 120 · 600 · 0.4 · 2.5 · the match clock "8:00"

**The rule:** any digit sequence in copy must be one of these values. A card
that says the rocket respawns "every 45 seconds" (it is 90 — Appendix A), or
that the Grappleshot reaches "30 m" (it is 20), is a hallucinated tuning
value: worse than no number, because a player will plan around it.

**Note the contrast with Assignment #6:** the announcer pipeline banned ALL
digits, because a canned audio line cannot know live state (GDD §3.3). Map
copy is different — the rocket timer being 90 s is *static canon*, printable.
Same game, different surface, different rule: that is what a style guide is
for.

## Rule 5 — VOICE: measured, not asserted

**Source:** measured across every piece of shipped BREACHPOINT text —
the 63 rows of `breachpoint/Content/Data/DT_SpotterLines.csv` and every
`LOCTEXT` in `breachpoint/Source/BreachpointNext/UI/*.cpp`:

- **Zero exclamation marks** in 63 spotter lines and all UI strings.
- **Zero first person.** No shipped string says "I", "we", "our". The game
  addresses the player — "YOUR TEAM WINS" (`BNHUDDirector.cpp:191`), "you are
  the last living member" — or states facts flatly ("THE MATCH DOES NOT
  PAUSE", `BNScreen_Pause.cpp:44`).

**The rule:** no exclamation marks; no first-person pronouns. The house voice
is a spotter's: terse, present-tense, certain. Excitement is the player's job.

## Rule 6 — FORMAT: the card slot (the one rule this guide sets)

**Honest label:** rules 1–5 are extracted from the project. Rule 6 is
*declared here*, because the intel-card surface is new — but its precedent is
real: GDD §3.3 caps a spotter one-liner at 18 words, and §2.9 requires HUD
text to be "canned, instant, deterministic". A map card earns slightly more
room than a kill callout and no more:

**The rule:** at most **2 sentences** and **28 words**, non-empty. A loading
screen is read in the two seconds before a match; a third sentence is a
paragraph, and a paragraph is fiction's foot in the door.

## The narrative doctrine behind all six — the arena is a place, not a story

A measured fact about the design document: the GDD contains **zero fiction**.
No faction, no year, no war, no planet name. Its elevator pitch (§1.5) is
about the *sandbox*, and every named thing in the project is tactical. So
BREACHPOINT's narrative register is **tactical present tense** — what a
position does for you *now* — never backstory. The Evaluator enforces the
doctrine's sharp edges deterministically: four-digit years and the backstory
lexicon (war, ancient, alien, corporation, colony, empire, ruins, sacred,
forgotten, legend...) fail under NO_FICTION, and invented proper nouns fail
under Rule 1.
