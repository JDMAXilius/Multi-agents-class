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

- [x] `probe` reported all 21 node types present, output in the Log
- [x] `ST_BNBot` read back from a fresh load contains `Evade` and `Cover` states and a `Shoot`
      state with a strafe task, `compiled: YES`
- [x] `DT_BNBotTuning` read back with four tier rows; Marine's sight is 1200/1500/70
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

### 25 Aug 2026 — terminal: probe PASS, built, read back. Assets done; the two PIE boxes are not.

**Rung: EDITOR.** The assets exist, compiled, and were read back out of the running editor.
Nothing here has been played. `compiles` ≠ `works`, and an asset that saved is not an asset that
was watched: the last two boxes are still open and are named below.

#### 1 — `python3 Tools/bn/62_bot_assets.py probe` — PASS, verbatim

```
--- build probe: every BN node type must exist in the RUNNING editor ---
  FBNHasTargetCondition            condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNHasLineOfSightCondition       condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNNeedsReloadCondition          condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNFaceTargetTask                task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNMoveToTargetTask              task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNFireBurstTask                 task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNStrafeTask                    task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNShouldTakeCoverCondition      condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNTakeCoverTask                 task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNIncomingBlastCondition        condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNEvadeBlastTask                task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNReloadTask                    task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNSelectWeaponTask              task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNMoveToPointOfInterestTask     task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNReactedCondition              condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNCanThrowGrenadeCondition      condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNThrowGrenadeTask              task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNInMeleeRangeCondition         condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNMeleeTask                     task       -> /Script/CoreUObject.ScriptStruct  OK
  FBNHasLastKnownCondition         condition  -> /Script/CoreUObject.ScriptStruct  OK
  FBNSearchLastKnownTask           task       -> /Script/CoreUObject.ScriptStruct  OK
  UBNAssetSettings (the trigger)   settings   -> /Script/BreachpointNext.BNAssetSettings  OK
  build probe PASS -- the editor is running current code
```

21 of 21, including all five R10 additions. The gate opened, so `build` ran. (Editor pid 16538,
launched with `-ModelContextProtocolStartServer`; `Source/BreachpointNext/AI/` unchanged since the
24 Aug rung-1 PASS, so rung 1 was not re-run.)

#### 2 — BEFORE and AFTER, from the bytes on disk

Independent of the builder's own report: `strings -a Content/BN/AI/ST_BNBot.uasset`. Note the
grep in the task as written (`FBN[A-Za-z]+(Task|Condition)`) returns **empty both before and
after** — the package stores struct names without the `F`, so the honest pattern is `BN…`.
That empty result is a property of the grep, not of the asset, and would read as "nothing landed"
to anyone who ran only that one.

BEFORE (`ST_BNBot.uasset`, 22 Aug 18:38, 154318 bytes) — 16 node types:

```
BNCanThrowGrenadeCondition   BNHasLineOfSightCondition   BNMoveToTargetTask     BNSearchLastKnownTask
BNFaceTargetTask             BNHasTargetCondition        BNNeedsReloadCondition BNSelectWeaponTask
BNFireBurstTask              BNInMeleeRangeCondition     BNReactedCondition     BNThrowGrenadeTask
BNHasLastKnownCondition      BNMeleeTask                 BNReloadTask
BNMoveToPointOfInterestTask
```

Missing, exactly the five the roadmap named: `BNStrafeTask`, `BNShouldTakeCoverCondition`,
`BNTakeCoverTask`, `BNIncomingBlastCondition`, `BNEvadeBlastTask`.

AFTER (`ST_BNBot.uasset`, 25 Aug 15:08, 186715 bytes) — 21 node types, the five now present:

```
BNCanThrowGrenadeCondition   BNHasLineOfSightCondition   BNMoveToTargetTask     BNSelectWeaponTask
BNEvadeBlastTask             BNHasTargetCondition        BNNeedsReloadCondition BNShouldTakeCoverCondition
BNFaceTargetTask             BNInMeleeRangeCondition     BNReactedCondition     BNStrafeTask
BNFireBurstTask              BNIncomingBlastCondition    BNReloadTask           BNTakeCoverTask
BNHasLastKnownCondition      BNMeleeTask                 BNSearchLastKnownTask  BNThrowGrenadeTask
BNMoveToPointOfInterestTask
```

The 16 → 21 diff is the four switched-off packets arriving in the asset the bots actually load.
(The roadmap's BEFORE list — "tasks `BNFireBurstTask`, `BNMoveToTargetTask` and nothing else" —
was the COMPILED tree's node list; the 16 above is the editor-data graph in the same file. Both
descriptions are of one stale asset, and both are now superseded.)

#### 3 — the editor's own read-back (`LogBN`, 19:08:22, verbatim)

