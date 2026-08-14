# TICKET — input wiring: melee, grenade, and the take-damage test key

**Cut:** 13 August 2026 by the cloud lead · **For:** the terminal session (editor + Unreal MCP)
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) first** — §1b (search order), §5 (do only
what is listed), §7 (the standing rule this executes).
**Supersedes §1 of** [`BREACHPOINT-NEXT-TASK-R3-W3-MELEE-GRENADE.md`](BREACHPOINT-NEXT-TASK-R3-W3-MELEE-GRENADE.md)
— the input half of that handoff lands here instead, consolidated with the new test key. Its §2
(the `DT_BNWeapons` cells) still stands over there.

**Owner path:** `Content/BN/Input/`. **Never `Source/`.**

## Why the third key exists

The founder asked for a **take-damage button** to test the health→death→kill-credit→ragdoll→respawn
chain that just landed. The C++ is already complete and waiting: `Input.Debug.DamageSelf` is bound
in `SetupInputComponent` (shipping-guarded) and routes through the SAME `BNDamageSelf` exec the
console uses — one press, one authority hop, damage through the one door. The binding has logged
*"that control is dead"* on every run since R2 because these three edits were announced and never
made. This ticket is that announcement being paid.

## Step 1 — three InputActions, created in `/Game/BN/Input/`

FPSTemplate ships no melee, grenade or debug InputAction (verified — its `Input/Actions/` has only
Aim, Crouch, Jump, Look, Move, Sprint, Weapon_Fire, Weapon_Reload), so per ASSET-RULES §4 these are
BN-owned. All three are **Digital (bool)** — same shape as the existing `IA_BN_LeanLeft`.

| Create | Value type |
|---|---|
| `IA_BN_Melee` | Digital (bool) |
| `IA_BN_Grenade` | Digital (bool) |
| `IA_BN_DebugDamageSelf` | Digital (bool) |

## Step 2 — FOUR rows in `DA_BNInput`

*(Row 4 added 13 Aug with the ADS packet — its InputAction is the TEMPLATE's `IA_FPST_Aim`, reused
in place per §4; create nothing for it. The `Input.Weapon.ADS` tag lands with the ADS C++ — if the
row will not save because the tag does not exist yet, the build is stale: stop and report.)*

| Tag (exact) | InputAction |
|---|---|
| `Input.Melee` | `IA_BN_Melee` |
| `Input.Grenade` | `IA_BN_Grenade` |
| `Input.Debug.DamageSelf` | `IA_BN_DebugDamageSelf` |
| `Input.Weapon.ADS` | `/Game/FPSTemplate/Input/Actions/IA_FPST_Aim` — **existing template asset** |

The tag strings are keys — `UBNInputConfig` resolves them literally. A typo is a silent dead
control, which is what this ticket exists to end, not extend.

## Step 3 — FOUR mappings in `IMC_BNNext`

| InputAction | Key | Why this key |
|---|---|---|
| `IA_BN_Melee` | **V** | the reference used F; V avoids the crouch row |
| `IA_BN_Grenade` | **G** | the reference's own key (`MyCharacter.cpp:856`) |
| `IA_BN_DebugDamageSelf` | **K** | unused by anything; mnemonic enough for a debug key |
| `IA_FPST_Aim` | **Right Mouse Button** | the genre's key, and the reference's |

Taken keys, do not collide: V, G, Q/E (lean), R (reload), Space, C/Ctrl (crouch), Shift (sprint),
mouse buttons and wheel (fire/swap).

## Step 4 — read back

Reload all three assets fresh; print intent vs actual for every row and mapping. Then PIE and
confirm the *"that control is dead"* line is GONE from the log for all four tags — that line's
absence is this ticket's acceptance test.

## Scope

Three assets created (ADS reuses a fourth, existing one), four rows, four mappings, the read-back. Nothing else — no `Source/`, no
`DT_BNWeapons` (that is the R3-W3 doc's §2), no other asset touched.

## Log

_(terminal: read-back table + the confirmation that the dead-control lines are gone)_

---

## For the founder — the test script this key unlocks, once built

Everything below is one PIE session, no console typing needed (though `BNDamageSelf 90`,
`BNKillSelf`, `BNRefill` still work from any window):

1. **Press K** — log prints `BNDamage: <you> -> <you>, 25.0 | shield 0 -> 0 | health 100 -> 75`.
   Shields are off, so the number lands straight on health. That line is the pipeline's receipt:
   instigator, victim, amount.
2. **K three more times** — at zero: `BNGameMode: <you> eliminated themselves.` (the self-kill
   wording — instigator == victim, exactly the edge case the kill credit decided up front), the
   body **ragdolls** on every machine, input dies (`State.Dead` refuses abilities client-side too).
3. **Wait 3 seconds** — respawn at 100, weapons re-granted, input back.
4. **The two-window version is the real test of kill credit:** listen server + client, shoot the
   other player dead — the server log must print `<killer> eliminated <victim>` with the actual
   names, and the ragdoll must appear on BOTH windows. That is the line that proves the instigator
   now survives to the death instead of dying at the log line.
