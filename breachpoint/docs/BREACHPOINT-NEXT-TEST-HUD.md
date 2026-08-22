# TEST — the HUD, proven in one session

**Cut:** 21 August 2026 by the cloud lead · covers Roadmap 7 (all waves). Needs a build with
R7 and the terminal's `TASK-R7-WBP-HUD` assets. Filter the log to `LogBN`.

## 0. Boot — the two lines that gate everything

Solo PIE: `BNUI: root layout up for …` then `BNUI: HUD up for …`. If instead a
`BNUI: … did not resolve` warning names a class — the asset is missing or misplaced; the ini
path in the warning is the contract. If NOTHING prints, the UI manager never ran (dedicated
instance, or the module didn't load).

## 1. The honest first frames

On the very first frames the HUD must show **dashes** everywhere (vitals dim at zero, ammo "—",
band "—:——") and then snap to live values as the feeds bind. A confident `0/100` or `0:00`
before data arrives is a FAILURE — that is the honest-unknown gate not gating.

## 2. The in-match surfaces

- **Vitals**: shield cyan; health yellow and **hidden while full** — appears on first damage,
  whole element (not an empty frame). Shield refills track the GE.
- **Ammo**: fire = mag counts down per shot (server-confirmed — no client prediction, a lagged
  count is lag made visible, not a bug); reload = transfer; swap = name and counts flip; knife =
  name + dashes, never 0/0.
- **Band**: my kills cyan, leader + limit dim, clock counts down on the whole-second flip.
  Warmup shows the banner; it clears at match start.
- **Killfeed**: every elimination, one line, ~6s linger, max 5; **my lines white, others dim**;
  suicide and world-death wordings match the kill log's exactly.
- **Reticle**: the static center dot. Per-weapon reticles are a named deferral, not a bug.

## 3. Death and back

Die: overlay with "ELIMINATED BY <name>" (threat red) + "RESPAWNING IN n" (amber) counting
3-2-1; the band and killfeed stay live behind it. Respawn: overlay gone, vitals full, health
hidden again. Die to your own grenade: "You eliminated yourself". Die to a world kill:
"Eliminated", no name.

## 4. The scoreboard

Hold Tab: rows sorted kills-desc, my row white, live updates while held. Release: gone. Match
end: pins itself with "<name> WINS" (winner's name cyan in the rows) or "DRAW"; a player dead
at the buzzer sees the scoreboard, NOT a stuck death screen. Restart: it unpins, scores zero,
killfeed empty.

## 4b. The cause of death (R7.3)

Kill a bot with the rifle: the feed line carries a weapon glyph (only if `DT_BNWeapons.Icon` is
filled for that row — a missing glyph with everything else alive is the column, not the chain).
Die to a bot and read the death screen: the killer's name, and the weapon under it. Kill with a
**melee**: the screen must read `Melee`, never the weapon in your hands. A grenade kill reads
`Grenade` with no glyph until a `Grenade` row exists in the table.

The server's own line is the arbiter: `BNGameMode: X eliminated Y with 'Rifle'`. If that line is
right and the screen is wrong, the fault is in the UI half; if the LINE says `None`, the capture
never happened — check that the damage went through `BNDamage`, which is the only door that
stamps a cause.

**The stowed slot:** the tray's lower line shows the NEXT weapon in the swap cycle, and swapping
rotates both readings together — never the gun already in your hands. Swap until the NEXT slot is
the unarmed one: the line must read "Unarmed" (an empty slot is a real state, not a blank), and
the press after it must actually empty your hands. A one-weapon loadout shows nothing at all.

## 5. The pause menu (R7.2) — **Standalone only**

**Escape is the editor's Stop-PIE shortcut**, so this section cannot be run in a PIE window: the
session dies before the mapping is read, and it looks like a broken menu. Launch Standalone.

Escape: the menu is up over the HUD, focus on RESUME, the "the match does not pause" warning
visible. Escape again, or click RESUME: gone, and shooting works immediately. LEAVE MATCH: one
`LeaveMatch has nowhere to go` warning, no travel (the ini path ships unset).

**The layer edge, worth one deliberate try:** open the menu and let a bot kill you. The pause
menu VANISHES and the death screen takes `Layer.GameMenu`; Escape while dead does nothing (one
Verbose line); on respawn the death screen clears and **no menu returns**. A pause menu on a
just-respawned player is the exact bug `UpdateGameMenuLayer` exists to prevent.

## 6. Three views (the rung that counts) + join-in-progress

Two-window PIE: every claim above on host AND client, and the client's kill lines/scores match
the host's. **Join a running match** (late-join PIE or a third window): correct remaining time
on the first rendered frame, current scores, NO burst of stale kill lines (the join-age filter),
and if joining during post-match — the winner banner, not a tie, even before the winner's
PlayerState maps.

## 7. Known and accepted (critic-recorded)

- The leader readout can lag one kill behind on bunch ordering; it self-heals on the next kill.
- A brief "Unarmed" during the respawn replication gap before the weapon bunch lands.
- Holding Tab visually replaces the whole HUD (same layer) — by design at this screen size.
- **The roster has no join/leave hook.** It rebuilds on a kill, on a match-state change, and on
  every scoreboard OPEN — so a player who joined or left between those moments appears (or
  clears) the instant you press Tab, not before. A row that is stale *while the board is already
  held* is the accepted edge; a stale row on opening it is a bug.
- **Tab in a map with no BN GameState** (a test level) shows an empty scoreboard shell — the
  panel with every row collapsed. Harmless, and it means the UI stack itself is alive.
- **The pause menu is closed by the screen's own key handler, not by CommonUI's back action.**
  BN ships no `UCommonUIInputData`, so `bIsBackHandler` binds nothing; the flag is a hook for the
  day that asset lands. Gamepad OPEN needs the `Gamepad_Special_Right` mapping row from the WBP
  ticket — without it a pad can close the menu but never open it.
- **A death the door could not name shows the killer alone.** Damage applied outside `BNDamage`
  (there is none today) captures `None`, and both the glyph and the weapon line stay absent —
  the honest-unknown rule, applied to the cause.
- **The killfeed glyph is art-gated.** No `Icon` on the row, no glyph; the line still reads —
  the death screen still names the weapon in words, which is where R7.3 put the readable half.
