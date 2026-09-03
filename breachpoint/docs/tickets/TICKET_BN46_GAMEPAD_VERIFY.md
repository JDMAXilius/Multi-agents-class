# TICKET — BN46: the gamepad, in hand — verify, and fix what only a pad reveals

> STATUS: open — cut 3 Sep 2026 by the cloud lead at the founder's direction ("I have not
> tried with the gamepad, make sure you have it"). OWNER: **terminal**, LIVE EDITOR + a
> real controller. requires: editor-live, unreal-mcp, **a physical pad**.
> Method: **R46** — drive the editor with the unreal-mcp tools directly (`list_toolsets` →
> `describe_toolset` → `call_tool`), read every write back, paste receipts into the Log.
> A Python driver is the fallback only, and using it is recorded here.

## What is ALREADY BUILT — do not rebuild it, verify it

The cloud audited the whole chain today. Every piece exists; **not one of it has been held
in a hand.** The 3 Sep package commit says so itself: *"NOT verified: the gamepad itself —
the smoke runs registered only KBM, so no pad was connected."*

| Layer | State | Where |
|---|---|---|
| Gameplay bindings, 16 actions | **built, audited 77/77, unproven** | `Tools/bn/10_input_assets.py` ACTIONS → `IMC_BNNext` |
| Stick dead zone 0.25 | built | `DEADZONE` modifier, both sticks |
| Look scalar 1.75 + NEGATE_Y | built, **explicitly a guess** | the package commit calls it "a starting point, not a measured one" |
| CommonUI accept/back | **wired 3 Sep** | `[/Script/CommonInput.CommonInputSettings] InputData=…InputData_Default` |
| Gamepad profile resolution | **wired 3 Sep** | `[CommonInputPlatformSettings_Windows …]` `DefaultGamepadName=Generic` + both ControllerData rows |
| Menu buttons navigable | **structurally yes, unproven** | `UBRButton`, `UBRHighlightButton`, `UBNPromptButton` all derive `UCommonButtonBase` |
| Initial focus per screen | built | `NativeGetDesiredFocusTarget` → `PlayButton` / `StartButton` |

The founder's layout, as generated: **R2** fire · **L2** ADS · **L1** grapple · **R1** dash
· **A** jump · **B** melee · **Y** weapon next · **X** reload · **L3** sprint · **R3**
crouch · **dpad ↑** grenade · **dpad ↓** weapon prev · **dpad ←/→** lean · **Share**
scoreboard (hold) · **Options** menu · sticks move/look.

## Do — plug a pad in and walk it

1. **The menu first**, because it is the newest and the least proven. On `FE_MainMenu`:
   left stick / dpad moves focus between rows; **A** activates; **B** backs out of PLAY
   SETUP to the front end. Confirm the front end does NOT back out of itself (its
   `bIsBackHandler` is deliberately false — home has no "back", quitting is a choice).
2. **THE MOST LIKELY REAL DEFECT — focus visuals.** The button hover state was authored
   and verified for a MOUSE ("hover changes the background plate only", verified across
   all 8 buttons). A pad does not hover, it FOCUSES, and `UCommonButtonBase` treats those
   as different states. **If a focused row looks identical to an unfocused one, the menu
   is unusable on a pad even though every input works.** Check first, and if it is wrong
   the fix is the button style's focused state, not the input mapping.
3. **Double-handling check.** `UBNScreen_PlaySetup::NativeOnKeyDown` catches
   `Gamepad_FaceButton_Right` and deactivates — written when this project shipped no
   `UCommonUIInputData` and CommonUI's back action bound to nothing. **That is no longer
   true as of 3 Sep.** Press B once and confirm it pops ONE level, not two. If it double-
   pops, the C++ key handler is now redundant and should go (cloud will remove it — file
   it here); until then it is a belt beside a working brace and harmless either way.
4. **In-match**, every verb in the layout above, one at a time, against the HUD: fire,
   ADS, grapple, dash, jump, melee, weapon next, reload, sprint, crouch, grenade, weapon
   prev, lean both ways, scoreboard hold, menu.
5. **Feel pass on the two guessed numbers.** Look scalar 1.75 and dead zone 0.25 are the
   only tuning here and neither was measured. Stick look is a RATE where mouse look is a
   DELTA, so the scalar is the one to move; report a number that felt right rather than a
   verdict. Both live in the generator's table — retune there and re-run it, never in the
   asset (law 7: `10_input_assets.py` owns `IMC_BNNext` and rebuilds it every run).
6. **Design question for the founder, not a defect:** the MAP / MODE / PLAYERS rows cycle
   on activate, so on a pad they step with **A** only. The console convention is
   **left/right to change a value in place**. Say whether it reads wrong with a pad in
   hand; changing it is a real change (the rows would need to consume dpad ←/→ while
   focused) and is not in this ticket's scope.

## Done when
- [ ] The menu is fully navigable on a pad, with **visible focus**, and B pops exactly one level
- [ ] All 16 gameplay verbs confirmed in a match, listed here with any that did nothing
- [ ] Look scalar / dead zone either confirmed or retuned **in the generator** and re-run
- [ ] Anything that needed a fix landed via unreal-mcp with the read-back pasted here
- [ ] The founder's own verdict with a pad in hand

## Log
