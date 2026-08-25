# TICKET — Rebuild ST_BNBot and DT_BNBotTuning: four packets of bot behaviour are switched off

> STATUS: open — cut 24 Aug 2026 by the cloud lead, from the terminal's own finding in
> `docs/BREACHPOINT-NEXT-ROADMAP-10.md`. Needs a LIVE EDITOR and nothing else. This is the
> highest-value single action outstanding in the BN track.

Founder directive: the C++ for R9 and R10 is landed and **compiling** (rung 1 PASS, 24 Aug), and
none of it runs. `Content/BN/AI/ST_BNBot.uasset` is dated 22 Aug 18:38 — before R10 landed — so
the compiled tree has no `Cover` state, no `Evade` state and no `FBNStrafeTask`. Bots today
cannot take cover, cannot dodge a grenade and do not sidestep while firing, no matter what the
source says. Measured 24 Aug: 19 of 20 samples of a moving bot were FORWARD, which is exactly a
`Shoot` state with no strafe task in it.

**This is not a code change.** A StateTree's graph cannot be authored from Python or any MCP
toolset (`TASK-R5-ST-BNBOT` Log, 20 Aug); `UBNBotAuthoring` builds, compiles and saves it from
C++, and `Tools/bn/62_bot_assets.py` pulls that trigger over MCP.

**Ordering law:** `probe` gates `build`. A stale editor build authors a tree WITHOUT the new
nodes and saves it over the good one — the probe exists to stop exactly that, so a probe that
reports a missing node type is a STOP, not a warning to push past.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: editor-live
- Rung 1 PASS for `BreachpointEditor` on the current tree (it was, 24 Aug — re-run only if
  `Source/BreachpointNext/AI/` changed since)
- `Tools/bn/62_bot_assets.py` probes all **21** node types — every struct in
  `BNBotStateTreeTasks.h`, checked mechanically both directions. Five are the R10 additions
  this ticket exists for: `FBNStrafeTask`, `FBNShouldTakeCoverCondition`, `FBNTakeCoverTask`,
  `FBNIncomingBlastCondition`, `FBNEvadeBlastTask`.
  <!-- 25 Aug: the list held 14. The seven it was missing — Reacted, CanThrowGrenade,
       ThrowGrenade, InMeleeRange, Melee, HasLastKnown, SearchLastKnown — are all nodes the
       tree places, so a stale editor that had lost the grenade, the knife and the hunt would
       have passed the gate that exists to catch exactly that. -->
- owner_path: `Content/BN/AI/`, `Content/BN/Data/`
  <!-- ASSETS ONLY. This ticket writes NO C++. If something here cannot be done without a
       Source/ edit, that is a contract_gap: write it in the Log and STOP. The last time a
       Source/ edit was made from an editor ticket it was recorded as a deliberate violation
       (TASK-R7-WBP-HUD, 22 Aug) and it was the right call THAT time because nothing else was
       reachable — this time the C++ already compiles, so there is no such excuse. -->

## Steps (in order)

1. **Probe first.** `python Tools/bn/62_bot_assets.py probe` against the running editor. Paste
   the full output into the Log. **If any of the five new node types is missing, STOP** — the
   editor is running a stale build and the fix is to rebuild and relaunch it, not to author.
2. **Build.** `python Tools/bn/62_bot_assets.py build`. This authors `ST_BNBot`, compiles it,
   saves it, and also (re)builds `DT_BNBotAmbitions` and the new `DT_BNBotTuning`.
3. **Read back the tree from a FRESH load**, not from the builder's own report. Expect:
   `Root > [Evade, Engage > [Rearm, Arm, Nade, Knife, Cover, Close, Shoot], Search, Roam]`
   with `Shoot` carrying **three** tasks (`FaceTarget`, `Strafe`, `FireBurst`) and `compiled: YES`.
4. **Read back `DT_BNBotTuning`** — four rows, `Recruit / Marine / ODST / Spartan`, on
   `FBNBotTuningRow`. Marine must read sight **1200/1500/70**: those are the founder's arena
   numbers and every other tier is scaled around them.
5. **Verify the ini contract, no edits.** `[/Script/BreachpointNext.BNGameData]
   BotTuningTablePath` and `[/Script/BreachpointNext.BNBotController] BotTier=Marine` already
   name these; if an asset landed elsewhere, MOVE THE ASSET — the ini is the contract.
6. **PIE, solo, one match.** Paste the first `BNBots:` lines. Expect one
   `fights at tier Marine (reaction …, aim ±…°, sight …uu)` per bot at possession, and **no**
   `BotTuningTable is unset` warning.

## Done when

- [ ] `probe` reported all 21 node types present, output in the Log
- [ ] `ST_BNBot` read back from a fresh load contains `Evade` and `Cover` states and a `Shoot`
      state with a strafe task, `compiled: YES`
- [ ] `DT_BNBotTuning` read back with four tier rows; Marine's sight is 1200/1500/70
- [ ] PIE prints a per-bot tier line and no missing-table warning
- [ ] A bot has been SEEN to sidestep while firing (the 19-of-20-forward measurement is the
      before; anything other than all-forward is the after)

## What this ticket does NOT cover

Cover and grenade evasion are **behaviours nobody has watched yet** — they could not be, on a
build where the states do not exist. `docs/BREACHPOINT-NEXT-TEST-MATCH.md` §5d steps 9–12 is the
protocol for judging them, and it is the FOUNDER's pass, not this ticket's: this ticket only has
to make them reachable. Do not tune a number because a first impression felt wrong; write the
impression in the Log and let the tier row change be its own decision.

## Log

_(terminal: the probe output, the read-backs, and anything handed back)_
