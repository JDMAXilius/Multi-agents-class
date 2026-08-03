# TICKET — BP77: Draw the motion tracker — GATED behind BP76

> STATUS: **blocked on BP76.** Cut 3 Aug 2026 by BP65. Do not claim while BP76 has an unchecked
> box; the Kickoff gate below is the enforcement.

Founder directive: `UI-DESIGN-SYSTEM.md` §5 puts the tracker in scope; `docs/ui/HUD-AUDIT.md` and
BP69 §4 confirm the **art is already authored** (Figma `HUD / Motion Tracker` `24:2`, 133×133;
`SET Minimap /` Clear · Contacts · Disabled, `62:54/65/79`, 140×138). Only the feed is missing.

**Ordering law:** BP76 first, entirely. **A UI packet that computes contacts client-side to
"unblock the widget" is a `high` finding under law 1, not a shortcut** — it is the exact failure
BP65 was cut to prevent.

## Kickoff (machine-checkable)

- requires: **editor-live** (WBP authoring) after **engine-installed** rung 1
- **`docs/tickets/TICKET_BP76_MOTION_TRACKER_PRODUCER.md` has ZERO unchecked boxes.** This is the
  gate: `grep -c '^- \[ \]' docs/tickets/TICKET_BP76_MOTION_TRACKER_PRODUCER.md` must print `0`
- `grep -ril "FBRTrackerContact" Source/Breachpoint/Match/` returns a file (the producer exists)
- owner_path: `Source/Breachpoint/UI/`, `Content/UI/HUD/`, `Tools/gen_ui/wbp_plan.py`

## Steps (in order)

1. Add the ViewModel feed — a `UBRVM_Combat` field (or its own VM; make the call and record it)
   that mirrors the replicated contact array. **The VM copies; it never derives.**
2. `UBRMotionTracker` C++ widget in `Source/Breachpoint/UI/HUD/`, `BindWidget` members only, no
   `NativeTick` (the standing `ue5-ui-architecture` §8 greps apply).
3. `UBRHUDDirector` pushes the feed — one more wire in the existing producer layer, same
   one-direction rule as its other six.
4. `WBP_MotionTracker` authored bottom-left per `HUD-REFERENCE.md`; enemies in Enemy UI Colour,
   allies in Friendly UI Colour, both user-configurable.
5. Verify: rung 3 PIE, then rung 4 in threes with an observing client confirming it sees only
   what its own connection was sent.

## Done when

- [ ] BP76 is fully closed (the Kickoff gate, re-checked at landing)
- [ ] The widget reads only the replicated feed — `grep` for any pawn/actor iteration inside
      `Source/Breachpoint/UI/` returns nothing
- [ ] Rung 3 PIE render matches the authored art; rung 4 in threes
- [ ] Findings + decisions written to this Log

## Notes

- Crew: **ui-builder** owns all of it; **critic** re-checks that no contact is computed client-side
- Binary files this ticket OWNS (lock before editing): `Content/UI/HUD/WBP_MotionTracker.uasset`,
  `Content/UI/Layouts/WBP_HUDLayout.uasset`
- Out of scope: anything server-side, any replicated property, any change to `FBRTrackerContact`,
  and shipping a placeholder ring if BP76 slips (BP65's interim ruling forbids it)

## Log

(append findings here, dated, newest last)
