# PACKET — GAS in the editor: assets hold the references, C++ holds the logic

**Cut:** 13 August 2026 by the cloud lead, from the founder's ruling ·
**Sequenced:** AFTER R3 G1 (aim) is founder-confirmed. This is the design, not the execution.

## The founder's ruling, in their words

> *"if you're gonna be doing gameplay queues, so GA, so gameplay effect — try to have that
> represented in the editor as well … the access should be from the code soft references, and then
> in the editor you have the references correctly from the editor. That way it's easy for us to
> change it right away, so we don't have any hardcoded reference path."*

Plus the fence, and it governs every step below:

> *"Right now it's working. So we don't want to really change something that will break it."*

Scope confirmed by the founder: **all four** — cues, abilities, a weapon FX DataAsset, and GEs.

**This supersedes ASSET-RULES §3**, which said abilities/cues/GEs have no asset at all. §3 was right
that they are C++ *classes*; it was wrong to conclude they get no editor presence. Amend §3 when
this lands rather than leaving two rules disagreeing.

---

## The trap, found while designing this — read before touching a cue

`UBNGameplayCueRegistrar::OnWorldBeginPlay` builds the runtime cue set like this:

```cpp
GetDerivedClasses(UBNGameplayCue_Base::StaticClass(), HandlerClasses, /*bRecursive=*/true);
… CuesToAdd.Emplace(Handled, FSoftObjectPath(HandlerClass));
RuntimeSet->AddCues(CuesToAdd);
```

**A Blueprint child of `UBNGameplayCue_MuzzleFlash` is a derived class.** It inherits
`GetHandledCueTag()`, so it registers under the *same tag as its C++ parent*. `UGameplayCueSet`
keys by tag and **replaces** on a duplicate — so which handler survives depends on the order
`GetDerivedClasses` happens to return, which is not guaranteed.

That is exactly the "don't break what's working" failure: the muzzle flash would fire from the C++
CDO's ini values on one run and the Blueprint's values on the next, with nothing in the log to say
which. **Making BP cue children without fixing the registrar first is the single riskiest step in
this packet.**

### The fix — most-derived wins, per tag

Before any BP cue child exists, change the registrar to keep, for each tag, the class that no other
registered class derives from. Roughly:

```cpp
// A class that another registered handler derives FROM is a base, not the handler: the BP child
// holding the editor's asset references is what should answer the tag. With no BP children this
// selects exactly the same leaf classes as today, so it is behaviour-preserving on landing.
HandlerClasses.RemoveAll([&HandlerClasses](UClass* Candidate){
    return HandlerClasses.ContainsByPredicate([Candidate](UClass* Other){
        return Other != Candidate && Other->IsChildOf(Candidate);
    });
});
```

Then **log the winner per tag**, so a future ambiguity is visible instead of silent.

**Land this step alone, first, and confirm the FX still work.** With zero BP children it selects the
same classes as today — that is what makes it safe to land ahead of the rest.

---

## Step 2 — the four cue Blueprints

One BP child per cue class, in `/Game/BN/AbilitySystem/Cues/`, holding **only** asset references —
empty graphs, no new members (R26).

| BP | Parent | Holds |
|---|---|---|
| `BP_BNCue_MuzzleFlash` | `UBNGameplayCue_MuzzleFlash` | Effect, Sound |
| `BP_BNCue_Impact` | `UBNGameplayCue_Impact` | Effect, Decal, SurfaceRows |
| `BP_BNCue_Tracer` | `UBNGameplayCue_Tracer` | Effect |
| `BP_BNCue_Explosion` | `UBNGameplayCue_Explosion` | Effect, Sound |

The `Config` specifier stays on the C++ properties. It becomes the fallback: a BP child that leaves
a field empty gets the ini value. **Delete the ini lines only after the BPs are confirmed working**
— keeping both through one playtest is what makes this reversible.

## Step 3 — abilities

`FBNAbilitySetAbility` already carries `TSubclassOf<UGameplayAbility> Ability` + `InputTag`, so a
`DA_BNAbilitySet` can point at a BP child **today, with no code change.**

**But there is a catch worth stating plainly:** eight abilities are granted from C++ class literals
in `ABNPlayerState::GrantDefaults` — `UBNGA_Jump::StaticClass()`, `…Melee`, `…Grenade`, and the
rest. Those grants **ignore any BP child**, because they name the C++ class directly. Using BP
children for abilities therefore means moving those grants into `DefaultAbilitySet`, and the
fallback path in `GrantDefaults` exists precisely because that asset is not configured yet.

So this step is really: **populate `DefaultAbilitySet`, then delete the fallback.** That is a
genuine improvement — the fallback is a second source of truth for what a player is granted — but
it is a behaviour change, not a reference swap. Do it after cues, on its own, with a playtest.

Death stays granted from C++ and stays out of the set: it has no input tag and is activated by
class from `UBNHealthComponent`.

## Step 4 — the weapon FX DataAsset

`UBNWeaponFXSet` (a `UPrimaryDataAsset`): muzzle FX, fire sound, tracer FX, per-weapon impact
overrides. One soft ref column on `FBNWeaponRow` points at it; the cues read it off the firing
weapon exactly as `FireSound` already does, falling back to the cue's own value.

This is the piece that most directly answers *"easy for us to change it right away"* — swapping the
rifle's muzzle flash becomes one field on one asset, and the pistol can differ from the rifle
without a new cue class.

`FireSound` on the row is the prototype of this pattern and folds into the set when it lands.

## Step 5 — GEs

`UBNGE_Damage`, `UBNGE_InitAttributes` and the cooldown GEs get BP children only where a **number**
needs to be editor-tunable. Note this cuts against law 3 (*"tuning numbers live in data tables"*),
so the honest split is:

- **Damage magnitudes** stay in `DT_BNWeapons`. They are already data and already editable.
- **GE structure** (duration policy, modifier ops, granted tags) is what a BP child would expose,
  and none of that is currently something the founder needs to change.

**Recommendation: do steps 1, 2 and 4 — skip 3 and 5 unless a specific need appears.** Steps 3 and
5 move behaviour, not references, and the founder's stated pain was references. If the answer is
"do all four anyway," step 3 is the one that needs its own playtest.

---

## Order, and why

1. **Registrar fix** — alone, confirm FX unchanged. Behaviour-preserving by construction.
2. **Cue BPs** — the actual pain, and now safe.
3. **Weapon FX DataAsset** — the biggest quality-of-life win.
4. *(optional)* Ability set population + fallback deletion — behaviour change, own playtest.
5. *(optional)* GE BP children — only if a specific number needs to move.

Each step is independently revertable, and none of them start until aim is confirmed. Nothing in
here is written yet — this is the design.
