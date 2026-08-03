# TICKET — BP75: Every reticle soft path resolves to nothing, and the failed load blanks the brush

> STATUS: open — `contract_gap`, cut 3 Aug 2026 while planning BP69. **The centre of the screen
> is empty for every weapon, every match.** Verified by reading the code path end to end, not
> inferred from the path strings.

`Source/Breachpoint/UI/HUD/BRReticleWidget.cpp:44-48` registers five soft paths shaped
`/Game/UI/HUD/Reticles/T_Reticle_<Id>.T_Reticle_<Id>`. Neither the folder nor the prefix exists:

```
$ ls -d Content/UI/HUD/Reticles
ls: Content/UI/HUD/Reticles: No such file or directory

$ ls Content/UI/HUD/ | grep -i reticle
HUD_Reticle_AR.uasset  HUD_Reticle_BR.uasset  HUD_Reticle_Magnum.uasset
HUD_Reticle_EnemyState.uasset
```

Wrong folder AND wrong prefix. Two of the five ids (`Shotgun`, `Sniper`) have no texture under
any name.

## Why it blanks the screen rather than falling back

This is the load-bearing half, and it is why the WBP's own art does not save it.
`ApplyArt` (`:340`) guards a null *path*, never a failed *load*:

```cpp
if (SoftArt.IsNull())        { Image->SetBrushResourceObject(nullptr); return; }
UTexture2D* Loaded = SoftArt.Get();
if (!Loaded) { Loaded = SoftArt.LoadSynchronous(); }   // still nullptr - path resolves to nothing
Image->SetBrushResourceObject(Loaded);                 // <-- unconditional
```

A non-null path that resolves to nothing falls straight through to
`SetBrushResourceObject(nullptr)`. `WBP_ReticleWidget` ships a correct baked-in brush —
`gen-ui-20260803T212916Z.md` records `ReticleImage` with
`ResourceObject="/Game/UI/HUD/HUD_Reticle_AR"` — and `RefreshReticle` **overwrites** it with
null on the first weapon equip. The asset is right; the code erases it.

**Failure, concretely:** equip the AR → `SetActiveWeaponId("AR")` → `ReticleByWeaponId.Find("AR")`
HITS → `ApplyArt` resolves nothing → brush cleared → empty centre screen. Equip the Rocket →
`Find` misses → stand-in `"AR"` → identical empty result, plus the BP69 fallback warning firing
to say it stood in with art that also did not load.

**Why it hid:** the constructor comment calls these paths "a coordination point with the art
pipeline, not a promise the .uassets exist yet" — true when written, false since the textures
landed under different names. A `TSoftObjectPtr` literal is invisible to the compiler by design,
and `--verify` checks widget *trees* against the plan, not soft paths against disk. Both green.

## Kickoff (machine-checkable)

- requires: **files-only** to fix; **engine-installed** (BP71 green) to verify
- `ls Content/UI/HUD/HUD_Reticle_AR.uasset` exists; `ls Content/UI/HUD/Reticles/` does not
- `grep -c "T_Reticle_" Source/Breachpoint/UI/HUD/BRReticleWidget.cpp` returns 5
- owner_path: `Source/Breachpoint/UI/`

## Steps (in order)

1. Decide direction ONCE: rename assets to the C++ paths, or correct the C++ defaults to the
   landed names. **Prefer correcting C++** — the assets are imported, referenced by the WBP's
   baked brush, and named `HUD_Reticle_*` in `Tools/gen_ui/preflight_textures.py`'s size table.
   Moving them breaks the generator and the WBP together.
2. `Shotgun` and `Sniper` have no texture at all. Per BP69's bucketing they are post-slice
   weapons — drop the two map entries rather than inventing art, and let the stand-in own them.
3. Same bug, same constructor: `HitMarkerArtByKind` (`:57-60`) uses the identical broken shape
   and three of its four assets do not exist. Fix the path form here; BP74 #5 owns the producer.
4. **Add the check that would have caught it.** A spec that resolves every entry in
   `ReticleByWeaponId` and `HitMarkerArtByKind` and asserts the package exists. A soft path is
   invisible to the compiler; a spec is the only thing that can see it.
5. Consider making `ApplyArt` refuse to clear a brush on a *failed* load — erasing correct
   authored art because a config path is wrong is the wrong default.

## Done when

- [ ] Every `ReticleByWeaponId` and `HitMarkerArtByKind` path resolves to a package on disk
- [ ] A spec fails if a future entry does not
- [ ] PIE (rung 2): a reticle is visible at screen centre, per weapon, with a screenshot

## Log

**3 Aug 2026 — cut.** Found by the BP69 planning pass, then verified independently: the path
mismatch was confirmed by `ls`, and the blanking behaviour by reading `ApplyArt` to its last
line. The agent's original report claimed the empty screen without accounting for the WBP's
baked-in brush; the brush is real, and `SetBrushResourceObject(nullptr)` overwrites it anyway,
so the conclusion held for a reason the first pass had not established.

Note this makes BP69's box 3 ("the Rocket reticle question is answered") vacuous until it lands:
the stand-in decision is sound, and the thing it stands in with draws nothing.
