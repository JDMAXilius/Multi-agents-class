# Respawn — what it does, what is wrong with it, and what it is missing

**Cut:** 13 August 2026 by the cloud lead, at the founder's request: *"do a deep research and ultra
think. Multiplayer and GAS purity."*

---

## The flow as it stands

```
Health hits 0 ─ every machine ─ UBNHealthComponent::OnDeath
                                      └─ ABNCharacter::HandleDeath  (authority only)
AUTHORITY  UBNGA_Death (ServerOnly)
             ├─ ASC->CancelAbilities()          every verb's EndAbility takes its own GE off
             ├─ State.Dead applied              blocks input on EVERY machine, client included
             ├─ ExecuteGameplayCue(Death)       ragdoll, multicast
             ├─ movement stopped + disabled     MovementMode replicates
             ├─ SetIgnoreMove/LookInput(true)   the authority's controller copy
             └─ GameMode->RequestRespawn()      ◄── LAW 7 VIOLATION, see below
                    └─ 3s timer ON THE GAMEMODE (never on the dying pawn)
                          └─ RespawnPlayer
                               ├─ UnPossess + Destroy the corpse
                               ├─ ASC->CancelAbilities()               ends death → State.Dead gone
                               ├─ RemoveActiveEffectsWithGrantedTags(State)   the sweep
                               ├─ ApplyInitAttributes()                through the init GE
                               └─ RestartPlayer()                      engine picks a PlayerStart
```

## What is already right, and worth not breaking

Several things here are load-bearing and were clearly deliberate:

- **The timer lives on the GameMode, never on the dying pawn.** The corpse is destroyed before the
  delay is up; a timer on it would die with it and the player would never respawn.
- **`TWeakObjectPtr<AController>`** through the timer — a player who disconnects during the delay
  resolves to null and the callback returns instead of crashing.
- **Cleanup happens BEFORE the new pawn exists.** `CancelAbilities` → sweep → init GE → *then*
  `RestartPlayer`. The new body is born onto a clean ASC rather than being tidied up afterwards.
- **The `State` root tag is what makes the sweep one line.** `RemoveActiveEffectsWithGrantedTags`
  matches parents, so a single `State` query clears `State.Dead`, `State.Shields.Broken`,
  `State.Movement.*` and anything a future roadmap adds under it.
- **ASC init runs on BOTH roles** — `PossessedBy` (server) *and* `OnRep_PlayerState` (client). This
  is the single most common GAS-respawn bug in existence and BN does not have it. Worth stating
  plainly because I went looking for it expecting to find it.

## Finding 1 — the law 7 violation, and it is the contract's own example

`gas-purity.md` law 7, verbatim:

> **Events over calls at the seams.** Cross-system consequences travel as gameplay events
> (**`Event.Death` → GameMode**) and delegates — **an ability never reaches into GameMode**, UI, or
> another system's internals.

`UBNGA_Death::ActivateAbility` does exactly the thing named:

```cpp
if (ABNGameMode* GameMode = GetWorld()->GetAuthGameMode<ABNGameMode>())
{
    GameMode->RequestRespawn(Controller);
}
```

This is not pedantry, and the cost is concrete:

- **The ability now depends on the GameMode's class**, so `BNGA_Death.cpp` includes `BNGameMode.h`.
  A second game mode (training, warmup, a mode with no respawn at all) means editing the death
  ability, which has nothing to do with modes.
- **Corpse lifetime and respawn delay are the same number** and cannot diverge, because one call
  starts both. Wanting a 6-second corpse and a 3-second respawn requires restructuring.
- **Nothing else can react to a death.** A killfeed, scoring, an announcer, a spectator hand-off —
  each would have to be another call bolted into the death ability.

**Fix: the death ability broadcasts; the GameMode listens.** `ABNPlayerState` is the natural
broadcaster — it is the persistent object, it is what the GameMode already tracks per player, and
the law explicitly permits delegates alongside gameplay events.

