# TICKET — BN43: menu art — our captures, their boxes

> STATUS: open — cut 1 Sep 2026 by the cloud lead. OWNER: **terminal**, LIVE EDITOR.
> DEPENDS ON BN42's loop walking. This is WAVE 3 — polish, sacrificed first if the
> deadline bites (BN-FRONTEND-PLAN.md's knife).

**The IP line (01-MENU-MEASURED.md §6, binding):** the Figma mocks embed 343-owned art —
Halo screenshots, Spartan renders, playlist key-art. NONE of it is exported. From Figma we
take NUMBERS and any panel geometry we authored; every image asset is generated from OUR
game.

## Do

1. **Map previews** (349×196.7 slot): high-res screenshot per roster map — BR_Spillway,
   BR_Arena01, BR_Aquarius — from a flattering angle, imported as `T_Preview_<Map>`,
   wired into the setup screen's Preview Photo (add a soft-path per map entry if needed —
   that is an INI field + one optional Image bind, cloud pre-approved).
2. **FE backdrop**: a Spillway vista camera in FE_MainMenu (the stage IS the background,
   zero texture cost) — or one captured still if the vista fights the widget contrast.
3. **News card** (349×222): one of the BN37 build screenshots + "NEW ARENA: SPILLWAY"
   overlay text from C++/asset — our news, really.
4. Contrast pass at the referee's boxes: Profile Bar band, panel fills vs backdrop —
   `BNUIColors` tokens only.

## Done when
- [ ] Three previews + backdrop + news card, all from our renders, landed via direct
      unreal-mcp calls (R46 — screenshot/import/set-brush through the toolsets; a script
      only as a LOGGED fallback)
- [ ] Side-by-side screenshots to the founder

## Log
