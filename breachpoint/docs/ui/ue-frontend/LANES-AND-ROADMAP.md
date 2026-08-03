# Lanes and roadmap — what is left, and which machine can do it

**Status:** v1, 3 Aug 2026. Cut after the menu audit (`CPP-AUDIT.md`) and the HUD audit
(`HUD-CPP-AUDIT.md`) landed eight packets of C++ across nine commits. Everything those packets
changed is **rung 0: written, grep-verified, compiled by nothing.**

This file answers one question — *what remains, and where can it physically be done* — because
the remaining work is gated by **machine capability**, not by design. Four lanes exist and only
one of them is a cloud session.

---

## 1. The four lanes

| Lane | What it is | What it can do | Live in this container? |
|---|---|---|---|
| **files-only** | any session, incl. cloud | docs, plans, C++ *authoring*, ticket work | ✅ yes |
| **figma-mcp** | any session with the Figma MCP | measurement reads against the reference file | ✅ **yes — connected now** |
| **engine-installed** | a box with source-built UE 5.8, headless | **compile**, Automation specs, Gauntlet | ❌ no toolchain here |
| **editor-live** | that box with the editor OPEN + its MCP | build/verify WBPs, screenshots, asset reads | ❌ no Unreal MCP here |

`AUTHORING-MATRIX §5` owns these definitions; this table is the UI lane's application of them.
**R21 still binds `editor-live`: one editor, one driver, no overlapping build.**

---

## 2. The gate chain — why order is not a preference

```
   [BP71] COMPILE  ── engine-installed ────────────────────────────┐
      the eight packets' C++, three targets                        │
      ↓ nothing below is claimable until this is green             │
                                                                   │
   [BP72] BUILD THE ASSETS ── editor-live ──────────────┐          │
      --verify the 10 built · rebuild at digest         │          │
      · build the 12 menu WBPs                          │          │
      ↓ closes BP70 D1 by construction                  │          │
                                                        ↓          ↓
                                      ┌──────── PIE: the HUD renders live data
                                      │         for the first time (rung 2)
   [BP73] MEASUREMENTS ── figma-mcp ──┘
      9 DECIDE values · runs NOW, gates nothing above
      but every one it leaves open is a number a
      builder would otherwise invent

   [BP74] THE SIX PRODUCER GAPS ── files-only to write, BP71 to verify
      team id · phase tags · grapple · rocket · hit-marker · spotter
      ↓ each one is a HUD element that currently renders "unknown" forever
```

**The chain's one hard rule:** BP71 is the gate. Eight packets of unverified C++ is the largest
single risk in the project right now — not because the changes are speculative (each was
grep-verified and pattern-matched to adjacent proven code) but because *nothing has read them
back*. A compile is the cheapest possible disagreement with that work.

---

## 3. What is NOT in the chain, and why

**The three founder DECIDEs** (`HUD-CPP-AUDIT.md` §8, logged in `TICKET_BP70`): D3's gate
(health-damage vs shield-state), the killfeed corner, the tray split. Each is provisionally
ruled and recorded. They block *their own items only* — no packet waits on them, and any of the
three can be reversed after BP72 renders the screens, which is the right time to look.

**BP24 (lobby ViewModel)** and the eleven front-end ViewModel gaps: already ticketed, already the
front end's real critical path, and untouched by this pass. `TICKETS.md` §1 owns that map.

**The art.** `T_UI_Weapon_Unknown` (blocks BP70 D2), weapon glyphs, hit-marker states, the
emblem families. No lane here produces art; it is the long pole and it is `ART-PROMPT-LIBRARY`'s.

---

## 4. The honest state, per surface

| Surface | C++ | Assets | Data | Renders? |
|---|---|---|---|---|
| **HUD** | complete + producer wired (BP71 to prove) | 7 built, unverified since the plan moved | live after BP71 | **not yet** |
| **Main menu** | complete, 0 new classes owed | **0 of 12 built** | `UBRVM_FrontEnd` wired, no producer | **no** |
| **Front end, waves 2–7** | ~24 component classes exist | 0 built | 10 VM gaps open | no |

The HUD is one compile and one asset pass from being the first thing in this project that shows
real numbers. The menu is that plus twelve assets. Everything else is behind BP24.

---

## 5. Ticket index for this pass

| Ticket | Lane | One line |
|---|---|---|
| **BP71** | engine-installed | Compile the eight packets and run the ladder. **The gate.** |
| **BP72** | editor-live | `--verify`, rebuild at digest, build the 12 menu WBPs. |
| **BP73** | figma-mcp | Close the 9-value DECIDE ledger by reading the reference nodes. |
| **BP74** | files-only + BP71 | The six producer contract gaps the HUD director filed. |

**Closed by this pass:** BP66 (killfeed BindWidget contract — landed in HUD-C).
**Amended:** BP70 (all three defects re-judged; two of its prescriptions were wrong).
