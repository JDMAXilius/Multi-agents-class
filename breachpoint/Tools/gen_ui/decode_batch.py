#!/usr/bin/env python3
"""Decode a batch of base64 PNGs exported from Figma into Content/UI/Icons/<family>/.

    python3 Tools/gen_ui/decode_batch.py batch.json

batch.json: [{"family": "Ranks", "name": "T_UI_Rank_01_Recruit", "b64": "..."}]

Exports MUST come from the Figma Plugin API `exportAsync`, not `download_assets`:
download_assets composites the node against the page backdrop, so every icon arrives
with a baked-in background (corner alpha 255) and is unusable. Verified 2 Aug 2026 —
41 assets were exported that way and discarded. exportAsync renders in isolation.
"""
import base64, json, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ICONS = REPO / "Content/UI/Icons"

batch = json.load(open(sys.argv[1]))
for a in batch:
    d = ICONS / a["family"]
    d.mkdir(parents=True, exist_ok=True)
    (d / f"{a['name']}.png").write_bytes(base64.b64decode(a["b64"]))
    print(f"  wrote {a['family']}/{a['name']}.png")
print(f"{len(batch)} file(s)")
