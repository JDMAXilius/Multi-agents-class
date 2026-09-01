# TICKET — BN43: menu art — our captures, their boxes

> STATUS: in-progress — 1 Sep 2026, terminal, live editor. The FE STAGE is built and the
> three map plates are captured, imported and wired. OWNER: **terminal**, LIVE EDITOR.
> **Rung 1 re-run after the C++ change: PASS BreachpointEditor + PASS Breachpoint;
> BreachpointServer refused by the launcher distribution (BN38). The running editor still
> holds the OLD dylib, so the plates cannot be seen until the editor is restarted.**
> DEPENDS ON BN42's loop walking. This is WAVE 3 — polish, sacrificed first if the
> deadline bites (BN-FRONTEND-PLAN.md's knife).

**The IP line (01-MENU-MEASURED.md §6, binding):** the Figma mocks embed 343-owned art —
Halo screenshots, Spartan renders, playlist key-art. NONE of it is exported. From Figma we
take NUMBERS and any panel geometry we authored; every image asset is generated from OUR
game.

## Do

1. **Map previews** (349×196.7 slot): high-res screenshot per roster map — BR_Spillway,
   BR_Arena01, BR_Aquarius — from a flattering angle, imported as `T_Preview_<Map>`,
   wired into the setup screen's Preview Photo (add a soft-path per map entry if needed —
   that is an INI field + one optional Image bind, cloud pre-approved).
2. **FE backdrop**: a Spillway vista camera in FE_MainMenu (the stage IS the background,
   zero texture cost) — or one captured still if the vista fights the widget contrast.
3. **News card** (349×222): one of the BN37 build screenshots + "NEW ARENA: SPILLWAY"
   overlay text from C++/asset — our news, really.
4. Contrast pass at the referee's boxes: Profile Bar band, panel fills vs backdrop —
   `BNUIColors` tokens only.

## Done when
- [~] Three previews landed from OUR renders via direct unreal-mcp calls (CaptureViewport →
      TextureTools.import_file → soft path in ini → SetBrushFromTexture in C++). Backdrop is
      the new FE stage, not a texture. **News card art: NOT done** — the panel still carries
      only its title string.
- [~] Stage screenshots sent to the founder at 1280x720. Side-by-side against the Figma
      frames not assembled.

## Log

### 1 Sep 2026 — the FE stage, and a plate per map

Founder brief, mid-session: *"lets setup a level for the main menu we should attend to have
just like this one to one image level design, player character with weapon on hand and
animation just for the main menu level setup"*, then *"character is looking front not to side,
enviroment should be propertly setup and one to one. light should be identical aswell, effects
and postvolume too"*, then *"you should use place holders images for each level selected on the
list"* and *"make sure the team death match can be 4v4, 8v8"*.

**THE IP LINE, stated once and then obeyed.** The reference frame is Halo Infinite's FE_Play.
§6 of `01-MENU-MEASURED.md` is binding: from Figma we take NUMBERS and geometry we authored;
every image asset is generated from OUR game. So the composition is matched 1:1 and **not one
pixel is exported from the mock** — no Spartan render, no playlist key-art, no Halo screenshot.
The hero is the UE mannequin, the weapon is our `SKM_Rifle`, the plates are our own arenas.

#### The stage — `FE_MainMenu`, additive, all through unreal-mcp

| actor | what |
|---|---|
| `FE_Hero` | `SkeletalMeshActor`, `SKM_Manny`, `animationMode=AnimationSingleNode`, `animationData.animToPlay = MM_Rifle_Idle_Hipfire`, `bSavedLooping/bSavedPlaying = true` — a real looping idle, not a frozen pose |
| `FE_Rifle` | `SkeletalMeshComponent` on the hero, `SKM_Rifle`, held diagonally across the chest |
| `FE_Camera` | `CameraActor`, `autoActivateForPlayer = Player0` — the front-end spectator adopts it with **no code change** |
| set | 7 blockout actors (floor, back wall, two platforms, a mid block, a pillar, a ledge) from `/Game/LevelPrototyping` |
| light | `FE_Sun` (directional, key), `FE_SkyLight`, `FE_RimSpot` (warm spot, separation) |
| `FE_PostProcess` | **unbound** volume: cinematic DOF f/1.4 focused at 215uu, exposure LOCKED (min=max=1.0, bias +1.15) so the menu never breathes, vignette 0.62, bloom 0.9, cool desaturated grade |

All 13 post-process values were written and **read back**; the readback is in the session
transcript. The volume is `bUnbound=true, priority=10`.

