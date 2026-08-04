#!/usr/bin/env python3
"""Rasterise clean Figma SVGs into RGBA PNGs at an arbitrary scale. UE cannot import SVG.

    python3 Tools/gen_ui/rasterize_svg.py Export/UI --scale 4 --out Content/UI/Icons
    python3 Tools/gen_ui/rasterize_svg.py --selftest      # prove the backend, no assets needed
    python3 Tools/gen_ui/rasterize_svg.py --probe         # just report which backends work

WHY
---
SVG is the only Figma export path that survives intact: vector, exact dimensions, and the
page backdrop is removable with certainty (see clean_svg.py). UE imports none of it. So the
last mile is a rasteriser — and the rasteriser is where the corruption creeps back in,
because most of them composite onto an opaque page. That is exactly the defect that killed
the first 41 PNG exports, and it is invisible in every image viewer.

So a backend is never trusted because it is installed. It is probed: rendered against a
known 24x24 SVG at 4x, and rejected unless the output is 96x96 with all four corners at
alpha 0, some ink present, and real anti-aliasing. Two of the tools that ship with macOS
fail that probe — qlmanage flattens onto white (corners come back 255) and sips renders
stroke-only paths at two alpha levels. Both are kept in the list so the rejection prints
out loud instead of being rediscovered later, in an icon, at runtime.

HOW THE SCALE IS APPLIED
------------------------
Not by resampling. The source header is rewritten to the target pixel size with the viewBox
left alone, into a temp copy — Export/ is never touched — so the backend re-renders the
vector at the final resolution. This matters: `sips --resampleWidth 304` on a 76px SVG
renders at 76 and upsamples, and the blur is measurable (5980 partial-alpha pixels versus
816 for a true 4x render of the same file).

Every output is then handed to preflight_textures.preflight() — the same gate the import
step uses, no second opinion. Failures go to Tools/gen_ui/quarantine/ with the reason
beside them and never reach the output folder.

ANTI-ALIASING IS NOT ALWAYS POSSIBLE
------------------------------------
This module is the only one holding the SVG text, so it is the only one that can tell
preflight's AA check whether partial alpha is even reachable. aa_possible() below answers
that, conservatively: it returns False ONLY for geometry that provably cannot land off a
device pixel. Elimination_16 was quarantined as "1-bit, will look jagged" when it is
mathematically correct art — {M,H,V,Z} only, every coordinate a multiple of 0.25, so at 4x
every edge is an exact integer pixel and zero AA is the forced output. Add_24 is the same
case in stroke form (axis-aligned segments, integer coords, even stroke-width, square caps).
Anything else — one curve, one diagonal, one coordinate that does not survive x scale, a
round cap, a transform, a non-path element — is AA-possible and stays checked.

Run clean_svg.py first; a surviving backdrop rect shows up here as a corner-alpha failure.

WHITENING (--whiten)
--------------------
Icons ship WHITE and are tinted in UMG from the palette (preflight check 6). The HUD frames
do not honour that: 14 of the 29 are one flat colour over transparency. Desaturating a
single flat fill discards no information, and baked colour permanently forecloses a
colourblind option on exactly the values most likely to need one — enemy state and team
colour. So --whiten rewrites every paint to white.

It happens HERE, on the SVG text, in the temp copy rasterize() already makes — not on the
rendered PNG. Three reasons, in order of how much they matter:

  1. Alpha is untouched by construction. Geometry does not change, so the renderer produces
     the identical alpha channel; there is no channel-stomping pass that could round an
     edge. selftest asserts this byte-for-byte against a non-whitened render of the same
     file. A post-hoc RGB stomp would have to be argued about; this one cannot be wrong.
  2. The colour removed is the exact source hex, so the receipt can name the token the UMG
     tint must be set to. Sampling it back off a rendered PNG gives an anti-aliased
     approximation of it.
  3. The multi-colour guard reads paints, not pixels. Two fills that meet along an edge
     blend through every intermediate hue in the raster; in the text they are two strings.

The guard (single_chroma) is the whole safety story: whitening is only meaningful when the
source has ONE chroma, and applied to a composite it would flatten it into a white blob that
looks plausible and is unusable. Anything with two chromas, a gradient, a pattern, an
embedded raster, or a paint it cannot parse is REFUSED and quarantined with the reason.
Greys, black and white are not chromas — they carry no colour to preserve and whiten
harmlessly — but they are reported, because an opaque black keyline baked into art meant to
be tinted (four grenades, the grapple, HUD_Reticle_Magnum) is an authoring defect that no
preflight check catches: check 6 measures RGB SPREAD, and black's spread is 0.
"""
from __future__ import annotations

import argparse, json, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path

from PIL import Image

from clean_svg import SVG_OPEN, _attrs, _canvas
from preflight_textures import MIN_AA_LEVELS, preflight

REPO = Path(__file__).resolve().parents[2]
QUARANTINE = REPO / "Tools/gen_ui/quarantine"