```
=== BN bot assets: BUILD ===
-- DT_BNBotAmbitions --
asset      : /Game/BN/Data/DT_BNBotAmbitions.DT_BNBotAmbitions (reused existing)
rows       : 3, mirrored from UBNBotBrain::DefaultRow
save       : OK
-- DT_BNBotTuning --
asset      : /Game/BN/Data/DT_BNBotTuning.DT_BNBotTuning (created)
rows       : 4, mirrored from ABNBotController::DefaultTuning
save       : OK
-- ST_BNBot --
asset      : /Game/BN/AI/ST_BNBot.ST_BNBot (reused existing)
states     : Root > [Evade, Engage > [Rearm, Arm, Nade, Knife, Cover, Close, Shoot(+strafe)], Search, Roam]
compile    : OK
save       : OK
=== read-back ===
ST_BNBot   : FOUND at /Game/BN/AI/ST_BNBot.ST_BNBot
  schema   : StateTreeAIComponentSchema_0
  compiled : YES (ready to run)
  state    : Root (0 enter conditions, 0 tasks, 0 transitions)
    +- Evade (1 enter conditions, 1 tasks, 2 transitions)
    +- Engage (1 enter conditions, 0 tasks, 0 transitions)
       +- Rearm (1 enter conditions, 2 tasks, 2 transitions)
       +- Arm (0 enter conditions, 1 tasks, 2 transitions)
       +- Nade (2 enter conditions, 2 tasks, 2 transitions)
       +- Knife (2 enter conditions, 2 tasks, 2 transitions)
       +- Cover (1 enter conditions, 1 tasks, 2 transitions)
       +- Close (0 enter conditions, 2 tasks, 2 transitions)
       +- Shoot (2 enter conditions, 3 tasks, 2 transitions)
    +- Search (1 enter conditions, 1 tasks, 2 transitions)
    +- Roam (0 enter conditions, 1 tasks, 2 transitions)
```

Matches step 3's expectation exactly, `Shoot` on three tasks, `compiled: YES`.
`LogStateTreeEditor: Compile StateTree '…ST_BNBot' succeeded.` is in the same log at 19:08:22.

**One honesty note on "fresh load."** `AuditBotAssets` uses `LoadObject` on the path the ini
names — which proves the object lives at the RIGHT path, but returns the already-resident package
rather than re-reading the bytes. A literal fresh load means restarting the editor, which R29 and
this session's instructions forbid. The disk-side proof is §2 above: the saved bytes carry all 21
node types. Anyone wanting the strict version gets it free on the next editor start.

#### 4 — `DT_BNBotTuning`, read back over MCP (`DataTableTools.list_rows` + `get_rows`)

Rows: `Recruit, Marine, ODST, Spartan` — four, on `FBNBotTuningRow`.

| row | reaction | aim° | reaim | sight | loseSight | periph° | hearing | jumpCD | strafe | juke | evades |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Recruit | 0.60–1.10 | 7.0 | 0.9 | 900 | 1200 | 55 | 2200 | 6.0 | 2.2 | 0 | false |
| Marine | 0.22–0.45 | 2.5 | 0.5 | **1200** | **1500** | **70** | 2200 | 1.5 | 1.2 | 3 | true |
| ODST | 0.14–0.28 | 1.4 | 0.35 | 1500 | 1900 | 85 | 2200 | 1.2 | 0.9 | 2 | true |
| Spartan | 0.08–0.16 | 0.6 | 0.2 | 1800 | 2200 | 100 | 2200 | 0.9 | 0.7 | 2 | true |

Marine reads **1200 / 1500 / 70**, the founder's arena numbers, unchanged. Every value matches
`ABNBotController::DefaultTuning` field for field — the table is a faithful mirror, which is what
makes it safe to retune.

#### 5 — the ini contract, verified, NOT edited

`[/Script/BreachpointNext.BNGameData] BotTuningTablePath=/Game/BN/Data/DT_BNBotTuning.DT_BNBotTuning`
and `[/Script/BreachpointNext.BNBotController] BotTier=Marine`. The table was created at exactly
that path, so no asset had to move and no ini line was touched.

#### 6 — PIE: NOT DONE, and not for a reason I can argue with

`EditorAppToolset.StartPIE` was refused by the session's permission classifier, twice, so no PIE
session was started and no `BNBots: … fights at tier Marine` line was produced by this ticket.
The last `BotTuningTable is unset` warning in the log is from the 19:05 PIE session, which ran
**before** the table existed at 19:08 — it is the BEFORE, not a result, and nobody should read it
as one. Both remaining boxes need a human at the keyboard:

- **PIE tier line + no missing-table warning** — press Play, read the first `BNBots:` lines.
- **A bot SEEN to sidestep while firing** — the 19-of-20-FORWARD measurement is the before.
  `LogBN: BNLocomotion: … (STRAFE-LEFT/RIGHT)` while a bot is in `Shoot` is the after. Worth
  saying plainly: the strafe task is now IN the compiled tree, which is a different claim from
  the bot visibly strafing, and only the second one closes that box.

#### Handbacks (things I could not do from inside my owner path)

- **`62_bot_assets.py audit` does not read `DT_BNBotTuning` at all.** It reads `ST_BNBot` and
  `DT_BNBotAmbitions` and stops — so the table this ticket exists to create is invisible to the
  script's own read-back, and step 4 had to be done by hand through `DataTableTools`. Same for
  `UBNBotAuthoring::AuditBotAssets`, which prints the ambition rows and not the tuning rows.
  Both fixes live in `Tools/` and `Source/`, outside this ticket's owner path
  (`Content/BN/AI/`, `Content/BN/Data/`, `docs/tickets/`), so neither was made.
- **`LogsToolset.GetLogEntries` failed** — `Log category 'LogsToolset' not found` — so the
  script's in-band log read is dead and it silently falls through to the thin asset audit. Every
  `LogBN` block quoted above came from `~/Library/Logs/Unreal Engine/BreachpointEditor/Breachpoint.log`
  instead. Also `Tools/bn/62_bot_assets.py` calls `get_row_names`, which does not exist; the tool
  is `list_rows`.
- **Observation, NOT tuned** (per "do not tune a number because a first impression felt wrong"):
  `HearingRange` is 2200 on all four tiers, because `DefaultTuning` only overrides it… nowhere.
  R10.2 describes hearing as "a tier number… LONGER than sight", and at 2200 that holds for
  Recruit (900) but is only +400 on Spartan (1800) — the asymmetry the design leans on narrows as
  the tier rises. It is a row field, so the table can carry the fix without a recompile. Left
  alone; it is its own decision.
