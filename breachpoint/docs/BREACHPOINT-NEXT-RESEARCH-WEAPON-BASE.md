# The weapon base class — three references, one verdict

**Cut:** 13 August 2026 by the cloud lead, at the founder's request: *"research into the weapon
base and ultra thinking. Let's create a weapon base class."*

## 0. The verdict up front

**BN already has its weapon base — `ABNWeapon` — and the right move is to FORMALIZE it, not to
grow a hierarchy under it.** This packet makes it a true base: subclassable *through data* when a
weapon someday needs behavior, complete where it is honestly incomplete (ammo has no reserve —
reload today refills the magazine from nothing), and seamed where a subclass would genuinely hook
in. Zero subclasses are created, deliberately.

## 1. The three references, and what each one teaches

| Reference | Shape | The lesson |
|---|---|---|
| **Old module `ABPWeaponBase`** | An actor base that is deliberately EMPTY — it exists so C++ and Blueprint pins agree, and its own comment warns that adding any member breaks all five weapon assets by name-shadowing | What a base is FOR is a contract point, not a feature bucket. Also: BN has no BP weapon children, so BN does not need this class's reason to exist |
| **Old module `UBRWeaponInstance`** | A replicated UObject, not an actor: row handle + `AmmoInMag` + `AmmoReserve` + `CalcReloadTransfer` (a static, testable transfer function) + an ammo-changed delegate | The ammo model. Mag + reserve with a pure transfer function is the proven shape, and BN's weapon currently has HALF of it |
| **Template `BP_FPST_Weapon_*`** | Four Blueprint children of one base, each carrying per-weapon VALUES (sockets, montages, fire mode) and no per-weapon logic | Even the template's "hierarchy" is data wearing class syntax. BN already moved those values into `FBNWeaponRow`, which is the same idea done properly |

And the standing law that fences the whole design: **abilities are the only action entry** (purity
law 2). Fire, reload, melee, ADS all live in GAs reading the row. The moment a weapon subclass
grows a `Fire()` method, there are two entry points and one of them is illegal. Lyra reaches the
same conclusion from the other direction — its weapon instances hold data and cosmetic state while
equipment abilities do the acting.

## 2. So what does "a weapon base class" mean here?

Not `ABNWeapon_Rifle / _Shotgun / _Knife` — that hierarchy would hold nothing but values the row
already holds, and it is precisely the overengineering the founder keeps fencing against. The
differences between BN's four weapons are ALL data today: numbers, sockets, montages, sounds, an
ability set, `bCanADS`. A subclass with no behavior is a row with extra steps.

What a BASE genuinely owes, and what this packet adds:

### 2a. Subclassable through data — `FBNWeaponRow.WeaponClass`

A soft class pointer on the row, defaulting to none = plain `ABNWeapon`. The equipment component
now resolves the ROW FIRST (which also kills the meshless-ghost spawn for missing rows at the
source, replacing the spawn-then-destroy skip) and spawns the row's class. The day an energy
weapon needs a charge state or the grapple needs an attached cable actor, its subclass is one C++
class plus one table cell — **the hierarchy exists the moment it earns existence, and not one day
before.**

### 2b. Complete ammo — the reserve `UBRWeaponInstance` proved

Today `Reload()` sets `CurrentAmmo = MagazineSize` — infinite ammo, silently. The base gains
`AmmoReserve` (replicated, authority-written like `CurrentAmmo`) fed from a new row field
`ReserveAmmo`, and reload becomes the reference's transfer: `min(MagSize − InMag, Reserve)` moves
from reserve to magazine — partial reloads work, an empty reserve means an empty click, and the
math is one static function (`CalcReloadTransfer`) exactly as the old module kept it, because a
pure function is checkable by eye and by test. Knife: `MagazineSize 0, ReserveAmmo 0` — nothing
changes for it.

### 2c. The seams a subclass would actually use

- `GetMuzzleTransform()` becomes **virtual** — a beam weapon with a moving emitter overrides one
  function, not the cue system.
- `OnEquipped()` / `OnUnequipped()` — empty virtuals called from `ApplyCurrentWeapon`, which
  already runs on EVERY machine (authority + both OnReps), so a subclass's cosmetic reaction to
  being drawn or holstered is multiplayer-correct by construction with zero replication of its own.

### What stays exactly where it is

Identity = `RowName` (the whole replicated identity, looked up per machine, never sent twice) ·
behavior = GAs · variation = rows · FX = cues · ammo authority = server-only writes. None of that
moves; the base is the thing those systems meet at.

## 3. Not built, with triggers

| Deferred | Trigger |
|---|---|
| Any actual subclass | The first weapon whose BEHAVIOR (not values) differs — projectile weapons, charge weapons, the grapple |
| Ammo-changed delegate (`UBRWeaponInstance` has one) | The HUD's ammo counter — its first consumer |
| Ammo pickup / resupply API | Pickups existing |
| Weapon instance as UObject (Lyra/`UBRWeaponInstance` style) vs actor | Never, most likely: BN's weapon IS visible in hands and socket-attached, so the actor is earning its keep. Recorded because someone will ask |