# A white checkmark on a 24 grid, inset far enough that edge clipping cannot confuse the
# probe. At 4x it must land as 96x96 with clear corners.
#
# The shape is chosen, not arbitrary. It is the hardest case in the real glyph set: a
# stroke-only path whose every segment sits at exactly 45 degrees. sips anti-aliases FILLS
# to ~30 alpha levels but collapses that geometry to TWO, which preflight rejects as 1-bit.
# So a filled probe — or a probe that mixes a fill in, since the levels are counted over the
# whole image — passes sips and then quarantines every arrow and checkmark in the set. The
# probe holds a candidate to preflight's own MIN_AA_LEVELS bar on the shape that actually
# breaks, which is what makes it worth running.
PROBE_SVG = ('<svg width="24" height="24" viewBox="0 0 24 24" fill="none" '
             'xmlns="http://www.w3.org/2000/svg">'
             '<path d="M20 6L9 17L4 12" stroke="white" stroke-width="2" '
             'stroke-linecap="square"/></svg>')

CHROME_CANDIDATES = [
    Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"),
    Path("/Applications/Chromium.app/Contents/MacOS/Chromium"),
    *sorted(Path.home().glob(
        "Library/Caches/ms-playwright/chromium*/chrome-*/chrome-headless-shell")),
    *sorted(Path.home().glob(
        "Library/Caches/ms-playwright/chromium*/*/Chromium.app/Contents/MacOS/Chromium")),

    # WINDOWS, added 4 Aug 2026. The list above is macOS-only and the `shutil.which` fallback
    # does not help either: Chrome and Edge are not on PATH on a stock Windows box, so
    # `--probe` reported "no Chrome/Chromium binary found" on a machine that had a perfectly
    # good Chromium installed. That left ZERO working backends here — cairosvg installs from
    # pip but cannot load `libcairo-2.dll`, and rsvg/qlmanage/sips are all Unix.
    #
    # MSEDGE COUNTS. Edge IS Chromium and supports the same `--headless --screenshot` and
    # `--default-background-color=00000000` flags, which is the flag that keeps the export
    # transparent. It ships on every Windows install, so it is the one backend that is always
    # present here. It is listed last so a real Chrome still wins when both exist.
    Path(os.environ.get("PROGRAMFILES", r"C:\Program Files")) / "Google/Chrome/Application/chrome.exe",
    Path(os.environ.get("PROGRAMFILES(X86)", r"C:\Program Files (x86)")) / "Google/Chrome/Application/chrome.exe",
    Path(os.environ.get("LOCALAPPDATA", "")) / "Google/Chrome/Application/chrome.exe",
    *sorted(Path(os.environ.get("LOCALAPPDATA", "")).glob(
        "ms-playwright/chromium*/chrome-win/chrome.exe")),
    Path(os.environ.get("PROGRAMFILES", r"C:\Program Files")) / "Microsoft/Edge/Application/msedge.exe",
    Path(os.environ.get("PROGRAMFILES(X86)", r"C:\Program Files (x86)")) / "Microsoft/Edge/Application/msedge.exe",
]


def _rel(p: Path) -> str:
    """Repo-relative for readability, absolute when the path is outside the repo."""
    try:
        return p.relative_to(REPO).as_posix()
    except ValueError:
        return str(p)


def _run(cmd: list[str]) -> None:
    r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"{cmd[0]} exit {r.returncode}: {r.stderr.strip()[:200]}")


def _chrome_exe() -> Path:
    for p in CHROME_CANDIDATES:
        if p.exists():
            return p
    for name in ("chromium", "chrome", "google-chrome", "chrome-headless-shell"):
        found = shutil.which(name)
        if found:
            return Path(found)
    raise RuntimeError("no Chrome/Chromium binary found")


# --- backends: each renders `src` (already header-scaled to w x h) into `out` ------------

def _b_cairosvg(src: Path, w: int, h: int, out: Path) -> None:
    import cairosvg
    cairosvg.svg2png(url=str(src), write_to=str(out), output_width=w, output_height=h)


def _b_rsvg(src: Path, w: int, h: int, out: Path) -> None:
    _run(["rsvg-convert", "-w", w, "-h", h, "-o", out, src])


def _b_qlmanage(src: Path, w: int, h: int, out: Path) -> None:
    with tempfile.TemporaryDirectory() as d:
        _run(["qlmanage", "-t", "-s", max(w, h), "-o", d, src])
        made = next(Path(d).glob("*.png"), None)
        if made is None:
            raise RuntimeError("qlmanage produced no thumbnail")
        Image.open(made).convert("RGBA").resize((w, h)).save(out)


def _b_sips(src: Path, w: int, h: int, out: Path) -> None:
    _run(["sips", "-s", "format", "png", src, "--out", out])


