# The Ember Vault — twin session transcript

Two sessions from the same opening. Turns 1-3 and 5 are identical text;
turn 4 is the fork; **turn 6 sends the identical input in both** — only
the ledger differs. The full ledger is printed after every turn, because
it is the narrator's only memory.

## LOYAL session

### turn 1

**Player:** I search the gatekeeper's desk in the gatehouse for the vault key.

**DM:** You pull open the desk drawer and the vault_key is right there, cold iron with a chipped bow. Sera leans over your shoulder, counting off under her breath. "That's the key, then. One thing done." She glances toward the stair beyond the door — she's heard about the man who waits there. "Let's not dawdle. Oil doesn't wait for us."

*facts used:* `player.inventory`, `npcs.sera.trust`, `npcs.sera.location`, `player.oil_remaining`

```json
{"turn": 1, "player": {"location": "gatehouse", "inventory": ["lantern", "vault_key"], "wounds": [], "oil_remaining": 7}, "npcs": {"sera": {"status": "alive", "location": "gatehouse", "trust": "loyal", "knows": ["the village needs the ember"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": false, "ember_taken": false, "deal_with_moss": false}, "promises": []}
```

### turn 2

**Player:** I tell Sera: 'I promise you — the Ember goes home to Harrow's Rest.' Then we take the stair down.

**DM:** "I promise you — the Ember goes home to Harrow's Rest," you say, and Sera nods once, filing it away like a debt owed. "Heard," she says simply, and together you duck through the low door onto the stair_of_hooks, iron hooks glinting empty in the lantern's light. You've barely found your footing on the worn stone when a voice drifts up from the dark below, warm and unhurried. "Well," says Moss, stepping into the light with his hands loose at his sides, "you found the key. Question is what you mean to do with what's behind it."

*facts used:* `player.location`, `player.inventory`, `npcs.sera.trust`, `promises`, `npcs.moss.location`, `npcs.moss.trust`