## Finding 2 — the ignore-input flags may never clear on the server's copy

Death sets `SetIgnoreMoveInput(true)` / `SetIgnoreLookInput(true)` on the controller. The code's
comment says these are *"cleared by `ClientRestart` on the respawn"* — and
`APlayerController::ClientRestart_Implementation` does call `ResetIgnoreInputFlags()`.

**But `ClientRestart` is a client RPC.** On a listen-server host the controller is local and it
executes; for a **remote** client, the *server's* copy of that controller runs no such reset.

Whether that matters depends on a detail I cannot verify from here: a remote client's movement
arrives as `ServerMove` RPCs into the CMC rather than through `AddMovementInput`, so the server's
flag likely never gates it. **Likely is not verified.** The symptom if I am wrong is severe and
unmistakable — *a respawned remote client cannot move or look* — and the defence costs two lines.

**Fix: clear them explicitly in `RespawnPlayer`, server-side.** Belt and braces on an uncertainty,
rather than an engine-internals claim I would be guessing at.

## Finding 3 — the ordering that keeps death working is undefended

`ApplyInitAttributes()` runs before `RestartPlayer()`, so Health is back to 100 before the new
pawn's health component ever registers. That component deliberately watches **changes only** — its
own comment explains why: reading the value at registration would kill a new body on the frame it
spawned, because the persistent ASC still holds the zero that killed the last one.

Those two facts together are load-bearing, and nothing checks them. If the init GE ever fails to
run — a null `InitEffect`, a future code path that skips it, a designer clearing the reference —
the new pawn is **alive at zero health and can never die again**, because no *change* will follow.
Silent, permanent, and it would present as "that one player is unkillable."

**Fix: a loud log if Health is not positive after the init GE.** Not a behaviour change — a
tripwire on an invariant that currently has none.

## Finding 4 — no spawn protection

There is none. In a 4v4 arena with the engine's default spawn selection, being shot before you can
turn around is a matter of time. The GAS-shaped answer is a short duration GE granting a damage
immunity tag that `BNDamage` checks — but that is a **gameplay** decision (does BN want it? how
long? does firing cancel it?), not a correctness one, so it is named here and not built.

## Finding 5 — spawn selection is the engine's default

`RestartPlayer` → `AGameModeBase::ChoosePlayerStart` picks among unoccupied `APlayerStart`s. For
4v4 that eventually needs team ownership and enemy-proximity scoring. Deliberately deferred — the
existing comment says so, and it is right that a `ABNPlayerStart` holding nothing would be worse
than none.

## What multiplayer correctness looks like here, checked one by one

| Question | Answer |
|---|---|
| Does everything run on the authority? | Yes — `RequestRespawn` and `RespawnPlayer` are GameMode, which only exists on the server. |
| Does the client's ASC re-point at the new pawn? | Yes — `OnRep_PlayerState` → `InitializeAbilitySystem` → `InitAbilityActorInfo`. |
| Can a client cause a respawn? | No path. Death is ServerOnly and the GameMode is server-only. |
| Does a disconnect mid-delay break anything? | No — weak pointer. |
| Can two respawns race? | Not currently: `bDeathReported` fires once, and GAS refuses to re-activate an already-active instanced ability. Recorded because the timer handle is deliberately not stored, so if a second path ever calls `RequestRespawn` there is nothing to cancel. |
| Does the observing client see the respawn? | The new pawn replicates normally. **Never verified** — this is D6, and nothing in this project has been tested on two windows. |

## Recommended order

1. **Law 7 seam** — death broadcasts, GameMode listens. Unblocks killfeed/scoring later and
   decouples corpse lifetime from respawn delay.
2. **Input-flag clear** — two lines against an unverified engine assumption.
3. **Init tripwire** — a log on an invariant that has none.
4. *(founder's call)* **Spawn protection** — gameplay, not correctness.
5. *(later roadmap)* **Team spawn selection.**

1–3 are correctness and are built with this document. 4 and 5 are not.