def _b_chrome(src: Path, w: int, h: int, out: Path) -> None:
    # One browser process per file, ~2s each — fine for a few hundred icons, and the last
    # resort by design. If the icon set grows past that, install librsvg
    # (`brew install librsvg`) and rsvg-convert takes over automatically: it is earlier in
    # the probe order and needs no other change here.
    # TWO PASSES, BLACK AND WHITE, AND THE ALPHA IS SOLVED FOR — not requested.
    #
    # `--default-background-color=00000000` is documented to give a transparent screenshot and
    # DOES NOT on the Chromium available here (Edge 1xx on Windows, 4 Aug 2026): both the old
    # and the new headless modes emit `mode=RGB`, no alpha channel at all, and
    # `preflight_textures` correctly quarantined all six Button Border exports twice on exactly
    # that. Relying on that flag is what makes this backend "the last resort by design".
    #
    # The recovery is exact and needs no flag to work. Render the SAME svg against black and
    # against white. For each channel, compositing gives:
    #     B = C·a + 0·(1−a) = C·a
    #     W = C·a + 255·(1−a)
    # so  W − B = 255·(1−a)  ->  a = 1 − (W−B)/255,  and  C = B/a  for a > 0.
    # Two renders, one subtraction, one divide. It is correct for partial alpha too, which is
    # what keeps the anti-aliased edges that `preflight_textures` checks for.
    import numpy as np

    exe = _chrome_exe()

    # The background is set in CSS, NOT with `--default-background-color`. That flag is ignored
    # outright by the Chromium here — proven, not assumed: rendering with `000000` and with
    # `ffffff` produced BYTE-IDENTICAL images, so the recovered alpha came back 255 everywhere
    # and preflight caught it as "background is baked in". A CSS `background` on <body> is
    # honoured by every Chromium and is what the two passes actually differ by.
    def _shot(bg: str, dest: Path, page: Path) -> None:
        page.write_text(
            "<!doctype html><html><body style=\"margin:0;padding:0;background:" + bg + "\">"
            "<img src=\"" + src.resolve().as_uri() + "\" "
            "style=\"display:block;width:" + str(w) + "px;height:" + str(h) + "px\">"
            "</body></html>", encoding="utf-8")
        _run([exe, "--headless=new", "--disable-gpu", "--no-sandbox", "--hide-scrollbars",
              f"--window-size={w},{h}", f"--screenshot={dest.resolve()}", page.resolve().as_uri()])

    with tempfile.TemporaryDirectory() as d:
        on_black, on_white = Path(d) / "b.png", Path(d) / "w.png"
        _shot("#000", on_black, Path(d) / "b.html")
        _shot("#fff", on_white, Path(d) / "w.html")

        B = np.asarray(Image.open(on_black).convert("RGB").resize((w, h)), dtype=np.float64)
        W = np.asarray(Image.open(on_white).convert("RGB").resize((w, h)), dtype=np.float64)

    # Max across channels: the three should agree, and taking the max means any single-channel
    # rounding disagreement errs toward MORE opacity rather than punching a hole in the ink.
    alpha = 255.0 - np.clip(W - B, 0.0, 255.0).max(axis=2)

    # Un-premultiply. Where alpha is 0 the colour is unknowable and irrelevant; leave it black
    # rather than dividing by zero and seeding NaNs into the PNG.
    safe = np.where(alpha > 0.0, alpha, 1.0)[:, :, None]
    rgb = np.clip(B * 255.0 / safe, 0.0, 255.0)
    rgb = np.where(alpha[:, :, None] > 0.0, rgb, 0.0)

    rgba = np.dstack([rgb, alpha]).round().astype(np.uint8)
    Image.fromarray(rgba, mode="RGBA").save(out)


BACKENDS = [
    ("cairosvg", _b_cairosvg),
    ("rsvg-convert", _b_rsvg),
    ("qlmanage", _b_qlmanage),      # present on every Mac, flattens onto white — probe kills it
    ("sips", _b_sips),
    ("chrome", _b_chrome),
]


DRAW_TAG = re.compile(r"<(path|polygon|polyline|circle|ellipse|line|rect|text|image|use)\b",
                      re.I)
PATH_TAG = re.compile(r"<path\b[^>]*>", re.I)
D_ATTR = re.compile(r'\bd\s*=\s*"([^"]*)"')
PATH_CMD = re.compile(r"[A-Za-z]")
NUMBER = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")
# Axis-aligned commands only. L/l is EXCLUDED on purpose: a straight line between two
# integer points can still be a diagonal, and a diagonal edge always needs AA. That is
# stricter than "no curve commands" and it is what keeps Slayer_16 checked.
AXIS_CMDS = set("MHVZmhvz")
# Attributes that can move an edge off the pixel grid or make ink partially transparent.
RISKY_ATTR = re.compile(
    r"\b(transform|opacity|fill-opacity|stroke-opacity|stroke-dasharray)\s*=", re.I)


def _integral(v: float, scale: float) -> bool:
    return abs(v * scale - round(v * scale)) < 1e-9


def aa_possible(text: str, scale: float) -> tuple[bool, str]:
    """Can this SVG produce partial alpha at `scale`? Returns (possible, why).

    False is a PROOF obligation, not a guess: every claim of "impossible" is checked
    against the 128 rendered PNGs and none of them carries partial alpha. When in any
    doubt the answer is True and preflight's AA check runs as before.
    """
    if RISKY_ATTR.search(text):
        return True, "transform/opacity/dash attribute present"
    tags = {m.group(1).lower() for m in DRAW_TAG.finditer(text)}
    if tags - {"path"}:
        return True, f"non-path geometry {sorted(tags - {'path'})}"
    for tag in PATH_TAG.findall(text):
        d = D_ATTR.search(tag)
        if not d:
            return True, "<path> with no d="
        payload = d.group(1)
        offenders = {c for c in PATH_CMD.findall(payload) if c not in AXIS_CMDS}
        if offenders:
            return True, f"non-axis-aligned path command {sorted(offenders)}"
        for n in NUMBER.findall(payload):
            if not _integral(float(n), scale):
                return True, f"coordinate {n} x{scale:g} is not a whole pixel"
        if re.search(r'\bstroke\s*=\s*"(?!none")', tag, re.I):
            if re.search(r'stroke-line(?:cap|join)\s*=\s*"round"', tag, re.I):
                return True, "round cap/join draws a curve"
            sw = re.search(r'stroke-width\s*=\s*"([^"]*)"', tag)
            half = (float(sw.group(1)) if sw else 1.0) / 2.0
            if not _integral(half, scale):
                return True, f"half stroke-width {half:g} x{scale:g} is not a whole pixel"
    return False, "axis-aligned; every edge lands on an integer device pixel"


