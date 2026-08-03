# RECEIPT — texture import · 2026-08-03T02:41:22.842408+00:00

**Source:** `Content/UI/Icons/Difficulty` → **`/Game/UI/Icons/Difficulty`** · 8 PNG(s)
**Settings applied:** `{"lODGroup": "TEXTUREGROUP_UI", "compressionSettings": "TC_EditorIcon", "mipGenSettings": "TMGS_NoMipmaps", "sRGB": true}`

- PASS `T_UI_Icon_Difficulty_Heroic_120` 480x480 — 4/4 settings verified
- PASS `T_UI_Icon_Difficulty_Heroic_40` 160x160 — 4/4 settings verified
- PASS `T_UI_Icon_Difficulty_Legendary_120` 480x480 — 4/4 settings verified
- PASS `T_UI_Icon_Difficulty_Legendary_40` 160x160 — 4/4 settings verified
- PASS `T_UI_Icon_Difficulty_Recruit_120` 480x480 — 4/4 settings verified
- PASS `T_UI_Icon_Difficulty_Recruit_40` 160x160 — 4/4 settings verified
- PASS `T_UI_Icon_Difficulty_Regular_120` 480x480 — 4/4 settings verified
- PASS `T_UI_Icon_Difficulty_Regular_40` 160x160 — 4/4 settings verified

- save_assets → True

## Verdict
8 imported and verified, 0 failed, of 8.

## Rung honesty
- **Not a rung.** An imported texture is not a rendered one. This proves the asset exists with the right settings; it does not prove it looks right in a widget.
- Settings are verified by read-back, not assumed — but only the four above. Anything else on the texture is engine default and unreviewed.
