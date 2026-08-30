#!/usr/bin/env python3
"""BN29 — trace the founder-supplied Aquarius top-down into a class grid.

    python3 Tools/blockout/trace_aquarius.py          # emit trace + preview
    
Reads docs/design/reference/aquarius_thegamescabin.jpg (the article's own
figure source, supplied by the founder 29 Aug — precision channel (a)) and
classifies every pixel into the image's four structural tones + background:

    BLACK (~26)  DARK (~65)  MID (~102)  LIGHT (~150)  BG (blue)

Saturated pixels (the green mirror line, team trim, capsule outlines, logo
text) are resolved to the majority of their neighborhood. The result is
downsampled to a grid (majority vote per cell), cleaned to the largest
connected component (drops the logo/watermark), and written as:

    docs/design/reference/aquarius_trace.json   (the class grid + geometry)
    docs/design/reference/aquarius_trace_preview.png  (for eyeball diff)

The manifest generator consumes the JSON; the preview exists so a human can
hold it against the reference and judge the ONE-TO-ONE claim before any
geometry is built on it.
"""

from __future__ import annotations

import json
from collections import Counter, deque
from pathlib import Path

from PIL import Image

GAME = Path(__file__).resolve().parents[2]
REF = GAME / "docs" / "design" / "reference" / "aquarius_thegamescabin.jpg"
OUT_JSON = GAME / "docs" / "design" / "reference" / "aquarius_trace.json"
OUT_PNG = GAME / "docs" / "design" / "reference" / "aquarius_trace_preview.png"

# v3: the founder supplied PER-FLOOR overheads - trace each from its own image.
import argparse as _argparse

CELL = 4          # px per grid cell
CLASSES = ["bg", "black", "dark", "mid", "light"]
CENTERS = {"black": 26, "dark": 65, "mid": 102, "light": 150}
PREVIEW_COLOR = {"bg": (62, 72, 99), "black": (10, 10, 10), "dark": (70, 70, 70),
                 "mid": (130, 130, 130), "light": (200, 200, 200)}


def classify(px):
    r, g, b = px
    lum = (r + g + b) // 3
    sat = max(r, g, b) - min(r, g, b)
    if b - (r + g) // 2 > 14 and lum < 135:
        return "bg"
    if sat > 40:
        return None          # accent: resolve from neighbors later
    return min(CENTERS, key=lambda c: abs(CENTERS[c] - lum)) if lum < 175 else "light"


def main():
    global REF, OUT_JSON, OUT_PNG
    ap = _argparse.ArgumentParser()
    ap.add_argument("--image", default=str(REF))
    ap.add_argument("--tag", default="")
    a = ap.parse_args()
    REF = Path(a.image)
    if a.tag:
        base = GAME / "docs" / "design" / "reference"
        OUT_JSON = base / ("aquarius_trace_%s.json" % a.tag)
        OUT_PNG = base / ("aquarius_trace_%s_preview.png" % a.tag)
    im = Image.open(REF).convert("RGB")
    W, H = im.size
    p = im.load()

    cls = [[classify(p[x, y]) for x in range(W)] for y in range(H)]
    # resolve accents from the neighborhood majority (two passes widen reach)
    for _ in range(2):
        for y in range(H):
            for x in range(W):
                if cls[y][x] is None:
                    votes = Counter()
                    for dy in (-2, -1, 0, 1, 2):
                        for dx in (-2, -1, 0, 1, 2):
                            yy, xx = y + dy, x + dx
                            if 0 <= yy < H and 0 <= xx < W and cls[yy][xx]:
                                votes[cls[yy][xx]] += 1
                    if votes:
                        cls[y][x] = votes.most_common(1)[0][0]
        if all(c is not None for row in cls for c in row):
            break
    for y in range(H):
        for x in range(W):
            if cls[y][x] is None:
                cls[y][x] = "bg"

    gw, gh = W // CELL, H // CELL
    grid = [["bg"] * gw for _ in range(gh)]
    for gy in range(gh):
        for gx in range(gw):
            votes = Counter()
            for y in range(gy * CELL, (gy + 1) * CELL):
                for x in range(gx * CELL, (gx + 1) * CELL):
                    votes[cls[y][x]] += 1
            # THIN WALLS SURVIVE: the reference draws interior room borders as
            # ~2 px black lines; a plain majority at 4 px cells erased them and
            # opened phantom cross-map sightlines (iteration 2's validator
            # finding). A cell that is meaningfully black IS a wall.
            if votes["black"] >= 3 and votes["bg"] < 8:
                grid[gy][gx] = "black"
            else:
                grid[gy][gx] = votes.most_common(1)[0][0]

    # largest 4-connected non-bg component (kills the logo and watermark text)
    seen = [[False] * gw for _ in range(gh)]
    best = []
    for sy in range(gh):
        for sx in range(gw):
            if grid[sy][sx] != "bg" and not seen[sy][sx]:
                comp, q = [], deque([(sy, sx)])
                seen[sy][sx] = True
                while q:
                    y, x = q.popleft()
                    comp.append((y, x))
                    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        yy, xx = y + dy, x + dx
                        if 0 <= yy < gh and 0 <= xx < gw and not seen[yy][xx] \
                                and grid[yy][xx] != "bg":
                            seen[yy][xx] = True
                            q.append((yy, xx))
                if len(comp) > len(best):
                    best = comp
    keep = set(best)
    for y in range(gh):
        for x in range(gw):
            if grid[y][x] != "bg" and (y, x) not in keep:
                grid[y][x] = "bg"

    xs = [x for (y, x) in keep]
    ys = [y for (y, x) in keep]
    bbox = [min(xs), min(ys), max(xs), max(ys)]

    prev = Image.new("RGB", (gw * 3, gh * 3))
    q = prev.load()
    for y in range(gh):
        for x in range(gw):
            c = PREVIEW_COLOR[grid[y][x]]
            for dy in range(3):
                for dx in range(3):
                    q[x * 3 + dx, y * 3 + dy] = c
    prev.save(OUT_PNG)

    counts = Counter(c for row in grid for c in row)
    OUT_JSON.write_text(json.dumps({
        "source": REF.name, "cell_px": CELL, "grid_w": gw, "grid_h": gh,
        "bbox_cells": bbox, "counts": dict(counts),
        "rows": ["".join({"bg": ".", "black": "#", "dark": "d",
                          "mid": "m", "light": "L"}[c] for c in row) for row in grid],
    }, indent=0), encoding="utf-8")
    print("grid %dx%d, bbox %s, counts %s" % (gw, gh, bbox, dict(counts)))
    print("wrote", OUT_JSON.name, "and", OUT_PNG.name)


if __name__ == "__main__":
    main()