# --- whitening: single-chroma art ships neutral and is tinted in UMG --------------------

PAINT_ATTR = re.compile(r'\b(fill|stroke|stop-color|flood-color)\s*=\s*"([^"]*)"', re.I)
# A paint hidden in a style="" or a <style> block would survive the rewrite and ship
# coloured. Neither appears in any Figma export in Export/UI; if one ever does, refuse.
STYLE_PAINT = re.compile(r'(<style\b|\bstyle\s*=\s*"[^"]*\b(?:fill|stroke)\s*:)', re.I)
NO_PAINT = {"none", "transparent", "currentcolor", "inherit"}
NAMED_RGB = {"white": (255, 255, 255), "black": (0, 0, 0)}
ACHROMATIC_SPREAD = 4   # black/white/grey — no colour to preserve, whitens harmlessly
CHROMA_MERGE = 8        # two hexes this close are one intended colour plus authoring drift

TOKENS = Path(__file__).with_name("figma_tokens.json")


def _parse_rgb(v: str) -> tuple[int, int, int] | None:
    """(r,g,b), or None for 'no paint'. Raises ValueError on anything it cannot read —
    a paint the guard does not understand must stop the run, never be assumed harmless."""
    s = v.strip().lower()
    if s in NO_PAINT:
        return None
    if s in NAMED_RGB:
        return NAMED_RGB[s]
    m = re.fullmatch(r"#([0-9a-f]{3}|[0-9a-f]{6})", s)
    if m:
        h = m.group(1)
        if len(h) == 3:
            h = "".join(c * 2 for c in h)
        return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
    raise ValueError(f"unreadable paint {v!r}")


def _hex(rgb: tuple[int, int, int]) -> str:
    return "#%02X%02X%02X" % rgb


def token_for(rgb: tuple[int, int, int]) -> str:
    """Name the palette token this colour is, or the one it MISSED and by how much.

    Reported, never corrected. A one-digit miss of a token is a Figma authoring bug and it
    belongs in front of whoever owns the frame; silently snapping it here would make the
    drift permanent and invisible."""
    try:
        prims = json.loads(TOKENS.read_text())["primitives"]
    except Exception:
        return "no token table"
    best, best_d = None, 10 ** 9
    for group, entries in prims.items():
        for name, val in entries.items():
            try:
                other = _parse_rgb(val.split("@")[0])
            except ValueError:
                continue
            if other is None:
                continue
            d = max(abs(a - b) for a, b in zip(rgb, other))
            if d < best_d:
                best, best_d = f"{group}/{name}", d
    if best is None:
        return "no token table"
    if best_d == 0:
        return best
    return f"OFF-PALETTE, nearest {best} (max channel delta {best_d})"


def single_chroma(text: str) -> tuple[list[tuple[int, int, int]], list[tuple[int, int, int]]]:
    """Return (chromas, achromatics) for a whitenable SVG. Raises ValueError otherwise.

    Chromas within CHROMA_MERGE of each other are ONE colour with drift — HUD_Ability_
    Grapple_Ready paints its frame #35D0F2 and its glyph #36D1F2, which is the same intended
    visr/shield typed twice. More than one real chroma means whitening would destroy
    information, and that is the case this function exists to refuse."""
    if STYLE_PAINT.search(text):
        raise ValueError("paint in a style attribute/block — cannot be rewritten safely")
    if re.search(r"<image\b", text, re.I):
        raise ValueError("embedded raster <image> — its pixels cannot be whitened here")

    chromas: list[tuple[int, int, int]] = []
    achromatic: list[tuple[int, int, int]] = []
    for _attr, val in PAINT_ATTR.findall(text):
        if val.strip().lower().startswith("url("):
            raise ValueError(f"gradient/pattern paint {val!r} — not a flat colour")
        rgb = _parse_rgb(val)            # ValueError propagates on purpose
        if rgb is None:
            continue
        bucket = achromatic if max(rgb) - min(rgb) <= ACHROMATIC_SPREAD else chromas
        if not any(max(abs(a - b) for a, b in zip(rgb, seen)) <= CHROMA_MERGE
                   for seen in bucket):
            bucket.append(rgb)
    if len(chromas) > 1:
        raise ValueError("multi-colour: " + ", ".join(_hex(c) for c in chromas)
                         + " — a white master cannot reproduce these with one tint")
    return chromas, achromatic


