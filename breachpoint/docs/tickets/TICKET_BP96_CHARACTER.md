# TICKET — The pawn is a body: BRCharacter and the CMC subclass

> STATUS: open — cut 7 Aug 2026. Blocked on BP95 DONE. First rung-4b packet.

Founder directive: the pawn is a body, not a brain. It owns meshes, the GAS init dance, and
its own death *consequence* — never health, never score, never weapon logic. The movement
component is a **subclass**, never a rewrite: saved moves, corrections and smoothing are the
most battle-tested networked code in the engine and re-implementing them is not "our gameplay
code", it is re-writing an engine subsystem.

**Ordering law:** the CMC's `FSavedMove_BR` lands before any ability that sets a movement
flag (BP100 sprint, BP102 grapple). Both flags are declared here even though nothing sets
them yet — adding a compressed flag later is a wire-format change.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP95 DONE — `ABRPlayerState` hosts the ASC; `ABRPlayerController` relays input tags
- BP92 DONE — `UBRInputComponent` + `DA_InputConfig` exist
- owner_path: `Source/Breachpoint/Character/`

## Steps (in order)

1. **[builder]** `Character/BRCharacter.h/.cpp` — rebuilt, not patched:
   - **Dual mesh:** `Mesh1P` (arms + weapon, `bOnlyOwnerSee`, `bCastDynamicShadow=false`,
     `VisibilityBasedAnimTickOption = OnlyTickPoseWhenRendered`) attached to the camera;
     `Mesh3P` = inherited `GetMesh()` with `bOwnerNoSee`, casts shadow. Weapon attaches to
     both by socket.
   - **The GAS init dance:** implement `IAbilitySystemInterface` by **forwarding to the
     PlayerState**. Call `InitAbilityActorInfo(PS, this)` in `PossessedBy` (server) **and**
     `OnRep_PlayerState` (client). Both paths, always — this is the canonical respawn-safe
     wiring and one missing half is a class of bug that only shows on one topology.
   - **`CheckReady()` — the convergence barrier (gap 1).** ASC, equipment and input each call
     it when they finish initialising; it broadcasts `OnCharacterReady` **once**, when all
     have. Nothing downstream may assume ordering between `PossessedBy`, `OnRep_PlayerState`
     and `OnRep_PlayerController`. Chains to `ABRPlayerController::FOnCombatSurfaceReady`.
   - **Death is a consequence, not a decision:** on `Event.Death`, play the cue, ragdoll
     `Mesh3P`, disable collision and input, and wait for GameMode. It does not score, it does
     not decide respawn, it does not touch attributes.
   - Remote aim: read the engine's replicated `RemoteViewPitch` for 3P aim; do not add a
     bespoke replicated pitch.
   - One combat helper only: the server-side rear-arc check used by BP100's melee.
2. **[netcode-builder]** `Character/BRCharacterMovementComponent.h/.cpp` — subclass:
   - `FSavedMove_BR` + `FNetworkPredictionData_Client_BR` with compressed flags for
     **sprint** and **grapple**. Declare both now (see ordering law), even unset.
   - `GetMaxSpeed()` override: use `MoveSpeedBase` for the **walking case only** and **only
     when non-zero** (gas-purity 2 Aug amendment — zero means unset). `MaxWalkSpeedCrouched`
     and all non-ground modes stay `Super::GetMaxSpeed()`.
   - Grapple detach rules (arrival radius, jump-cancel) live here; the *decision* to grapple
     is BP102's ability. Expose `ApplyGrappleRootMotion(FVector Target, float Strength)` now,
     unimplemented-but-declared, so BP102 is an ability packet and not also a CMC packet.
   - Halo-feel numbers (air control, jump velocity, gravity scale, friction) are **CMC
     defaults in `Config/DefaultGame.ini`**, not code.
3. **[builder]** Replace BP92's input stub: bind through `UBRInputComponent`, forward ability
   tags to `ABRPlayerController::InputTagPressed/Released`, unbind on unpossess using the
   returned handles.
4. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 3: PIE — spawn,
   possess, move, crouch, jump; `OnCharacterReady` fires exactly once. **Rung 4a** (dedicated
   + 2 clients): both pawns visible and moving, asserted in threes. **Rung 4b REQUIRED**
   (listen server + 1 remote client): the GAS init dance runs a different path on a host that
   is also a player — assert server-authority view, host-local view and remote-client view
   **separately**. Then re-run 4a under `-PktLag=120 -PktLoss=5`.
5. **[critic REFUTER]** Attack surface: what if `OnRep_PlayerState` arrives before
   `PossessedBy`? After? Never (spectator)? Does `CheckReady` fire twice on a respawn into a
   reused PlayerState? Does the 1P mesh ever render for a remote client? Does ragdoll on a
   simulated proxy desync the capsule?

## Done when

- [ ] `InitAbilityActorInfo` is called from BOTH `PossessedBy` and `OnRep_PlayerState`
- [ ] `OnCharacterReady` fires exactly once per possession under all three arrival orders
      (asserted in a spec, not observed)
- [ ] `grep` finds no health, score, ammo, or weapon-state member on `ABRCharacter`
- [ ] `FSavedMove_BR` declares sprint AND grapple flags; `ApplyGrappleRootMotion` is declared
- [ ] `GetMaxSpeed()` falls back to `Super` when `MoveSpeedBase` is zero — asserted
- [ ] No movement literal in C++; all Halo-feel numbers are in `Config/DefaultGame.ini`
- [ ] Rung 1 as above; rung 3 green; **rung 4a green AND rung 4b green, each asserted in
      threes** — 4a green is not 4b evidence
- [ ] Critic REFUTER pass recorded with findings verbatim
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder owns the pawn; netcode-builder owns the CMC and co-signs the init dance;
  anim-builder consults on socket names only (ABPs are a later art packet).
- Binary files this ticket OWNS: none. `BP_BRCharacter` may exist only under R26 (direct BP
  child, defaults only, empty graph) — prefer `Config/DefaultGame.ini`.
- Out of scope: abilities, equipment, respawn policy, anim blueprints. A movement *ability*
  belongs to BP100/BP102; this packet builds the flags they will set.

## Log

(append findings here, dated, newest last)
