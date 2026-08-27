# Assignment #8 — Narrative Engine Prototype

**Course:** Multi-Agent AI for Game Development · **Student:** Juan Diego Lugo
**Project type:** standalone, per the assignment. (My capstone, BREACHPOINT, bans narrative
by doctrine — its own style guide rules "no lore, no fiction, no characters" — so the
narrative engine gets a world of its own.)

A virtual Dungeon Master with a JSON facts ledger, built in Python on the Claude API.

```bash
./verify.sh               # ← START HERE: checks every rubric criterion, exit 0 = all pass
python3 dm.py             # replay BOTH committed sessions — stdlib only, no API key
python3 dm.py --live      # re-run the scripted sessions for real
python3 dm.py --live --play   # free-form interactive session (you type the actions)
```

---

## The world

**The Ember Vault.** A sealed mountain vault holds the Ember — a stone that burns without
fuel — and the village of Harrow's Rest will not survive winter without it. You climb with
**Sera**, the village lamplighter who trusts you and keeps count of promises out loud. On the
Stair of Hooks waits **Moss**, a scavenger who offers silver for the Ember and keeps his word
to the letter and no further. Three rooms, two NPCs, one lantern burning down, one moral
fork. Small on purpose: every element exists to exercise state.

## What the ledger tracks

The full JSON ledger is printed after **every** turn — in the session output and in
[`output/transcript.md`](output/transcript.md):

| Path | Tracks |
|---|---|
| `player.location` · `inventory` · `wounds` | where you are, what you hold, what it cost |
| `player.oil_remaining` | the lantern — **engine-owned**, burns down 1/turn, models may not touch it |
| `npcs.<name>.status / location / trust / knows` | alive→wounded→dead (terminal) · a trust state machine · **what each NPC has learned** |
| `flags.vault_opened / ember_taken / deal_with_moss` | sticky story facts — once true, never untrue |
| `promises[]` | who was promised what — the moral bookkeeping the dialogue runs on |

**Updates come from what the player *does*, not what they say they are.** An extractor model
turns each action into delta ops; a **deterministic guard layer** applies or bounces them
with the rule named: no movement off the map graph, no opening the vault without the key in
hand and feet on the vault floor, no taking the Ember before the vault opens, dead is dead,
broken trust doesn't mend, and the engine owns the resources. Illegal deltas bounce back to
the model with the exact violation, once; `verify.sh` proves all six illegal-transition
classes are blocked.

## The design decision everything hangs on

**The ledger is the only memory.** The narrator is never shown the transcript — it gets the
world rules, the current ledger, the action, and one previous beat for prose continuity.
Consistency across eight turns is therefore not a property we hope the model has; it is a
property the architecture forces, because there is nothing else to remember with. Every
response must also declare `facts_used` — the ledger paths that shaped it — and every one of
the **73 citations** across both sessions resolves against its turn's ledger.

## Proving reactivity: the twin sessions

The hardest rubric line to *prove* is that dialogue reacts to state, "not just the most
recent input." One session can't show that. So the committed artifact is a **twin**: a LOYAL
and a BETRAYAL session sharing turns 1–3 and 5, forking at turn 4, and sending the
**character-for-character identical input at turn 6**:

> *"I hold out the Ember to Sera and ask her to carry it home for the village."*

Same input, same prompt, same world — different ledger. In the loyal run Sera takes the stone
*"like it's the most fragile thing she's ever touched… straight down the stair, gatehouse,
then home — just like you promised."* In the betrayal run she takes it *"testing the weight of
it against the one still owed"* and warns: *"Moss is on the stair, and he'll be wanting his
silver — **mind you remember which promise you're keeping** when we reach him."* Her dialogue
cites `npcs.sera.trust`, both `promises`, and `npcs.moss.location`. Recency cannot explain the
difference; only the ledger can. [`output/probe_comparison.json`](output/probe_comparison.json)
holds the side-by-side, and `verify.sh` asserts it.

## The moment the agent surprised me

**It out-judged my own verification.** The betrayal fork has the player shake Moss's hand *in
front of Sera* and promise him the Ember. My verify script asserted Sera's trust would read
`betrayed` — I wrote the world, I "knew" the answer. The extractor instead ruled her **`wary`**
— and then did three things I hadn't scripted anywhere: it added *"the player promised the
Ember to Moss"* to Sera's `knows` list, flipped **Moss** to `loyal` (he got his deal — of
course he's satisfied), and recorded **both conflicting promises** side by side in the ledger.
Its reading is defensible — the deal was made openly, and the player later still hands Sera
the stone — and the state it built is *richer* than the one I had planned to check for. My
check failed against a model that was more right than the check. I changed the verification to
assert the fork's **consequences** (the deal flag persists, Sera's trust is no longer `loyal`,
she *knows*, both promises survive to turn 8) rather than my pre-written label; the comment in
`verify.sh` records the incident.

Runner-up, from an earlier test run: given only the post-action ledger, the narrator once
found the vault key already in inventory and **invented a past for it** — *"your hand already
closes on the vault_key at your belt; you took it before you climbed."* State-only narration
guarantees consistency of facts, not of their history. Worth knowing about the architecture.

## Consistency, audited rather than claimed

`verify.sh` replays both sessions from the recordings (all deterministic layers execute for
real) and checks: 8 turns per session (rubric floor is 5) · oil strictly decreasing · no
sticky flag ever forgotten · the turn-2 promise alive at turn 8 in both branches · the fork's
consequences persisting in the betrayal branch and **never leaking into the loyal one** · no
dead NPC speaking anywhere · the turn-7 memory probe naming no item the ledger denies · all
73 fact citations resolving. Exit 0 = every rubric criterion holds.

## What went wrong while building it, honestly

Every failure across four live runs was **my validation layer being narrower than correct
model output** — never the state machine. The narrator cited `promises[0].text` (a correct
list-index path my resolver couldn't walk), then `player.location: vault_floor` (a path
helpfully annotated with its value). Each killed a run before I made the validator a tolerant
reader: annotations are stripped, the path is kept, the value half is discarded and never
trusted. The guards, meanwhile, blocked every illegal transition in every run and produced
zero false positives. Strict about state, tolerant about format — that asymmetry is the
lesson this build taught.

## Files

```
dm.py                the whole engine: world · ledger · guards · extractor · narrator
verify.sh            checks every rubric criterion against a fresh replay  ← run this first
recording_loyal.json / recording_betrayal.json   the committed live sessions
make_submission.sh   builds the zip; refuses to ship if verify.sh fails from the staged copy
output/
  transcript.md          both sessions, full ledger after every turn
  session_*.json         per-turn actions, deltas, narration, citations, ledger history
  probe_comparison.json  the identical-input probe, side by side
README.md            this file
```
