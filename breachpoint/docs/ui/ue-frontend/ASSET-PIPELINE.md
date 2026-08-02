# Figma → Unreal: the asset pipeline

**Status:** v1, 2 Aug 2026. The missing half of `docs/ui/ue-frontend/`. The other files answer
*what to build* (`SCREEN-MANIFEST.md`), *in what order* (`ROADMAP.md`), *how it moves*
(`MOTION-INTERACTION.md`) and *where the work runs* (`TERMINAL-VS-EDITOR.md`). **This one answers
how a thing in Figma becomes a thing in Unreal** — export settings, import settings, and which
kind of UE object each Figma layer should become.

Binding law: `CLAUDE.md` laws 3/4/7, `R18`/`R26`. Method: `ui-presentation` skill.

---

## 1. The rule that keeps this small

> **Export nothing that UMG can draw.**

Our design language is flat panels, sharp corners (radius 0), 1px/0.5px/2px strokes, solid fills
and uppercase text (`SCREEN-MANIFEST.md` §7.4). **UMG draws all of that natively.** A panel is a
`Border` with a solid brush. A rule is an `Image` 1px tall. A tab is a `Border` + `TextBlock`.

So the exported asset count for the whole front end is roughly:

| Category | Count | Why it must be an asset |
|---|---|---|
| Icons / glyphs | ~98 | Shapes UMG cannot draw |
| Rank + medal marks | ~30 | Art |
| Scene plates / backgrounds | ~12 | Photography or rendered art |
| Weapon silhouettes | 3 | Rendered from the meshes (`WEAPON-RENDER-PLAN.md`) |
| Materials | ~4 | Gradients, scanlines, radial fills |
| Fonts | 1–2 | Type |

**Everything else is zero assets.** If a build ends up with a texture for a divider or a panel
background, that is the defect this rule exists to prevent — it is unreviewable, it does not
recolour with the palette, and it does not scale.

---

## 2. Decision table — what each Figma layer becomes

Read the Figma layer, pick the row, stop. This is the whole routing decision.

| In Figma | In Unreal | Notes |
|---|---|---|
| Solid rect, any size | `Border` / `Image`, solid brush, colour from `UBRUISettings` | No asset. No texture. |
| 1px / 0.5px / 2px rule | `Image`, solid brush, fixed height | No asset |
| Panel with a border | `Border`, solid brush + `Outline` settings | No asset |
| Panel that **stretches** and has decorated corners | Texture → **`Draw As: Box`** brush + `Margin` | Nine-slice. §5 |
| Icon / glyph | **PNG** → `Image` | §3, §4 |
| Rank, medal, emblem | **PNG** → `Image` | §3, §4 |
| Gradient, scanline, glow, radial | **Material** → `Image` with material brush | Cheaper and recolourable vs a texture |
| Photo / render / scene plate | **PNG** (or JPG if huge and opaque) | §4 |
| Text | `TextBlock` / `CommonTextBlock` + a **CommonTextStyle** | Never an image of text |
| Auto-layout frame | `HorizontalBox` / `VerticalBox` | §6 |
| Absolute-positioned group | `CanvasPanel` | Use sparingly — §6 |
| Component / instance | **A WBP** parented to its `UBR*` C++ class | The component inventory |

---

## 3. Exporting from Figma

**Format: PNG.** Not SVG. UE 5.8 has no production-ready native SVG path — SVG/SDF import needs a
marketplace plugin, and adding a plugin dependency to save a few kilobytes is the kind of
complexity that costs more than it returns at our size. **Revisit only if icon crispness at 4K
becomes a real complaint.**

**Scale: 2× (i.e. `@2x`), always.** Our design base is 1280×720 and the game renders at 1920×1080
and above. Exporting at 2× of the 1280 base gives enough pixels for 1080p and headroom for 1440p.

**Rules:**
- **Transparent background.** No baked-in panel colour behind an icon.
- **Export the icon, not its frame.** Trim to the visible ink; padding belongs to the UMG layout,
  not to the texture. A padded texture is padding you cannot change without re-exporting.
- **White / neutral ink.** Icons ship white and are **tinted in UMG** from the palette. One
  texture serves every state and every colour. This is the single biggest asset-count saving —
  do not export a cyan copy and an amber copy of the same glyph.
- **Power-of-two is not required** for UI textures, but keep sizes tidy (48, 64, 80, 96).

**Naming — this is also the import path:**

```
T_UI_<Family>_<Name>       T_UI_Icon_Grapple, T_UI_Rank_Sergeant, T_UI_Medal_DoubleKill
T_UI_Plate_<Screen>        T_UI_Plate_MainMenu
M_UI_<Effect>              M_UI_Scanline, M_UI_GradientFade
```

---

## 4. Importing into Unreal — the settings that matter

Drop the PNG into `Content/UI/Textures/<Family>/`. Then set **four** things. Defaults are wrong
for UI and the failure is subtle: blurry icons and a texture budget three times bigger than it
needs to be.

| Setting | Value | Why |
|---|---|---|
| **Texture Group** | `UI` | UI textures must not be downscaled by texture-quality scalability. The default `World` group means low-spec players get a blurry HUD. |
| **Compression Settings** | `UserInterface2D (RGBA)` | Preserves alpha edges. The default `DXT` blocks-compresses and produces visible fringing on 1px strokes and small glyphs. |
| **Mip Gen Settings** | `NoMipmaps` | UI is drawn at ~1:1; mips waste memory and soften at some resolutions. |
| **sRGB** | ✅ on for colour, ❌ off for masks/data | A mask texture read as sRGB gives wrong values. |