def whiten(text: str) -> tuple[str, list[tuple[int, int, int]], list[tuple[int, int, int]]]:
    """Rewrite every paint to white. Returns (text, chromas_removed, achromatics_removed)."""
    chromas, achromatic = single_chroma(text)

    def repl(m: re.Match) -> str:
        return m.group(0) if m.group(2).strip().lower() in NO_PAINT \
            else f'{m.group(1)}="white"'

    return PAINT_ATTR.sub(repl, text), chromas, achromatic


def scaled_header(text: str, scale: float) -> tuple[str, float, float, int, int]:
    """Retarget the <svg> header to src_size * scale px, viewBox untouched.

    Returns (svg_text, src_w, src_h, out_w, out_h). Without a viewBox, growing width/height
    would grow the canvas and leave the art at its original size, so one is synthesised
    from the original dimensions first.
    """
    m = SVG_OPEN.search(text)
    if not m:
        raise ValueError("no <svg> element")
    size = _canvas(m.group(0))
    if not size:
        raise ValueError("no usable width/height or viewBox")
    sw, sh = size
    ow, oh = int(sw * scale), int(sh * scale)

    tag = m.group(0)
    if "viewbox" not in _attrs(tag):
        tag = tag[:4] + f' viewBox="0 0 {sw:g} {sh:g}"' + tag[4:]
    tag = re.sub(r'\s(?:width|height)\s*=\s*"[^"]*"', "", tag, flags=re.I)
    tag = tag[:4] + f' width="{ow}" height="{oh}"' + tag[4:]
    return text[:m.start()] + tag + text[m.end():], sw, sh, ow, oh


def rasterize(svg: Path, scale: float, out: Path, backend,
              whiten_ink: bool = False) -> tuple[bool, str, list]:
    """Render one SVG. Returns aa_possible(), the one fact only this side knows, plus the
    paints whitening removed (empty unless whiten_ink).

    It no longer returns the source size: feeding that to preflight's dimension check made
    the check tautological, since it is the same number the render was made from.
    """
    text = svg.read_text()
    removed: list = []
    if whiten_ink:
        text, chromas, achromatic = whiten(text)      # ValueError = refused, do not render
        removed = [(_hex(c), token_for(c)) for c in chromas] + \
                  [(_hex(c), "achromatic") for c in achromatic]
    text, sw, sh, ow, oh = scaled_header(text, scale)
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d) / svg.name
        tmp.write_text(text)
        backend(tmp, ow, oh, out)
    if not out.exists():
        raise RuntimeError("backend wrote nothing")
    aa_ok, why = aa_possible(text, scale)
    return aa_ok, why, removed


def probe_backend(fn, tmpdir: Path) -> str:
    """Render PROBE_SVG at 4x. Returns "" if usable, else why not."""
    src = tmpdir / "probe.svg"
    src.write_text(PROBE_SVG)
    out = tmpdir / "probe.png"
    out.unlink(missing_ok=True)
    try:
        rasterize(src, 4, out, fn)
    except Exception as e:
        return str(e).splitlines()[0][:120]
    im = Image.open(out).convert("RGBA")
    w, h = im.size
    if (w, h) != (96, 96):
        return f"size {w}x{h}, expected 96x96"
    a = im.getchannel("A")
    px = a.load()
    corners = [px[0, 0], px[w - 1, 0], px[0, h - 1], px[w - 1, h - 1]]
    hist = a.histogram()
    if any(corners):
        return f"corner alpha {corners} — flattens transparency onto a background"
    if not sum(hist[1:]):
        return "no ink — rendered an empty canvas"
    levels = sum(1 for lvl in range(1, 255) if hist[lvl])
    if levels < MIN_AA_LEVELS:
        return f"only {levels} partial-alpha levels — strokes come out jagged"
    return ""


def pick_backend(verbose: bool = True):
    with tempfile.TemporaryDirectory() as d:
        for name, fn in BACKENDS:
            why = probe_backend(fn, Path(d))
            if verbose:
                print(f"  {'ok   ' if not why else 'no   '} {name:14} {why}")
            if not why:
                return name, fn
    return None, None


def quarantine(png: Path, rel: str, reasons: list[str]) -> Path:
    dest = QUARANTINE / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(png), dest)
    dest.with_suffix(".txt").write_text("\n".join(reasons) + "\n")
    return dest


# --- self-test fixtures. Real payloads, copied from Export/UI, so the cases are the ones
# --- that actually shipped wrong. No assets are read: the strings ARE the evidence.
def _svg(vb_w: int, vb_h: int, body: str) -> str:
    return (f'<svg width="{vb_w}" height="{vb_h}" viewBox="0 0 {vb_w} {vb_h}" fill="none" '
            f'xmlns="http://www.w3.org/2000/svg"><g>{body}</g></svg>')


# Elimination_16's real path: {M,H,V,Z} only, every coordinate a multiple of 0.25.
AXIS_PATH = ('<path d="M2.25 2.5H4.75V13.5H2.25V2.5ZM6.75 6H9.25V13.5H6.75V6ZM11.25 '
             '9.5H13.75V13.5H11.25V9.5Z" fill="white"/>')
# Slayer_16's real path — diagonals, and coordinates that do not survive x4.
SLAYER_PATH = ('<path d="M3.11306 11.0565L11.0565 3.11306L12.8869 4.9435L4.94351 '
               '12.8869L3.11306 11.0565Z" fill="white"/>')
