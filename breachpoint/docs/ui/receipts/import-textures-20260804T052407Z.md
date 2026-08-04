# RECEIPT — texture import · 2026-08-04T05:24:07.674835+00:00

**Source:** `Content/UI/Components/Buttons/Assets` → **`/Game/UI/Components/Buttons/Assets`** · 6 PNG(s)
**Settings applied:** `{"lODGroup": "TEXTUREGROUP_UI", "compressionSettings": "TC_EditorIcon", "mipGenSettings": "TMGS_NoMipmaps", "sRGB": true}`

- PASS `ButtonBorder_Default_Fade_Default` 400x400 — 4/4 settings verified
- PASS `ButtonBorder_Default_NoFade_Bonus` 400x448 — 4/4 settings verified
- PASS `ButtonBorder_Default_NoFade_Default` 400x400 — 4/4 settings verified
- PASS `ButtonBorder_Hover_Fade_Default` 416x408 — 4/4 settings verified
- PASS `ButtonBorder_Hover_NoFade_Bonus` 416x464 — 4/4 settings verified
- PASS `ButtonBorder_Hover_NoFade_Default` 416x416 — 4/4 settings verified

- save_assets → True

## Verdict
6 imported and verified, 0 failed, of 6.

## Rung honesty
- **Not a rung.** An imported texture is not a rendered one. This proves the asset exists with the right settings; it does not prove it looks right in a widget.
- Settings are verified by read-back, not assumed — but only the four above. Anything else on the texture is engine default and unreviewed.
