#!/usr/bin/env python3
"""Strip the baked-in page backdrop out of a Figma SVG export, and verify what is left.

    python3 mcp-ui/gen_ui/clean_svg.py Export/UI            # clean + verify a tree
    python3 mcp-ui/gen_ui/clean_svg.py Export/UI --check    # verify only, change nothing

WHY
---
Figma's asset export composites the node against its PAGE BACKDROP. In PNG that shows up as
corner alpha 255; in SVG it shows up as a full-canvas rect emitted before the content group:

    <svg width="76" height="76" viewBox="0 0 76 76" ...>
      <rect width="76" height="76" fill="#F5F5F5"/>     <-- the backdrop, not the art
      <g id="Rank / 01"> ... </g>
    </svg>

That rect is why the first 41 PNG exports were unusable. In SVG it is removable with
certainty, because it is identifiable structurally rather than by guessing at colour:
a <rect> that (a) is a direct child of <svg>, (b) covers the whole canvas, and (c) carries a
solid fill. The artwork lives inside <g> elements, so nothing real matches that shape.

WHAT IS CHECKED AFTER CLEANING
------------------------------
  - no full-canvas rect survives
  - width/height/viewBox are present and agree with each other
  - at least one drawing element (path/polygon/circle/ellipse/line) remains, so an
    "everything was stripped" bug cannot pass silently
"""
from __future__ import annotations

import re, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

SVG_OPEN = re.compile(r"<svg\b[^>]*>", re.I)
RECT = re.compile(r"<rect\b[^>]*/?>", re.I)
ATTR = re.compile(r'(\w[\w:-]*)\s*=\s*"([^"]*)"')
# `rect` belongs here. Omitting it was a false negative that failed three real HUD assets:
# a crosshair IS four rects. HUD_Reticle_Sniper is 5 rects and nothing else; the damage
# direction indicator and shield-break flash are 4 rotated rects each. All three were
# reported "no drawing element left - everything was stripped" while being entirely correct.
# Safe against the opposite error: clean() removes the backdrop rect BEFORE verify() runs, so
# a file whose only rect was the backdrop still has zero rects here and still fails.
DRAW = re.compile(r"<(path|polygon|polyline|circle|ellipse|line|rect)\b", re.I)


def _attrs(tag: str) -> dict:
    return {k.lower(): v for k, v in ATTR.findall(tag)}


def _canvas(svg_tag: str) -> tuple[float, float] | None:
    a = _attrs(svg_tag)
    try:
        if "viewbox" in a:
            p = a["viewbox"].replace(",", " ").split()
            return float(p[2]), float(p[3])
        return float(a["width"]), float(a["height"])
    except Exception:
        return None


def clean(text: str) -> tuple[str, int]:
    """Remove backdrop rects. Returns (cleaned_text, n_removed)."""
    m = SVG_OPEN.search(text)
    if not m:
        return text, 0
    canvas = _canvas(m.group(0))
    if not canvas:
        return text, 0
    cw, ch = canvas

    head_end = m.end()
    # Only consider rects BEFORE the first <g>. The backdrop is emitted first; a rect
    # inside a group is artwork and must never be touched.
    first_g = text.lower().find("<g", head_end)
    scan_end = first_g if first_g != -1 else len(text)
    head, body = text[head_end:scan_end], text[scan_end:]

    removed = 0

    def drop(match: re.Match) -> str:
        nonlocal removed
        a = _attrs(match.group(0))
        try:
            w, h = float(a.get("width", -1)), float(a.get("height", -1))
            x, y = float(a.get("x", 0)), float(a.get("y", 0))
        except ValueError:
            return match.group(0)
        covers = abs(w - cw) < 0.51 and abs(h - ch) < 0.51 and abs(x) < 0.51 and abs(y) < 0.51
        solid = "fill" in a and a["fill"].lower() not in ("none", "transparent")
        if covers and solid:
            removed += 1
            return ""
        return match.group(0)

    return text[:head_end] + RECT.sub(drop, head) + body, removed


def verify(text: str, path: Path) -> list[str]:
    fails = []
    m = SVG_OPEN.search(text)
    if not m:
        return ["no <svg> element"]
    a = _attrs(m.group(0))
    canvas = _canvas(m.group(0))
    if not canvas:
        return ["no usable width/height or viewBox"]
    cw, ch = canvas

    if "width" not in a or "height" not in a:
        fails.append("missing width/height attribute")
    elif "viewbox" in a:
        try:
            if abs(float(a["width"]) - cw) > 0.51 or abs(float(a["height"]) - ch) > 0.51:
                fails.append(f"width/height {a['width']}x{a['height']} "
                             f"disagrees with viewBox {cw}x{ch}")
        except ValueError:
            fails.append("non-numeric width/height")

    first_g = text.lower().find("<g", m.end())
    head = text[m.end():first_g if first_g != -1 else len(text)]
    for r in RECT.finditer(head):
        ra = _attrs(r.group(0))
        try:
            if (abs(float(ra.get("width", -1)) - cw) < 0.51
                    and abs(float(ra.get("height", -1)) - ch) < 0.51
                    and ra.get("fill", "none").lower() not in ("none", "transparent")):
                fails.append(f"full-canvas rect survives: fill={ra.get('fill')}")
        except ValueError:
            pass

    if not DRAW.search(text):
        fails.append("no drawing element left — everything was stripped")
    return fails


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check_only = "--check" in sys.argv
    root = Path(args[0]) if args else (REPO / "Export/UI")
    if not root.is_absolute():
        root = REPO / root
    svgs = sorted(root.rglob("*.svg"))
    if not svgs:
        print(f"no SVGs under {root}")
        return 2

    bad, total_removed = [], 0
    for p in svgs:
        text = p.read_text()
        cleaned, n = clean(text)
        if n and not check_only:
            p.write_text(cleaned)
        total_removed += n
        fails = verify(cleaned, p)
        rel = p.relative_to(root).as_posix()
        m = SVG_OPEN.search(cleaned)
        size = _canvas(m.group(0)) if m else None
        dims = f"{size[0]:g}x{size[1]:g}" if size else "?"
        if fails:
            print(f"  FAIL  {rel}  ({dims})")
            for f in fails:
                print(f"          - {f}")
            bad.append(rel)
        else:
            note = f" · stripped {n} backdrop" if n else ""
            print(f"  ok    {rel}  {dims}{note}")

    print(f"\n{len(svgs) - len(bad)}/{len(svgs)} clean · {total_removed} backdrop rect(s) removed")
    if bad:
        print("REJECTED:")
        for b in bad:
            print(f"  {b}")
        return 1
    print("All SVGs are backdrop-free with consistent dimensions.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