# Elimination_40's real path — axis-aligned, but 6.45502 and 29.46 are not whole pixels.
ELIM40_PATH = ('<path d="M6.45502 7.10001H11.615V29.46H6.45502V7.10001ZM13.765 '
               '12.69H18.925V29.46H13.765V12.69Z" fill="white"/>')
CURVE_PATH = '<path d="M2 2H14V8C14 12 10 14 2 14Z" fill="white"/>'


def selftest_checks(fn) -> None:
    """Prove the two repaired checks: dimensions can FAIL, and AA exemption is narrow."""
    # --- check 7 predicate: exempt only what provably cannot anti-alias ---------------
    cases = [
        ("axis-aligned quarter-unit (Elimination_16)", _svg(16, 16, AXIS_PATH), False),
        ("same shape plus one C curve",                _svg(16, 16, CURVE_PATH), True),
        ("diagonals (Slayer_16)",                      _svg(16, 16, SLAYER_PATH), True),
        ("6.455 / 29.46 coords (Elimination_40)",      _svg(40, 40, ELIM40_PATH), True),
        ("stroked axis-aligned, width 2 (Add_24)",
         _svg(24, 24, '<path d="M5 12H19" stroke="white" stroke-width="2" '
                      'stroke-linecap="square"/>'), False),
        ("same stroke with a round cap",
         _svg(24, 24, '<path d="M5 12H19" stroke="white" stroke-width="2" '
                      'stroke-linecap="round"/>'), True),
    ]
    for label, text, want in cases:
        got, why = aa_possible(text, 4)
        assert got == want, f"aa_possible({label}) = {got}, expected {want} ({why})"
        print(f"  aa_possible={str(got):5} {label:44} {why}")

    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)

        # --- check 7 end to end: the exemption skips, and without it the check bites ---
        src = tmp / "T_UI_Icon_Mode_SelfAxis_16.svg"
        src.write_text(_svg(16, 16, AXIS_PATH))
        png = tmp / "T_UI_Icon_Mode_SelfAxis_16.png"
        aa_ok, *_ = rasterize(src, 4, png, fn)
        assert aa_ok is False, "predicate changed its mind between text and render"
        levels = sum(1 for lvl in range(1, 255)
                     if Image.open(png).convert("RGBA").getchannel("A").histogram()[lvl])
        assert levels < MIN_AA_LEVELS, (
            f"fixture rendered {levels} AA levels — it is no longer the 1-bit case, so it "
            "proves nothing")
        ok_ex, fails_ex, _ = preflight(png, {"scale": 4}, aa_possible=False)
        ok_ck, fails_ck, _ = preflight(png, {"scale": 4}, aa_possible=True)
        assert ok_ex, f"exempt art still failed: {fails_ex}"
        assert any("1-bit" in f for f in fails_ck), (
            f"check 7 did not fire when AA was possible — it is dead: {fails_ck}")
        print(f"  check 7  {levels} AA levels · exempt=PASS · checked=FAIL — skip is "
              "conditional, not removed")

        # --- check 4: a 17x16 source under a _16 name must FAIL ----------------------
        wide = tmp / "T_UI_Icon_Mode_SelfWide_16.svg"
        wide.write_text(_svg(17, 16, AXIS_PATH))          # the Assault_16 / Extraction_16 bug
        wpng = tmp / "T_UI_Icon_Mode_SelfWide_16.png"
        wide_aa, *_ = rasterize(wide, 4, wpng, fn)
        w, h = Image.open(wpng).size
        assert (w, h) == (68, 64), f"fixture rendered {w}x{h}, expected the 68x64 defect"
        ok_w, fails_w, _ = preflight(wpng, {"scale": 4}, aa_possible=wide_aa)
        assert not ok_w and any("expected 64x64" in f for f in fails_w), (
            f"a 68x64 '_16' icon passed the dimension check: {fails_w}")
        print(f"  check 4  68x64 under a _16 name -> FAIL: {fails_w[0]}")
        # and the square control passes, so the check is not simply always-fail
        ok_sq, fails_sq, _ = preflight(png, {"scale": 4}, aa_possible=False)
        assert ok_sq and not any("size" in f for f in fails_sq), (
            f"the correct 64x64 icon was rejected too: {fails_sq}")
        print("  check 4  64x64 under a _16 name -> pass (control)")

        # --- an unrecognised name warns, never silently passes -----------------------
        odd = tmp / "Whatever_This_Is.png"
        Image.open(png).save(odd)
        _, _, summary = preflight(odd, {"scale": 4}, aa_possible=False)
        assert "WARN:" in summary and "UNCHECKED" in summary, summary
        print(f"  check 4  unrecognised name -> {summary.split('·')[-1].strip()}")


# HUD_Grenade_Frag_Sel's real glyph: one chroma (#36D1F2) plus the baked black keyline.
FRAG_PATH = ('<path id="glyph" fill-rule="evenodd" clip-rule="evenodd" d="M5.5 0.834961L10.5 '
             '4.83496V11.835L5.5 15.835L0.5 11.835V4.83496L5.5 0.834961Z" fill="#36D1F2" '
             'stroke="black"/>')
