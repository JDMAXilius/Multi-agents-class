# `Content/UI/Art/`

Sourced or rendered art that is genuinely a picture: ~30 rank + medal marks, ~12 scene
plates / backgrounds, 3 weapon silhouettes (`docs/ui/ue-frontend/ASSET-PIPELINE.md:23-31`).

**Naming** (`ASSET-PIPELINE.md:81-82`):

| Thing | Name |
|---|---|
| Rank mark | `T_UI_Rank_<Name>` |
| Medal | `T_UI_Medal_<Name>` |
| Scene plate / background | `T_UI_Plate_<Screen>` |
| Weapon silhouette | `T_UI_Weapon_<Name>` (rendered from the mesh, `WEAPON-RENDER-PLAN.md`) |

**Never here:**
- **Icons.** They go in `Content/UI/Icons/` — that path is a hard-coded licence gate in
  `Tools/verify_notices.py:50`, and an icon parked here escapes it.
- Anything UMG can draw. A texture for a divider, a panel background or a solid fill is the
  defect `ASSET-PIPELINE.md:15` exists to prevent: unreviewable, does not recolour with the
  palette, does not scale.
- Text as an image. Ever.
- Gradients / scanlines / glows — those are materials, `Content/UI/Materials/`.
- Third-party art without a notice in `THIRD-PARTY-NOTICES.md`.