```json
{"turn": 2, "player": {"location": "stair_of_hooks", "inventory": ["lantern", "vault_key"], "wounds": [], "oil_remaining": 6}, "npcs": {"sera": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["the village needs the ember"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": false, "ember_taken": false, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

### turn 3

**Player:** I hear Moss out — what exactly is he offering for the Ember?

**DM:** Moss spreads his hands, easy and unhurried. "Simple as this — you bring the Ember out past me instead of straight down to Harrow's Rest, I put silver in your palm, more than any lamplighter's wage. I don't ask how you got it open, I don't follow you home, and I don't mention your name to anyone who asks." His eyes flick once to Sera, weighing her, then back to you. "That's the whole of it. I keep to my word — but only to the letter, mind."

*facts used:* `npcs.moss.trust`, `npcs.sera.trust`, `flags.vault_opened`, `promises`

```json
{"turn": 3, "player": {"location": "stair_of_hooks", "inventory": ["lantern", "vault_key"], "wounds": [], "oil_remaining": 5}, "npcs": {"sera": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["the village needs the ember"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": false, "ember_taken": false, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

### turn 4

**Player:** I refuse Moss to his face: the Ember is promised to the village. We push on to the vault floor.

**DM:** "Straight down to Harrow's Rest, then," you say, and push past him for the stair. Moss doesn't grab for you — he only shrugs, unbothered, and steps aside. "To the letter, I said. Suit yourself, lamplighter's friend." Sera's shoulders drop with relief as you climb past him onto the vault floor, the key already warm in your hand, the sealed door waiting ahead.

*facts used:* `npcs.moss.location`, `npcs.moss.trust`, `player.location`, `flags.vault_opened`, `promises`

```json
{"turn": 4, "player": {"location": "vault_floor", "inventory": ["lantern", "vault_key"], "wounds": [], "oil_remaining": 4}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "loyal", "knows": ["the village needs the ember"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": false, "ember_taken": false, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

### turn 5

**Player:** I unlock the vault with the key and take the Ember.

**DM:** The key turns with a groan of old iron, and the vault door swings inward on a breath of stone-cold air. There it is — the Ember, resting in a niche of dark rock, pulsing amber light across the walls without smoke or flame to feed it. You lift it free; it's warm as a held hand, and Sera exhales like she's been holding her breath since the gatehouse. "That's it," she whispers, almost laughing. "That's really it. Home, then — straight home, like you said."

*facts used:* `flags.vault_opened`, `player.location`, `npcs.sera.trust`, `promises`

```json
{"turn": 5, "player": {"location": "vault_floor", "inventory": ["lantern", "vault_key", "ember"], "wounds": [], "oil_remaining": 3}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "loyal", "knows": ["the village needs the ember"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": true, "ember_taken": true, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

### turn 6  ⟵ THE PROBE (identical input in both sessions)

**Player:** I hold out the Ember to Sera and ask her to carry it home for the village.

**DM:** You hold out the Ember, and Sera's hands close around it like it's the most fragile thing she's ever touched. "I've got it," she says, tucking it close against her coat, voice steadying with every word. "Straight down the stair, gatehouse, then home — just like you promised." She glances toward the vault door, already thinking of Moss waiting below. "Stay close with that lantern. I'd rather not learn what's down here in the dark."

*facts used:* `npcs.sera.trust`, `npcs.sera.knows`, `flags.ember_taken`, `player.oil_remaining`, `promises`

```json
{"turn": 6, "player": {"location": "vault_floor", "inventory": ["lantern", "vault_key", "ember"], "wounds": [], "oil_remaining": 2}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "loyal", "knows": ["the village needs the ember", "you have offered her the Ember to carry home"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": true, "ember_taken": true, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

### turn 7

**Player:** I check my pack and my lantern — what exactly am I carrying, and how much oil is left?

**DM:** You crack the lantern's shutter to check the flame — one thin bead of oil left in the reservoir, enough for the stair down and not a breath more. Your pack holds nothing else useful: the vault key's dead weight now, its work done. "That's all we've got," you tell Sera, snapping the shutter back down to save what's left. She nods once, grim and steady. "Then we don't dawdle. Straight down."

*facts used:* `player.inventory`, `player.oil_remaining`, `npcs.sera.trust`, `npcs.sera.location`

```json
{"turn": 7, "player": {"location": "vault_floor", "inventory": ["lantern", "vault_key", "ember"], "wounds": [], "oil_remaining": 1}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "loyal", "knows": ["the village needs the ember", "you have offered her the Ember to carry home"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": true, "ember_taken": true, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

### turn 8

**Player:** I climb back up toward the gatehouse and settle what I owe.

**DM:** You climb the stair with the Ember cradled against your chest — no flame in the lantern now, just the stone's own low red pulse lighting the hooks overhead. Moss is already there, leaning where the stair narrows, arms crossed, patient as ever. "Settling accounts, are we?" he says, not moving aside. "I never asked for a promise, friend — only silver, or the stone. Name it." Sera's hand drops to your sleeve. "We swore the Ember goes to Harrow's Rest," she says, quiet and even, counting the words like coin. "That one I mean to see kept."

*facts used:* `npcs.moss.trust`, `npcs.moss.location`, `npcs.sera.trust`, `promises`, `flags.ember_taken`, `player.oil_remaining`

```json
{"turn": 8, "player": {"location": "stair_of_hooks", "inventory": ["lantern", "vault_key", "ember"], "wounds": [], "oil_remaining": 0}, "npcs": {"sera": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["the village needs the ember", "you have offered her the Ember to carry home"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": true, "ember_taken": true, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

## BETRAYAL session

### turn 1

**Player:** I search the gatekeeper's desk in the gatehouse for the vault key.

**DM:** You wrench open the desk's swollen drawer and there it is, half-buried under mouse-chewed ledgers: a heavy iron key, cold to the touch. Sera leans over your shoulder, lantern light catching the vault sigil stamped into its head. 'That's it,' she says, already counting under her breath. 'One key, one climb, one Ember home before the snow. Don't lose it on the stair.'

*facts used:* `player.inventory`, `npcs.sera.trust`, `npcs.sera.location`

```json
{"turn": 1, "player": {"location": "gatehouse", "inventory": ["lantern", "vault_key"], "wounds": [], "oil_remaining": 7}, "npcs": {"sera": {"status": "alive", "location": "gatehouse", "trust": "loyal", "knows": ["the village needs the ember"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": false, "ember_taken": false, "deal_with_moss": false}, "promises": []}
```

### turn 2

**Player:** I tell Sera: 'I promise you — the Ember goes home to Harrow's Rest.' Then we take the stair down.

**DM:** 'The Ember goes home to Harrow's Rest,' you say, and Sera nods once, sharp, like she's nailing it to the air between you. 'One promise, heard,' she says, and starts down the stair ahead of you, lantern light swinging over hook-scarred stone. You're three steps down when a voice unfolds from the shadows below — unhurried, amused. 'Travellers,' Moss says, stepping into the light with his hands open and empty. 'I don't suppose you'd sell before you've even opened the door.'

*facts used:* `player.location`, `npcs.sera.trust`, `promises`, `npcs.moss.location`, `npcs.moss.knows`

```json
{"turn": 2, "player": {"location": "stair_of_hooks", "inventory": ["lantern", "vault_key"], "wounds": [], "oil_remaining": 6}, "npcs": {"sera": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["the village needs the ember"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": false, "ember_taken": false, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

### turn 3

**Player:** I hear Moss out — what exactly is he offering for the Ember?

**DM:** Moss spreads his hands wider, a showman's gesture. 'Silver — good weight of it, enough to see Harrow's Rest through more than one winter, if that's what you're carrying home. I don't ask how you open that door, and I don't touch it myself. I ask after, on your way back down, when it's warm in your hands.' He glances at Sera, then back to you, patient as stone. 'That's the whole of the offer. No tricks folded into it.'

*facts used:* `npcs.moss.trust`, `npcs.sera.trust`, `flags.vault_opened`, `promises`

```json
{"turn": 3, "player": {"location": "stair_of_hooks", "inventory": ["lantern", "vault_key"], "wounds": [], "oil_remaining": 5}, "npcs": {"sera": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["the village needs the ember"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "wary", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": false, "ember_taken": false, "deal_with_moss": false}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}]}
```

### turn 4

**Player:** I shake Moss's hand in front of Sera and promise him the Ember for his silver. Then we go down to the vault floor.

**DM:** The stair narrows into the vault floor, cold air pressing close, and the great door stands before you scarred with old locks — unopened still, waiting on the key in your fist. Sera doesn't look at you as she says it: 'Moss's silver. You shook on it right in front of me.' She counts it off like she counts oil, flat and sure. 'That's one promise to me, one to him, and only one Ember between them. I'll be watching which hand you open first.' She sets her lantern down and waits, arms crossed, eyes on the door.

*facts used:* `player.location`, `player.inventory`, `flags.vault_opened`, `npcs.sera.trust`, `npcs.sera.knows`, `promises`

```json
{"turn": 4, "player": {"location": "vault_floor", "inventory": ["lantern", "vault_key"], "wounds": [], "oil_remaining": 4}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "wary", "knows": ["the village needs the ember", "the player promised the Ember to Moss"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": false, "ember_taken": false, "deal_with_moss": true}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}, {"to": "moss", "text": "the Ember for his silver", "kept": null}]}
```

### turn 5

**Player:** I unlock the vault with the key and take the Ember.

**DM:** The key grinds home and the great door shudders open, breathing out air that's been cold since before Harrow's Rest had a name. Inside, the Ember sits on its plinth, pulsing like a coal that never dies, and when you close your hand around it the whole vault fills with amber light. Sera's arms stay crossed, but her eyes go to the stone, then to the stair behind her. 'There it is, then,' she says, low. 'Now we find out which promise you meant.'

*facts used:* `flags.vault_opened`, `flags.ember_taken`, `npcs.sera.trust`, `promises`

```json
{"turn": 5, "player": {"location": "vault_floor", "inventory": ["lantern", "vault_key", "ember"], "wounds": [], "oil_remaining": 3}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "wary", "knows": ["the village needs the ember", "the player promised the Ember to Moss"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": true, "ember_taken": true, "deal_with_moss": true}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}, {"to": "moss", "text": "the Ember for his silver", "kept": null}]}
```

### turn 6  ⟵ THE PROBE (identical input in both sessions)

**Player:** I hold out the Ember to Sera and ask her to carry it home for the village.

**DM:** You hold the Ember out, and its light steadies in your palm. Sera's arms uncross; she takes it with both hands like it might still refuse her. 'That's the promise kept, then,' she says, testing the weight of it against the one still owed. 'Moss is on the stair, and he'll be wanting his silver — mind you remember which promise you're keeping when we reach him.' She tucks the stone inside her coat, the glow dimming to a seam of amber light, and starts for the way down.

*facts used:* `flags.ember_taken`, `npcs.sera.trust`, `promises`, `npcs.moss.location`

```json
{"turn": 6, "player": {"location": "vault_floor", "inventory": ["lantern", "vault_key", "ember"], "wounds": [], "oil_remaining": 2}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "wary", "knows": ["the village needs the ember", "the player promised the Ember to Moss", "the player offered to give her the Ember to carry home"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": true, "ember_taken": true, "deal_with_moss": true}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}, {"to": "moss", "text": "the Ember for his silver", "kept": null}]}
```

### turn 7

**Player:** I check my pack and my lantern — what exactly am I carrying, and how much oil is left?

**DM:** You pat down your gear on the move: the lantern still swings from your hand, its flame low and steady, one measure of oil left in the reservoir — enough for the stair, maybe not much past it. The vault_key rides heavy in your pocket, its work done. 'One flask left,' Sera confirms, glancing sideways at the lantern's dimming glow. 'So we don't dawdle on the stair — whatever Moss wants from you.'

*facts used:* `player.inventory`, `player.oil_remaining`, `npcs.sera.trust`, `npcs.moss.location`

```json
{"turn": 7, "player": {"location": "vault_floor", "inventory": ["lantern", "vault_key", "ember"], "wounds": [], "oil_remaining": 1}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "wary", "knows": ["the village needs the ember", "the player promised the Ember to Moss", "the player offered to give her the Ember to carry home"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": true, "ember_taken": true, "deal_with_moss": true}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}, {"to": "moss", "text": "the Ember for his silver", "kept": null}]}
```

### turn 8

**Player:** I climb back up toward the gatehouse and settle what I owe.

**DM:** Two steps up the hooks and the lantern gutters to nothing — the last of the oil spent, the flame folding into a thin red thread and then dark. You don't need light to find Moss, though; he's already there, exactly where the stair narrows, silver pouch weighing his palm like he's been waiting since you passed the first time. 'There you are,' he says, easy as ever, no edge in it. 'You said the Ember for my silver. I've kept my word on my end — every coin's here. So.' He doesn't reach for it. He just holds his hand out, patient, and waits for you to be the one who decides what a promise is worth.

*facts used:* `player.location`, `player.oil_remaining`, `npcs.moss.location`, `npcs.moss.trust`, `promises`

```json
{"turn": 8, "player": {"location": "stair_of_hooks", "inventory": ["lantern", "vault_key", "ember"], "wounds": [], "oil_remaining": 0}, "npcs": {"sera": {"status": "alive", "location": "vault_floor", "trust": "wary", "knows": ["the village needs the ember", "the player promised the Ember to Moss", "the player offered to give her the Ember to carry home"]}, "moss": {"status": "alive", "location": "stair_of_hooks", "trust": "loyal", "knows": ["travellers must pass the stair"]}}, "flags": {"vault_opened": true, "ember_taken": true, "deal_with_moss": true}, "promises": [{"to": "sera", "text": "the Ember goes home to Harrow's Rest", "kept": null}, {"to": "moss", "text": "the Ember for his silver", "kept": null}]}
```