# HUD_Ammo_Readout's real pair of fills: the cyan mag count and the dim reserve count. This
# is the composite the guard exists to refuse — two fills, no single tint reproduces both.
AMMO_PATHS = ('<path d="M8 8H60V32H8V8Z" fill="#36D1F2"/>'
              '<path d="M70 14H120V30H70V14Z" fill="#8CBFE0"/>')
# HUD_Vitals_ShieldHealth's real gradient reference.
GRAD_PATH = ('<path d="M2 2H180V30H2V2Z" fill="url(#paint0_linear_6_48)"/>'
             '<defs><linearGradient id="paint0_linear_6_48"><stop stop-color="#617D94"/>'
             '<stop offset="1" stop-color="#AFD5FC"/></linearGradient></defs>')


def selftest_whiten(fn) -> None:
    """Prove the two properties whitening lives or dies on: alpha is untouched, and a
    composite is REFUSED rather than flattened."""
    # --- the guard: what it accepts, and what it must refuse ---------------------------
    chromas, achro = single_chroma(_svg(23, 17, FRAG_PATH))
    assert [_hex(c) for c in chromas] == ["#36D1F2"], chromas
    assert [_hex(c) for c in achro] == ["#000000"], achro
    print(f"  guard  accept  one chroma {_hex(chromas[0])} + baked {_hex(achro[0])} keyline")

    # #35D0F2 and #36D1F2 in one file (HUD_Ability_Grapple_Ready) are ONE colour typed
    # twice, not a composite — the merge must hold or the grapple is refused for drift.
    drifted, _ = single_chroma(_svg(52, 31, '<rect width="49" height="29" stroke="#35D0F2"/>'
                                            + FRAG_PATH))
    assert len(drifted) == 1, f"palette drift read as a composite: {drifted}"
    print(f"  guard  accept  #35D0F2 + #36D1F2 merged (delta 1) -> {_hex(drifted[0])}")

    for label, text, want in [
        ("two fills (HUD_Ammo_Readout)", _svg(190, 40, AMMO_PATHS), "multi-colour"),
        ("gradient (HUD_Vitals_ShieldHealth)", _svg(277, 35, GRAD_PATH), "gradient"),
        ("unreadable paint", _svg(16, 16, '<rect width="8" height="8" fill="rgb(1,2,3)"/>'),
         "unreadable paint"),
        ("paint in a style attribute",
         _svg(16, 16, '<rect width="8" height="8" style="fill:#36D1F2"/>'), "style"),
    ]:
        try:
            whiten(text)
        except ValueError as e:
            assert want in str(e), f"{label} refused for the wrong reason: {e}"
            print(f"  guard  REFUSE  {label:36} {e}")
        else:
            raise AssertionError(f"{label} was WHITENED — the guard is a shredder, not a gate")

    # --- alpha survives the rewrite ---------------------------------------------------
    #
    # MEASURED, not assumed. A fill-only source comes back BYTE-IDENTICAL. A source whose
    # path carries both a fill and a stroke does not, quite: 43 of 6256 px move by exactly
    # 1. That is the renderer, not the rewrite. Skia composites the stroke as a second op
    # over the fill, and when the two paints become the same colour it takes a different
    # rounding path along the coincident seam. Verified: same file rendered twice unwhitened
    # is bit-identical (so the backend is deterministic), the delta appears ONLY where a
    # fill and a stroke overlap, it never exceeds 1, and no pixel crosses 0 -> non-zero.
    # The silhouette is the thing that must not move, and it does not.
    #
    # The bar is set at that measurement rather than at zero on purpose: a passing test that
    # can only pass on fill-only art would have been deleted the first time a stroked icon
    # hit it, which is how a real check becomes a decoration.
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        fill_only = _svg(23, 17, re.sub(r'\s*stroke="black"', "", FRAG_PATH))
        for label, text, tol in [("fill only", fill_only, 0),
                                 ("fill + stroke", _svg(23, 17, FRAG_PATH), 1)]:
            src = tmp / "s.svg"
            src.write_text(text)
            plain, white = tmp / "plain.png", tmp / "white.png"
            rasterize(src, 4, plain, fn)
            _, _, removed = rasterize(src, 4, white, fn, whiten_ink=True)
            a_im = Image.open(plain).convert("RGBA")
            b_im = Image.open(white).convert("RGBA")
            A, B = list(a_im.getchannel("A").getdata()), list(b_im.getchannel("A").getdata())
            moved = [abs(x - y) for x, y in zip(A, B) if x != y]
            silhouette = sum(1 for x, y in zip(A, B) if (x > 0) != (y > 0))
            assert max(moved, default=0) <= tol, (
                f"{label}: alpha moved by {max(moved)} (tolerance {tol}) — anti-aliasing "
                "lives in this channel")
            assert silhouette == 0, f"{label}: {silhouette} px entered or left the silhouette"
            bad = [c for c, al in zip(b_im.convert("RGB").getdata(), B)
                   if al and c != (255, 255, 255)]
            assert not bad, f"{label}: {len(bad)} ink pixels are not white, e.g. {bad[0]}"
            # the control: it really was coloured before, so the comparison means something
            was = {c for c, al in zip(a_im.convert("RGB").getdata(), A) if al == 255}
            assert (0x36, 0xD1, 0xF2) in was, f"{label}: fixture was not coloured to begin with"
            levels = sum(1 for lvl in range(1, 255) if b_im.getchannel("A").histogram()[lvl])
            print(f"  alpha  {label:14} {len(moved)}/{len(A)} px moved (max "
                  f"{max(moved, default=0)}, tol {tol}) · silhouette intact · all ink RGB 255 "
                  f"· {levels} AA levels")
        print(f"  removed {removed}")