**A real defect the founder's "one to one" note caught, and its cause.** The first pass had the
hero in profile. Facing the actor at the camera was NOT enough: `MM_Rifle_Idle_Hipfire` twists
the torso roughly 35°, so an actor-yaw that points the *actor* at the camera points the *chest*
away from it. The stage counter-rotates (`yaw 22`) so the hero READS front-on. That number is a
compensation for the animation, not a measurement — if the idle is ever swapped it must be
re-tuned.

**Two engine lessons, both paid for:**

1. A second `DirectionalLight` for the rim printed *"Multiple directional lights are competing
   to be the single one used for forward shading"* **on screen, in game**. Replaced with a
   `SpotLight`. Never add a second directional for a rim.
2. Screen-right in this shot is **−Y**, not +Y (camera yaw ≈ 165°, so its right vector is
   ≈(−0.27, −0.96)). Two placement iterations were spent moving the rifle the wrong way before
   the axis was derived rather than guessed.

#### Preview plates — one per roster map, from our own renders

`CaptureViewport` at a hand-picked pose per map → centre-crop to 16:9 → `TextureTools.import_file`
into `/Game/BN/UI/Art`. All three imported at **698×393** (2× the referee's 349×196.7 slot).

| plate | source pose | note |
|---|---|---|
| `T_Preview_Spillway` | (5500,−3000,1800) | reads well first try — the tiers and the orange accents carry it |
| `T_Preview_Arena01` | (3800,500,900) | first attempt from outside was half black void; retaken from INSIDE the box |
| `T_Preview_Aquarius` | (6800,−1800,2400) | first attempt had the camera inside a wall; pulled back and up |

**Wiring (the ini field BN43 pre-approved):** `FBNFrontEndMapEntry` gains
`UPROPERTY(Config) TSoftObjectPtr<UTexture2D> PreviewTexture` — **soft, per law 3**, so a map's
art never gets dragged into every cook by a hard C++ reference. `BNScreen_PlaySetup` gains a
`BindWidgetOptional UImage* PreviewImage`, and `RefreshDisplay()` sets the brush and
un-collapses it as the MAP row cycles. An empty entry is legal and collapses the Image.
The load is `LoadSynchronous` **on purpose** — one 698×393 plate on a keypress; an async
request would land after the player had already cycled past the map it belongs to.

`bn41_selftest.py` after the change: **PASS 25 widgets 3/3 binds, PASS 32 widgets 10/10 binds**
(PreviewImage is the tenth, and the plan file now marks it bound).

#### TEAMS 4v4 and 8v8 — answered, with the gap named

Both already work in the menu: `Teams=1` makes the BOTS row halve the total, and
`PlayerCountPresets = {4, 8, 12, 16}` gives **2v2 / 4v4 / 6v6 / 8v8**. `HandleModeClicked`
resets the total to 8 on a mode change, so TDM opens at 4v4 and one BOTS press reaches 6v6,
two reaches 8v8. A TDM launch was observed this session:
`BR_Arena01 (listen?TargetPlayers=8?Teams=1)`.

**The real gap, counted not assumed:** every roster map ships exactly **eight** PlayerStarts —
`BR_Spillway` 8, `BR_Arena01` 8, `BR_Aquarius` 8 (read out of each level with
`find_actors(actor_type=PlayerStart)`). So a **16-player 8v8 lobby has twice as many players as
starts** and the GameMode will double up spawns. This is BN41's open 12/16-vs-8 finding,
now measured on all three maps. The fix is eight more starts per map, and **those maps belong
to other packets** (law 7), so it is not done here — it is recorded in `DefaultGame.ini`
beside the presets and belongs in a ticket of its own.

#### Honest state

- **Rung 1 re-run after the C++ change: PASS `BreachpointEditor`, PASS `Breachpoint`,
  `BreachpointServer` refused by the launcher distribution** (structural, BN38). The build ran
  with the editor open and produced `libUnrealEditor-BreachpointNext-0001.dylib`, so **the
  running editor is still on the old binaries: the plates cannot appear until it restarts.**
- The stage is a strong first pass, **not pixel-1:1** with the mock. What is genuinely still
  different: the set is grey blockout where the mock has built environment art; the hero is the
  UE mannequin; and **the rifle is positioned by a fixed relative transform, not attached to a
  hand socket** — the MCP toolset exposes `get_socket_names` but `attachSocketName` is not a
  settable property, so the weapon does not track the hand through the idle. That is the one
  thing a human in the editor can fix in seconds and the toolset cannot.
- News card art (BN43 step 3) is NOT done.