> **Scripted import.** These four are set identically on every UI texture, so this belongs in
> `Tools/` as a `-run=pythonscript` pass (law 7: generated by a committed script, not clicked).
> `unreal.AssetImportTask` + `unreal.TextureFactory`, then set `lod_group`,
> `compression_settings`, `mip_gen_settings`, `srgb`. Terminal work, editor closed.

---

## 5. Nine-slice — the only tricky brush, and it is not very tricky

For any panel art that must **stretch** (a bordered container whose width varies by screen).

1. Export the panel art at its **smallest** valid size — corners plus a 1–2px stretchable middle.
2. In the widget's `Brush`, set **`Draw As: Box`**.
3. Set **`Margin`** to the corner insets as a **0–1 fraction of the texture**, not pixels. A 64px
   texture with 16px corners → margin `0.25` on each side.
4. Leave the *widget* free to resize. The brush stretches only the middle; corners stay crisp.

`Draw As: Border` is the same idea but leaves the centre empty — right for a frame drawn over
content that must show through.

**Most of our panels do not need this**, because they are solid fills with a 1px outline that a
`Border` draws natively. Reach for nine-slice only when the art has decorated corners.

---

## 6. Layout: Box first, Canvas last

`SCREEN-MANIFEST.md` §7.1 rules this and it is worth restating because it is the difference
between a UI that survives a resolution change and one that does not.

- **`VerticalBox` / `HorizontalBox`** for anything with a rhythm — menu rows, roster rows, tab
  bars. Figma auto-layout maps to these directly, and `Padding` + `Size: Fill` reproduces
  Figma's gap and grow behaviour.
- **`Overlay`** to stack things (art behind text).
- **`CanvasPanel`** only for genuinely free positioning — the HUD's four anchors, a screen's
  top-level regions. **Canvas children need anchors set deliberately**; a canvas child anchored
  top-left with a fixed offset will drift on every other aspect ratio.
- **`SizeBox`** to pin an exact measured dimension from the manifest.

**The 1280 → 1920 conversion is the DPI curve, not hand maths** (`SCREEN-MANIFEST.md` §7.2).
Author at the measured 1280 numbers and let the DPI scale rule handle the rest.

---

## 7. The reuse backbone — CommonUI style assets

This is the answer to *"as small as possible, modular and scalable."* **Three data assets carry
the entire look**, and every widget references them instead of styling itself.

| Asset | Type | Holds |
|---|---|---|
| `CTS_BR_<Role>` | `CommonTextStyle` | Font, size, colour, letter-spacing (150 = 15%), casing. One per type role: `Tab`, `MenuRow`, `Body`, `Numeral`, `Caption` |
| `CBS_BR_<Role>` | `CommonButtonStyle` | Normal / Hovered / Pressed / Disabled / Selected brushes **and** the text style each state uses |
| `CBRS_BR_<Role>` | `CommonBorderStyle` | Panel fills and outlines |

**Why this is the whole modularity story:** the idle→hover inversion (§7.4: fill goes white, text
goes black, bottom line 0.3→1.0) is authored **once** in `CBS_BR_MenuRow`. Every menu row on
every screen inherits it. Change the hover treatment in one asset and 18 screens update.

**The rule that keeps it honest:** a WBP may reference a style asset. It may **not** set colours,
fonts or brushes locally. A locally-styled widget is a widget that will not follow a restyle, and
it is invisible to review because a `.uasset` does not diff.

Styles live in `Content/UI/Styles/`. They are Tier-4 assets holding *appearance only* — no logic,
no numbers the sim reads, R26-clean.

---

## 8. Animation

`MOTION-INTERACTION.md` specifies the nine named transitions. Mechanically:

- **UMG widget animations** are the vehicle, authored in the WBP's Animations panel. Tier 4
  permits this, with one hard limit from §7.4: **a widget animation may animate appearance only.**
  An animation that gates on gameplay state is a branch in an asset — R18's target wearing a
  costume.
- **Play them from C++**, not from a graph: `PlayAnimation(...)` on a `BindWidgetAnim` property.
  The WBP owns the curve; the C++ class owns *when*.
- **CommonUI transitions** handle push/pop between activatable widgets — do not hand-animate a
  screen change that the activatable stack already does.
- Nine animations authored once on the shared components, not per screen.

---

## 9. The loop

1. **Figma** — component is final, geometry measured.
2. **C++ class first** (editor closed, then build). The class declares `BindWidget` slots and
   every property the layout needs. `ui-presentation` §8.
3. **Export** only what §2 says is an asset. **Import** with §4's four settings.
4. **WBP** parented to the C++ class. Layout, anchors, animation only. Styles referenced from
   §7, never set locally.
5. **Bind** through the ViewModel. A missing field is a **C++ gap to file**, not something to
   work around in the widget.
6. **Look at it** — PIE, and a screenshot in the ticket Log.

**Wave 1 is the whole spine** (`SCREEN-MANIFEST.md` §4.1). Build the 8 missing components first;
the dependency graph in §5 exists because `UBRNavBar` alone blocks 18 screens. Component before
screen, always — that is the Eric Dies lesson and it is the reason this stays small.
