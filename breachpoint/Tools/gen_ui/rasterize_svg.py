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

Run clean_svg.py first; a surviving backdrop rect shows up here as a corner-alpha failure.
"""
from __future__ import annotations

import argparse, re, shutil, subprocess, sys, tempfile
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
    _run([_chrome_exe(), "--headless", "--disable-gpu", "--hide-scrollbars",
          "--default-background-color=00000000", f"--window-size={w},{h}",
          f"--screenshot={out.resolve()}", src.resolve().as_uri()])


BACKENDS = [
    ("cairosvg", _b_cairosvg),
    ("rsvg-convert", _b_rsvg),
    ("qlmanage", _b_qlmanage),      # present on every Mac, flattens onto white — probe kills it
    ("sips", _b_sips),
    ("chrome", _b_chrome),
]


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


def rasterize(svg: Path, scale: float, out: Path, backend) -> tuple[float, float]:
    """Render one SVG. Returns the SOURCE size, for preflight's dimension check."""
    text, sw, sh, ow, oh = scaled_header(svg.read_text(), scale)
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d) / svg.name
        tmp.write_text(text)
        backend(tmp, ow, oh, out)
    if not out.exists():
        raise RuntimeError("backend wrote nothing")
    return sw, sh


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
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("root", nargs="?", default="Export/UI")
    ap.add_argument("--scale", type=float, default=4)
    ap.add_argument("--out", default="Content/UI/Icons")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--probe", action="store_true")
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

    bad = []
    for p in svgs:
        rel = p.relative_to(root).with_suffix(".png").as_posix()
        dest = out_root / rel
        try:
            sw, sh = rasterize(p, args.scale, dest, fn)
        except Exception as e:
            print(f"  FAIL  {rel}")
            print(f"          - render failed: {e}")
            bad.append(rel)
            continue

        ok, fails, summary = preflight(dest, {"w": sw, "h": sh, "scale": args.scale})
        if ok:
            print(f"  ok    {rel}  {summary}")
        else:
            q = quarantine(dest, rel, fails)
            print(f"  FAIL  {rel}  -> {_rel(q)}")
            for f in fails:
                print(f"          - {f}")
            bad.append(rel)

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
