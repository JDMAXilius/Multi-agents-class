# BREACHPOINT — How to Play

A 4v4-scale arena shooter. Pick a map, pick a mode, pick how many players, and fight bots.
Everything below is generated from the game's own input table (`Tools/bn/10_input_assets.py`,
which owns `IMC_BNNext`) and its match config — if a key changes there, this page is wrong
until it is regenerated.

---

## Getting into a match

1. The game opens on the **main menu**.
2. **PLAY** →
3. Set three things, each row cycles when you activate it:
   - **MAP** — which arena
   - **MODE** — Free-For-All or Team Deathmatch
   - **PLAYERS** — total bodies in the match, you included
4. **START GAME**.

You fill one slot; bots fill the rest. At 8 players that is you and 7 bots in Free-For-All,
or 4v4 in Team Deathmatch.

---

## Controls

| Action | Keyboard / Mouse | Gamepad |
|---|---|---|
| Move | **W A S D** | **Left stick** |
| Look | **Mouse** | **Right stick** |
| Jump | **Space** | **A** |
| Crouch | **Left Ctrl** | **R3** (right stick click) |
| Sprint | **Left Shift** | **L3** (left stick click) |
| Fire | **Left Mouse** | **R2** |
| Aim down sights | **Right Mouse** | **L2** |
| Reload | **R** | **X** |
| Melee | **F** | **B** |
| Grenade | **G** | **D-pad ↑** |
| Next weapon | **Scroll up** | **Y** |
| Previous weapon | **Scroll down** | **D-pad ↓** |
| Lean left | **Q** | **D-pad ←** |
| Lean right | **E** | **D-pad →** |
| **Grapple** | **1** | **L1** |
| **Dash** | **2** | **R1** |
| Scoreboard (hold) | **Tab** | **Share / Select** |
| Menu | **Esc** | **Options / Start** |

> Gamepad support is generated and audited but has **not been tested with a controller in
> hand** — see `docs/tickets/TICKET_BN46_GAMEPAD_VERIFY.md`. Keyboard and mouse are proven.

---

## The two things that are not standard FPS

**Grapple (1 / L1)** — fires a hook at what you are looking at and pulls you to it. It is
how you take the high ground quickly, cross a gap, or leave a fight you are losing. The
bots use it too, on the same cooldown and the same range as you.

**Dash (2 / R1)** — a short, fast burst in the direction you are moving. Use it to cross an
open lane, break someone's aim mid-fight, or get out of a grenade's radius.

Both are on cooldowns. Neither is a "get out of jail free" — a bot that sees you dash will
still be shooting where you are going.

---

## Winning

| | Free-For-All | Team Deathmatch |
|---|---|---|
| Sides | Everyone for themselves | Two teams, split evenly |
| You score by | Killing anyone | Killing the other team |
| First to | **7 kills** | **7 kills** (team total) |
| Or | highest score when the **10-minute** clock runs out | same |

Score and time limits are set in the lobby before you start; the numbers above are the
defaults. Death costs you a few seconds and you respawn — there are no lives to run out of.

---

## Survival, in one paragraph

You have **shields over health**. Shields take damage first and **recharge on their own**
after you have been out of trouble for a moment; health does not come back that easily. So
breaking contact is a real move — duck a corner, let the shield come back, and re-enter the
fight whole. That is the rhythm of every gunfight here, and it is why leaning, dashing and
grappling matter more than raw aim.

Weapons carry an **AR**, a **Magnum** and, when you find it, a **Rocket**. Each is better
at a different range, and the scoreboard is on **Tab** whenever you want to see where you
stand.
