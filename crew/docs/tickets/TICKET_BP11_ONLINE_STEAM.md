# TICKET — BP11: Online, Steam, Spotter, and CI — the beta ticket

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP08 (M4 passed). W5.

Founder directive: two boxes over Steam, telemetry flowing, Spotter never load-bearing, CI
running the ladder without a human. Host-quit behavior is defined here, not discovered.

**Ordering law:** 1 → 2; 3, 4, 5 parallel after 1.

## Steps (in order)

1. Port `BRSessionsSubsystem` (Steam OSS): host listen server + travel to BR_Arena01,
   invite/accept → `FindAndJoinBestSession` → travel; Warmup-phase roster assembly;
   host-quit = match end with defined UI state on remotes. `IBRServerLifecycle` listen impl
   final. Owner: **services-builder**. Contract: `online-services.md` (twice-tested law).
2. Steam depot: App ID config, `Tools/steam/app_build.vdf` + depot; packaged Win64 demo
   build uploads and boots on a second machine. Owner: **builder** (tools),
   **services-builder** consults. TD holds upload credentials.
3. `BRTelemetrySubsystem` (authority-only folding of match events) +
   `BRSpotterSubsystem`: async Haiku calls (≤12 event + ≤8 coach), 3 s timeout,
   `DT_SpotterLines` canned fallback FIRST, replicated strings into killfeed/carnage.
   Offline ⇒ identical game minus flavor (proven by pulling the network). Owner:
   **ai-builder**. Contract: gas-purity (strings only), netcode (key never leaves host).
4. **CI wiring**: rungs 1–3 on push; rung 4 + soak nightly; morning report to the board.
   Owner: **builder**, **verifier** validates the wiring by breaking it once.
5. Verify + refute: full rung ladder on the PACKAGED build (editor ≠ packaged); critic:
   Steam ticket replay? key in logs? host-advantage sweep on session flows; Spotter
   blocking-path hunt. Owners: **verifier**, **critic**.

## Done when

- [ ] Two machines over Steam: invite → join → full match → rematch (M5 build)
- [ ] Host-quit produces the defined state on remotes (tested, not discovered)
- [ ] Spotter: canned-first proven, API path additive, offline identical
- [ ] CI posts ladder results without a human in the loop
- [ ] Packaged-build ladder green (not just editor)
- Crew: services-builder · builder · ai-builder · verifier · critic
- Out of scope: GameLift/dedicated (Phase 2), quickmatch, Steam auth validation (Phase 2)

## Log

(append findings here, dated, newest last)
