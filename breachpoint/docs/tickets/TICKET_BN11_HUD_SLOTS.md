# TICKET — The HUD's new optional slots: feed parts, death screen, score bars

> STATUS: open — cut 24 Aug 2026 by the cloud lead. Needs a LIVE EDITOR. Additive only: every
> widget below already exists and already loads; nothing here can break a HUD that works today.

Founder directive: R7.6 and R7.7 closed four of the seven C++ gaps the terminal handed back on
22 Aug (`TASK-R7-WBP-HUD` Log, "C++ gaps — the design needs these and the bind contract cannot
carry them"). The ViewModel fields now exist and are compiling; the WBPs have nowhere to put
them. Each slot below is a `BindWidgetOptional`, so a WBP that skips one still loads — which is
also why a missed slot is invisible rather than loud, and why the read-backs matter.

**Ordering law:** none between the three widgets. Do them in any order, or one at a time.

## Kickoff (machine-checkable)

- requires: editor-live
- Rung 1 PASS for `BreachpointEditor` (it was, 24 Aug) — the ViewModel getters these bind
  against are `UBNVM_Combat::GetKilledByWeaponIcon / GetRespawnFraction` and
  `UBNVM_Match::GetSelfScoreFraction / GetTopScoreFraction`
- The eleven WBPs from `TASK-R7-WBP-HUD` exist and load clean
- owner_path: `Content/BN/UI/`
  <!-- ASSETS ONLY, and layout only within them: no graph nodes, no variables, no bindings, no
       colours (every colour is C++'s — a WBP that sets one is a finding). ASSET-RULES §5. -->

## Steps (in order)

1. **`WBP_BNKillfeedEntry` — the feed's three parts** (design node `30:22`). Add two optional
   `TextBlock`s beside the existing `LineText` and `WeaponIcon`:
   **`KillerText`** at x8 and **`VictimText`** at x110, with the 22×8 glyph between them at x78.
   **Bind BOTH or NEITHER.** With both present and both names non-empty the row draws the three
   parts and hides `LineText`; with either missing it draws `LineText` alone. `LineText` stays
   REQUIRED — it is the only correct render for "X died" and for a suicide, which have no killer
   to lay out.
2. **`WBP_BNScreen_Death` — three optional additions** (nodes `36:9`, `36:11/12/13`, `36:6`):
   - `Image` **`WeaponIcon`**, brush EMPTY — C++ fills it from the same `Icon` column the tray
     and the feed glyph read, so one filled DT row lights three places.
   - `TextBlock` **`CountdownText`**, the bare numeral, LARGE, beside the existing sentence.
   - `ProgressBar` **`RespawnRing`**. **Read the note below before building this as a ring.**
3. **`WBP_BNMatchBand` — the two score bars.** The self bar becomes a `ProgressBar` named
   **`SelfScoreBar`** and the leader bar a `ProgressBar` named **`TopScoreBar`**, at their
   measured slots (x24 y7 60×8 and x240 y7 44×8). C++ fills both as fractions of the score limit
   and HIDES them until match data is live — an empty bar claims the score is zero, which is the
   lie the band's dashes already exist to avoid.
4. **Read back** each of the three WBPs from a fresh load: parent class plus the full child tree
   with exact `BindWidget` names.
5. **PIE, solo.** Expect no new `did not resolve` lines and no `placed no rows` warnings; the
   HUD must look exactly as it does today except where a new slot has something to say.

## THE RESPAWN RING, and why it ships as a bar

The design draws a radial sweep. A radial sweep is a MATERIAL (Tier 4), and the combat ViewModel
updates the respawn countdown **once a second** because law 4 forbids the per-frame push a smooth
sweep would need from C++. So the C++ binds a `UProgressBar` and publishes `RespawnFraction`: a
bar that steps once a second is honest, a ring that stutters is not. When the radial material
lands it reads the same fraction under the same bind name and no C++ changes.

## Done when

- [ ] `WBP_BNKillfeedEntry` read back with `KillerText` and `VictimText` (or with neither, stated
      as a deliberate choice in the Log)
- [ ] `WBP_BNScreen_Death` read back with `WeaponIcon`, `CountdownText`, `RespawnRing`
- [ ] `WBP_BNMatchBand` read back with `SelfScoreBar` and `TopScoreBar` as `ProgressBar`s
- [ ] PIE: no new unresolved-bind lines, HUD unchanged where nothing was added

## Handed back to the FOUNDER, not to this ticket

Four read-backs from `TASK-R7-WBP-HUD` Step 5 have never been run because they need a hand on the
keyboard: the **death overlay**, **hold-Tab scoreboard**, **post-match pin**, and the **pause
menu in Standalone** (Escape is Stop-PIE in the editor — the pause menu cannot be tested in a PIE
window at all). `docs/BREACHPOINT-NEXT-TEST-HUD.md` §4b–5 is the protocol. The R7.3 cause-of-death
line and the stowed slot are in the same set.

## Log

_(terminal: the read-backs, and anything handed back)_
