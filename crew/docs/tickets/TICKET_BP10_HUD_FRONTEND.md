# TICKET — BP10: HUD, front end, and the sound of information

> STATUS: open — cut by lead session, 29 Jul 2026. HUD v1 after BP03; full scope lands
> W3–4. Zero polling, zero property bindings — event-driven per the UI doctrine.

Founder directive: the game reads instantly. Shield-vs-flesh hit markers are gameplay
information, not decoration. With radar cut, FOOTSTEP/WEAPON AUDIO is the awareness system —
MetaSounds cues are a gameplay requirement in this ticket, not polish.

**Ordering law:** 1 → 2; 3–4 parallel after 2.

## Steps (in order)

1. `BRUIManagerSubsystem` (CommonUI layers: GameHUD/Menu/Modal) + `BRActivatableWidget`
   base + `BRViewModels` (Combat + Match, FieldNotify, fed by ASC delegates + RepNotify —
   grep gate: no NativeTick, no property bindings). Owner: **ui-builder**.
2. `BRHUDLayout` + BP visual subclasses: shields-over-health bars, ammo block, grenade
   count, grapple ring, timer + team score, rocket countdown, killfeed (widget pool) with
   Spotter-line append slot, shield-hit vs flesh-hit markers. Figma reference frames via
   MCP where available; BP widgets hold layout ONLY. Owner: **ui-builder**.
3. Front end: MainMenu → Host / Join / Solo-vs-bots; death overlay (killer cam + respawn
   timer); carnage report (K/D/A, accuracy, medals, coach line slot); rematch flow.
   Gamepad-navigable throughout (CommonUI input routing). Owner: **ui-builder**.
4. **Audio-as-information pass**: MetaSounds via GameplayCues — directional footsteps
   (surface-aware), reload audible at 15 m, shield break/recharge signatures, medal
   stings, rocket-spawn arena cue. Cue discipline: loops = WhileActive/Removed pairs;
   confirmed one-shots = Executed. Owner: **builder** (audio), ui-builder consults mix.
5. Verify + refute: join-in-progress UI (null PlayerState frames — honest empty states);
   critic: stale-data first frame, gamepad dead-ends, killfeed pool exhaustion.
   Owners: **verifier**, **critic**.

## Done when

- [ ] Zero polling (grep-audited); all HUD state event-driven through the two ViewModels
- [ ] A blindfold test: tester identifies approach direction by footsteps alone
- [ ] Menu → match → death → rematch fully gamepad-navigable
- [ ] Join-in-progress shows honest states, never stale/garbage frames
- Crew: ui-builder owns · builder (audio) · verifier · critic
- Out of scope: settings screens, radar (Phase 2), cosmetics

## Log

(append findings here, dated, newest last)
