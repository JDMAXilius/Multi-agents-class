# TICKET — BP65: The motion tracker is in scope and nothing produces its data

> STATUS: open — `contract_gap` G6, filed by BP26's cut, 2 Aug 2026. The sixth C++ gap. This
> ticket **routes and scopes**; it does not build the producer, because the producer is a
> replicated authority decision and that is netcode-builder's packet.

Founder directive: `docs/UI-DESIGN-SYSTEM.md` §5 records the reversal in the founder's own words —
*"**REVERSED by founder decision, 2 Aug 2026 — the motion tracker IS in scope**, built 1:1 from
Halo Infinite (Arena: 18 m precise blips, 30 m edge-direction; BTB 24 m / 40 m; crouch-walk and
Walk-binding movement undetected; disabled in Ranked and Tactical Slayer)."* That is a complete
behavioural spec and **no ticket owns it**. `grep -ril "motiontracker\|radar" Source/` returns
nothing: there is no producer, no replicated contact list, no ViewModel field, no widget.

A tracker is server-authoritative by construction — it tells a client where enemies are. A widget
that computes its own contacts from whatever the client happens to know is a wallhack with a
sweep animation. **This ticket exists so that the widget packet is never written first.**

**Ordering law:** step 1 (the authority shape) gates everything. No widget, no ViewModel field and
no HUD slot is cut until the producer's packet exists.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: files-only
- The gap is still open: `grep -ril "motiontracker\|MotionTracker\|radar" Source/` returns
  **nothing**, and no file under `docs/tickets/` other than this one names the motion tracker.
- `docs/UI-DESIGN-SYSTEM.md` §5 still records the reversal as in scope (if the founder has
  reversed again, close this ticket rather than scoping it).
- owner_path: `docs/tickets/`, `docs/DECISIONS-OWED.md`

## Steps (in order)

1. **Route it.** Write the producer's shape as a decision entry: what is replicated (a bounded
   contact list, relevancy-culled and per-viewer — never every pawn's transform), at what rate, and
   who owns it. **That is a netcode-builder packet**, and this ticket's output is the packet
   description plus the open questions, not the code. Owner: **lead**, with **netcode-builder**
   consulted on the replication shape.
2. **Name the rules the producer must implement, from §5, verbatim** — 18 m precise / 30 m
   edge-direction in Arena, 24 m / 40 m in BTB, crouch-walk and Walk-binding movement **undetected**,
   **disabled entirely in Ranked and Tactical Slayer**. Every one of those numbers is a tuning
   value and belongs in `Content/Data/` (law 3), proposed by **tuning-curator** — not inline in
   C++ next to a gameplay noun, and not in a widget.
3. **State the honest interim.** Until the producer exists, the HUD ships **without** the tracker;
   plan `curried-booping-waffle.md` W1 already says so (*"the motion tracker is drawn only when a
   producer exists"*). A placeholder that sweeps over an empty ring is worse than an absence — it
   reads as a bug to a playtester and as done to a reviewer. Record that here. Owner: **lead**.
4. Cut the two follow-on packets — the producer (netcode-builder) and, gated behind it, the widget
   (ui-builder) — as their own tickets. Owner: **lead**.

## Done when

- [ ] A netcode-builder packet exists that owns the replicated contact producer, naming the
      replication shape, the relevancy rule and the per-viewer filtering
- [ ] The §5 numbers are recorded as **tuning rows destined for `Content/Data/`**, with the
      Ranked / Tactical Slayer disable named as a rule the producer enforces **server-side** — a
      client-side "don't draw it" is not a disable
- [ ] The interim is written down: HUD ships with **no** tracker, no placeholder ring, and W1's
      verdict says so out loud
- [ ] A ui-builder widget packet exists and is **explicitly gated** behind the producer packet
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: lead (scoping and routing) · netcode-builder (consulted on step 1, owns the follow-on) ·
  tuning-curator (proposes the ranges) · ui-builder (owns only the gated widget packet)
- Binary files this ticket OWNS (lock before editing): none
- Out of scope — and this is the whole point of the ticket: **writing the producer here**, adding
  any replicated property or RPC, adding a field to `UBRVM_Combat`, and drawing anything. A UI
  packet that computes contacts client-side to "unblock the widget" is a `high` finding under law
  1, not a shortcut.
- Verified 2 Aug 2026: `UI-DESIGN-SYSTEM.md` §5 (~L120–132) carries the reversal, including its own
  correction to the record that the original "no motion tracker" entry mis-cited R12 (which is
  about bot legibility and says nothing about radar). **No closed ruling ever banned a tracker**,
  so nothing in `DESIGN-RULINGS.md` needs reversing and this ticket does not touch it.

## Log

(append findings here, dated, newest last — this is what the next session reads)