def selftest() -> int:
    print("backend probe:")
    name, fn = pick_backend()
    if not fn:
        print("\nNo usable rasteriser on this machine. STOPPING rather than pip-installing "
              "one silently — install cairosvg or librsvg and re-run.")
        return 2
    with tempfile.TemporaryDirectory() as d:
        src = Path(d) / "selftest.svg"
        src.write_text(PROBE_SVG)
        out = Path(d) / "selftest.png"
        rasterize(src, 4, out, fn)
        im = Image.open(out).convert("RGBA")
        w, h = im.size
        a = im.getchannel("A")
        px = a.load()
        corners = [px[0, 0], px[w - 1, 0], px[0, h - 1], px[w - 1, h - 1]]
        hist = a.histogram()
        ink = sum(hist[1:])
        levels = sum(1 for lvl in range(1, 255) if hist[lvl])
        assert (w, h) == (96, 96), f"24x24 @4x came out {w}x{h}"
        assert corners == [0, 0, 0, 0], f"corner alpha {corners}, expected all 0"
        assert ink > 0, "no ink — rendered an empty canvas"
        assert levels >= MIN_AA_LEVELS, f"only {levels} partial-alpha levels"
        print(f"\nselftest via {name}: {w}x{h} · corners {corners} · {ink} ink px · "
              f"{levels} AA levels · PASS")
    print("\ncheck 4 (dimensions) + check 7 (anti-aliasing):")
    selftest_checks(fn)
    print("\nwhitening (--whiten): the guard, and alpha survival:")
    selftest_whiten(fn)
    print("\nall self-tests PASS")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("root", nargs="?", default="Export/UI")
    ap.add_argument("--scale", type=float, default=4)
    ap.add_argument("--out", default="Content/UI/Icons")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--probe", action="store_true")
    ap.add_argument("--whiten", action="store_true",
                    help="rewrite single-chroma art to white for UMG tinting; REFUSES "
                         "multi-colour, gradient or unparseable sources")
    args = ap.parse_args()

    if args.selftest:
        return selftest()
    if args.probe:
        print("backend probe:")
        name, _ = pick_backend()
        print(f"\nchosen: {name or 'NONE'}")
        return 0 if name else 2

    root = Path(args.root)
    root = root if root.is_absolute() else REPO / root
    out_root = Path(args.out)
    out_root = out_root if out_root.is_absolute() else REPO / out_root

    svgs = sorted(root.rglob("*.svg"))
    if not svgs:
        print(f"no SVGs under {root}")
        return 2

    print("backend probe:")
    name, fn = pick_backend()
    if not fn:
        print("\nNo usable rasteriser on this machine. STOPPING rather than pip-installing "
              "one silently — install cairosvg or librsvg and re-run.")
        return 2
    print(f"\nrasterising {len(svgs)} SVG(s) at {args.scale:g}x via {name} -> "
          f"{_rel(out_root)}\n")

    bad, whitened = [], {}
    for p in svgs:
        rel = p.relative_to(root).with_suffix(".png").as_posix()
        dest = out_root / rel
        try:
            aa_ok, _why, removed = rasterize(p, args.scale, dest, fn, args.whiten)
        except ValueError as e:
            # The whitening guard refusing. Nothing was rendered, so there is no PNG to
            # quarantine — only the reason, which is the part anyone reads anyway.
            print(f"  FAIL  {rel}")
            print(f"          - not whitenable: {e}")
            note = (QUARANTINE / rel).with_suffix(".txt")
            note.parent.mkdir(parents=True, exist_ok=True)
            note.write_text(f"not whitenable: {e}\n")
            bad.append(rel)
            continue
        except Exception as e:
            print(f"  FAIL  {rel}")
            print(f"          - render failed: {e}")
            bad.append(rel)
            continue
        if removed:
            whitened[rel] = removed

        ok, fails, summary = preflight(dest, {"scale": args.scale}, aa_possible=aa_ok)
        if ok:
            print(f"  ok    {rel}  {summary}")
        else:
            q = quarantine(dest, rel, fails)
            print(f"  FAIL  {rel}  -> {_rel(q)}")
            for f in fails:
                print(f"          - {f}")
            bad.append(rel)

    if whitened:
        print(f"\nWHITENED — {len(whitened)} source(s); set the UMG tint from these tokens:")
        for rel, cols in sorted(whitened.items()):
            for hx, tok in cols:
                print(f"  {rel:38} {hx}  {tok}")
    print(f"\n{len(svgs) - len(bad)}/{len(svgs)} rasterised and pre-flighted")
    if bad:
        print(f"QUARANTINED — see {_rel(QUARANTINE)}:")
        for b in bad:
            print(f"  {b}")
        return 1
    print("All clean. Safe to import into UE.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
