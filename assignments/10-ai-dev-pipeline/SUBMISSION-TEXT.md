# Paste-ready text

Three blocks. Copy the one you need; nothing here needs editing except the link.

---

## BLOCK A — the itch.io page description (what a stranger reads)

> **BREACHPOINT — a 4v4-scale arena FPS**
>
> Shields that break and recharge, a grappleshot, a dash, and bots that fill every empty
> seat. Built in Unreal Engine 5.8 in native C++.
>
> **How to play — about 2 minutes**
>
> 1. **Download** the zip and **extract it somewhere** (Right-click → Extract All). You must
>    extract it — the game will not run from inside the zip.
> 2. Open the `Game` folder and run **`Breachpoint.exe`**.
> 3. Windows will show a blue **"Windows protected your PC"** box, because the build is not
>    code-signed. Click **More info → Run anyway**.
> 4. Windows Firewall may ask about network access. **Click Cancel** — this is single player
>    against bots and nothing needs the network.
> 5. On the menu: **PLAY** → the setup screen has five rows — **MAP** (Spillway or Arena 01),
>    **MODE** (Free-For-All or Team Deathmatch), **PLAYERS** (4/8/12/16), **SCORE LIMIT**
>    (10/20/30) and **TIME LIMIT** (5/10/15/20 min). Touch nothing and you get 10 kills on a
>    10-minute clock. → **START GAME**.
>
> **Controls** — WASD move · mouse look · **Left Mouse** fire · **Right Mouse** aim ·
> **Space** jump · **Shift** sprint · **R** reload · **F** melee · **G** grenade ·
> **1** grapple · **2** dash · **Tab** scoreboard · **Esc** menu.
>
> The two that are not standard: **1 = grapple** (hook what you are looking at and pull to
> it) and **2 = dash** (short burst in the direction you are moving). Shields recharge if
> you break contact, so ducking a corner and coming back is a real move.
>
> First to the score limit, or the highest score when the clock runs out — **10 kills on a
> 10-minute clock** unless you change those rows.
>
> **What this is, stated plainly:** a packaged **Windows client, single player against
> bots.** Unreal Engine 5 has no browser target — HTML5 was removed in UE 4.24 — so a native
> download is the only honest form this can take. Keyboard and mouse are proven; a gamepad
> layout ships and is audited but has never been tested with a controller in hand.

---

## BLOCK B — the assignment submission comment (what the grader reads)

> **BREACHPOINT** — 4v4-scale arena FPS, Unreal Engine 5.8, native C++ (zero Blueprint
> gameplay classes).
>
> **Playable link:** <PASTE THE ITCH.IO URL HERE>
>
> **To play (~2 min):** download → **extract the zip** → run `Game\Breachpoint.exe` →
> SmartScreen **More info → Run anyway** (unsigned build) → Firewall prompt **Cancel**
> (single player vs bots, nothing needs the network) → **PLAY** → set MAP / MODE / PLAYERS /
> SCORE LIMIT / TIME LIMIT → **START GAME** (defaults are 10 kills, 10 minutes).
> WASD + mouse; **1** grapple, **2** dash, **Tab** scoreboard.
>
> **What the pipeline produced, and where to see it:** the **bot callsigns** — Dulledge,
> Softaim, Slowdraw, Evenkeel, Wideshot, Shakygrip, Midpace. **Press Tab in a match** and
> they are the names on the scoreboard. Each was derived from a real number in the game's own
> tuning table, not invented: `Slowdraw` because `reaction_ms=500` is the slowest profile,
> `Deadeye` because `accuracy_pct=0.65` is the highest.
>
> **To verify the pipeline actually produced them — three commands, no API key, ~20 seconds:**
> ```
> cd assignments/04-content-pipeline && python3 run_pipeline.py
> cd ../10-ai-dev-pipeline && python3 land_in_engine.py --check
> bash verify.sh
> ```
> The pipeline **replays its recorded API responses**, so that first command runs the real
> generate → judge → refute → land sequence on your machine for free. The second re-proves
> every bot name in the shipped build came from the pipeline (exits non-zero if one didn't).
> The third checks all nine gradeable criteria and re-derives the cost from the recordings.
>
> **Cost:** $4.98 total across five pipeline assignments; the priciest step is #4's divergent
> curation at $3.62 (73%). Prompt caching cut that step $6.39 → $1.43, a 78% reduction. Full
> breakdown in `assignments/10-ai-dev-pipeline/AUDIT.md`.
>
> **One thing the audit surfaces rather than hides:** the same pipeline run also produced
> twelve announcer lines, and I deliberately **did not** land them. The live module has no
> announcer system and the four trigger IDs they are keyed to appear nowhere in the source.
> Landing them would have added twelve rows nothing reads to claim a bigger integration.
> `land_in_engine.py` refuses to touch them and says why in its own docstring.
>
> **Honest scope:** Windows client, single player vs bots. Not a browser build, not a
> multiplayer claim. Keyboard and mouse proven; gamepad supported but untested.

---

## BLOCK C — if the file is too large for the upload

itch.io's browser uploader gets unreliable near 1 GB. If it stalls, use **butler**
(itch's CLI, https://itch.io/docs/butler/):

```
butler push BREACHPOINT-Submission.zip <your-itch-user>/breachpoint:windows
```

Two safe ways to shrink the download if you need to:
- The **Shipping** build is ~190 MB smaller (`Export\Win64-Shipping`) — but its match path
  was never verified end to end, so the Development build is the safer thing to ship.
- Deleting `Game\Engine\Binaries\ThirdParty\Vulkan\` (the validation layers, ~45 MB) is
  safe — they are developer tooling and no player needs them.

**Whatever you upload, set the page to Public, not draft, and open the link in a private
window before you submit it.** A draft link 404s for the grader, which reads exactly like a
broken link and triggers the same 50% cap as no link at all.
