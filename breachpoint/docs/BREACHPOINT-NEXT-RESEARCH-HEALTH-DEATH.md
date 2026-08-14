# Where the health component belongs, and how death should work

**Cut:** 13 August 2026 by the cloud lead, at the founder's request ·
**Question asked:** *"Is the health component in the correct place? It's been called directly with
the PlayerState… we might need to have it in the PlayerState, we might need to have it just in the
character. I don't know."*

**Scope fence, the founder's:** health and death only. **Respawn is deliberately not this
document's** — it already works and gets its own pass.

---

## The answer: it is in the right place, and for a reason worth writing down

| Lives on | What | Why there |
|---|---|---|
| **PlayerState** | `UBNAbilitySystemComponent`, `UBNAttributeSet` | It must **outlive the pawn**. Health, shield and every granted ability survive death because the thing holding them is not the thing that died. A pawn-side ASC would lose the player's whole state every respawn. |
| **Pawn** (`ABNCharacter`) | `UBNHealthComponent` | It watches **this body's** life. A new body gets a fresh watcher with a fresh `bDeathReported`, and the old one dies with the corpse — so nothing it registered can leak onto the persistent ASC. |

The split looks odd until you say what each object *is*: **the ASC is the player, the health
component is the body.** Health is a player-scoped number; *dying* is a body-scoped event.

### This is Lyra's pattern, not an invention

The project migrated from Lyra (`/Game/MigrateLyra/`, 826 assets), and Lyra answers this exact
question the same way: `ULyraHealthComponent` is a **Pawn** component on `ALyraCharacter`, while the
ASC sits on `ALyraPlayerState`. Its initialiser is
`InitializeWithAbilitySystem(ULyraAbilitySystemComponent*)` — the same shape BN already has. BN
arrived here independently and landed on the canonical answer.

### What moving it to PlayerState would cost

It is worth being concrete about the alternative rather than just rejecting it:

- **`bDeathReported` would have to be reset by hand** every respawn instead of dying with the body.
  A missed reset is a player who can never die again.
- **The ragdoll, the corpse, the mesh and the capsule are all pawn-side.** A PlayerState-side
  component would reach across to the pawn for every one of them, and that reach is null for the
  window between destroying the corpse and possessing the new body.
- **Nothing is gained.** The one thing PlayerState placement would buy — knowing "this player is
  dead" while no pawn exists — is already held by `State.Dead`, a GE-applied tag on the persistent
  ASC. That is the right home for it, and it is already there.

**Verdict: leave it. No change.** The founder's instinct to check was right; the answer happens to
be that it is already correct.

---

## What IS wrong: death is invisible to everyone but the server

`UBNHealthComponent::OnDeath` fires on **every machine** — that is deliberate and documented:
Health replicates, so each machine reaches zero on its own and reports it locally, rather than one
machine sending a "he died" flag. Good design.

But there is exactly **one** listener, `ABNCharacter::HandleDeath`, and its first line is:

```cpp
if (!HasAuthority()) { return; }
```

So the only consumer of a signal that fires everywhere throws it away everywhere but the server.
**There is currently no seam for a cosmetic death reaction at all.** Whatever we build — ragdoll,
death montage, death sound — would run on the server and nowhere else.

That is the real gap behind the founder's ragdoll question, and it is not "we forgot the ragdoll";
it is that the architecture has no place to put one.

### What the authority side already does correctly

`UBNGA_Death` (ServerOnly) is sound and is not the problem:

- cancels every other ability, so each one's `EndAbility` takes its own state GE off the persistent ASC
- applies `State.Dead`, which `UBNGameplayAbility::CanActivateAbility` checks **on clients too** —
  so a dead player's inputs are refused locally and never even mispredicted
- stops and disables movement (`MovementMode` replicates, so the corpse does not walk on the machine
  that used to steer it)
- ignores move/look input on the authority's controller copy
- asks the GameMode to respawn, and stays **deliberately active** because it is what holds the tag

And respawn closes the loop properly: `CancelAbilities()` ends the death ability (removing
`State.Dead`), then a single `RemoveActiveEffectsWithGrantedTags(State)` sweep clears anything the
old life left, then the init GE runs — all **before** the new pawn exists.

---

## The design: death presentation is a GameplayCue

Law 6 is unambiguous — *"cues carry all cosmetic consequences; no FX/audio/camera-shake spawned
directly in ability or actor code."* A ragdoll is a mesh state change, which is presentation, so the
same rule applies and it also happens to be the mechanism that solves the visibility problem for
free: **a cue executed from the authority multicasts to every machine.**

```
Health hits 0  (every machine reaches it independently — Health replicates)
      │
      ├─ every machine: UBNHealthComponent::OnDeath fires
      │        └─ ABNCharacter::HandleDeath — authority only from here
      │
      └─ AUTHORITY: TryActivateAbilityByClass(UBNGA_Death)
               ├─ CancelAbilities → each verb takes its own GE off the ASC
               ├─ State.Dead applied  → blocks input on EVERY machine
               ├─ movement stopped + disabled (MovementMode replicates)
               ├─ ExecuteGameplayCue(GameplayCue.Character.Death)   ← NEW
               │        └─ multicast → EVERY machine ragdolls its own copy
               └─ GameMode->RequestRespawn  (3s, then corpse destroyed)
```

**Why a one-shot `Executed` cue rather than a tag-linked `OnActive`/`WhileActive` one:** the corpse
lives ~3 seconds. A client joining inside that window would miss an Executed cue and see a
T-posed corpse for a moment — but it is about to be destroyed anyway, and a `WhileActive` cue would
have to re-ragdoll a body whose physics state it cannot know. The one-shot is the honest trade, and
it is recorded here so nobody rediscovers it as a bug.

**Why not just ragdoll inside `UBNGA_Death`:** it is ServerOnly. Nothing there runs on a client.

### The one judgement call worth flagging before it is built

BN's camera is attached to the **mesh's `head` socket** with `bUsePawnControlRotation = true`. When
the mesh starts simulating, the camera rides the ragdolling head. That is a death-cam some games
ship deliberately and others find nauseating. Building it the simple way first and letting the
founder judge in the playtest is cheaper than pre-solving it — but it is a *known* consequence, not
a surprise, and detaching the camera is a two-line change if it reads badly.

---

## What this does NOT cover, deliberately

- **Respawn.** Already works; the founder fenced it out; it gets its own pass.
- **Death montages.** `AM_MM_Death_Front_01` and five directional variants exist in FPSTemplate. A
  montage and a ragdoll fight each other — the usual answer is montage first, ragdoll on blend-out,
  which is a second pass and a bigger one. Ragdoll alone is the honest first step.
- **Killfeed, scoring, death cam framing.** Match-layer, not health-layer.
- **Hit reactions.** ~20 `AM_MM_HitReact_*` exist and nothing plays them. Same seam problem as
  death had, same eventual cure, but a separate packet.
