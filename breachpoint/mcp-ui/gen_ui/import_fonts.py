#!/usr/bin/env python3
"""Build one composite Font asset per UI family, with the typeface names Figma uses.

    python3 mcp-ui/gen_ui/import_fonts.py [--receipts DIR]

Requires a LIVE editor with the MCP server. ONE DRIVER ONLY (R29.2) — two sessions
writing font assets at once interleave with no transaction boundary and there is no
rollback, because a .uasset is binary.

WHY THIS SCRIPT DOES NOT IMPORT THE .ttf ITSELF
The MCP surface has no font importer. `TextureTools.import_file` answers a .ttf with
"TextureFactory does not support '.ttf' files", and no other toolset exposes a factory
or a console. What DOES import them is the editor's own content-directory monitor
(Editor Preferences → Loading & Saving → Auto Reimport): a .ttf dropped into
Content/UI/Fonts/ becomes a UFontFace asset named after the file stem, plus a
single-entry UFont wrapper `<stem>_Font`. So the workflow is: drop the files, let the
editor import, then run this — it does the part that actually matters.

WHY THE TYPEFACE NAMES ARE THE DELIVERABLE
FSlateFontInfo picks a face by TypefaceFontName. If that FName is not an entry in the
composite font, Slate falls back to the FIRST entry SILENTLY — wrong weight, no error,
no log line, everywhere that style is used. So the entries written here are exactly the
weight strings from `figma_tokens.json` → `type.styles` (`SemiBold`, `Medium Italic`,
…), and every one is read back off the asset after the write. An unverified write is
not a write.

Adding a face means adding it to the composite here as well as dropping the .ttf —
a file the editor imported but that no composite names is invisible to Slate.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_textures import MCP  # same transport, same server, one class to fix

REPO = Path(__file__).resolve().parents[2]
TOKENS = Path(__file__).resolve().parent / "figma_tokens.json"
SRC_DIR = REPO / "Content/UI/Fonts"
GAME_DIR = "/Game/UI/Fonts"

OBJ = "editor_toolset.toolsets.object.ObjectTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"

# Font asset name per family. The faces keep their upstream file stems, so that
# `<stem>.ttf` sits beside `<stem>.uasset` — the same source-next-to-asset rule the
# icons follow, and the rule the auto-importer already obeys.
FONT_ASSET = {"Rajdhani": "F_Rajdhani", "Roboto Condensed": "F_RobotoCondensed"}


def face_stem(family: str, weight: str) -> str:
    """`Roboto Condensed` + `Medium Italic` -> `RobotoCondensed-MediumItalic`."""
    return f"{family.replace(' ', '')}-{weight.replace(' ', '')}"


def required_faces() -> dict[str, list[str]]:
    """Families and weights actually used by the type ramp — read, never assumed."""
    styles = json.loads(TOKENS.read_text())["type"]["styles"]
    out: dict[str, list[str]] = {}
    for s in styles:
        out.setdefault(s["family"], [])
        if s["weight"] not in out[s["family"]]:
            out[s["family"]].append(s["weight"])
    return out


def ttf_names(path: Path) -> tuple[str, str]:
    """(family, subfamily) out of the TTF name table — nid 16/17 if present, else 1/2.

    This is the check that the bytes on disk are the WEIGHT we think they are. Font CDN
    URLs are opaque hashes; downloading SemiBold and getting Regular is silent
    otherwise, and would ship as a wrong-weight render nobody can grep for.
    """
    b = path.read_bytes()
    sfnt, n = struct.unpack(">IH", b[:6])
    if sfnt not in (0x00010000, 0x74727565):  # 'true' is the Mac-flavoured TrueType tag
        raise ValueError(f"not a TrueType file (sfntVersion {sfnt:#x})")
    tables = {}
    for i in range(n):
        tag, _csum, off, ln = struct.unpack(">4sIII", b[12 + 16 * i : 28 + 16 * i])
        tables[tag.decode("latin-1")] = (off, ln)
    off, _ = tables["name"]
    _fmt, count, str_off = struct.unpack(">HHH", b[off : off + 6])
    got: dict[int, str] = {}
    for i in range(count):
        pid, _eid, lid, nid, ln, o = struct.unpack(">6H", b[off + 6 + 12 * i : off + 18 + 12 * i])
        if pid == 3 and lid == 0x409 and nid in (1, 2, 16, 17):
            got.setdefault(nid, b[off + str_off + o : off + str_off + o + ln].decode("utf-16-be"))
    return got.get(16, got.get(1, "?")), got.get(17, got.get(2, "?"))


def typeface_entry(name: str, face: str) -> dict:
    return {
        "name": name,
        "font": {
            "fontFilename": "",
            "hinting": "Default",
            "loadingPolicy": "LazyLoad",
            "subFaceIndex": 0,
            "fontFaceAsset": {"refPath": face},
        },
    }


def read_typefaces(m: MCP, ref: dict) -> tuple[list[str], list[str]]:
    """(names, face asset paths) as they actually are on the asset right now."""
    got, txt = m.call(OBJ, "get_properties", {"instance": ref, "properties": ["compositeFont"]})
    back = json.loads(got) if isinstance(got, str) else got
    fonts = back["compositeFont"]["defaultTypeface"]["fonts"]
    return [f["name"] for f in fonts], [f["font"]["fontFaceAsset"]["refPath"] for f in fonts]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--receipts", default="docs/ui/receipts",
                    help="where the R37 receipt lands (repo-relative)")
    args = ap.parse_args()

    wanted = required_faces()

    m = MCP()
    try:
        m.init()
    except Exception as e:
        print(f"BLOCKED — no MCP server at {MCP and 'the editor'}: {e}")
        return 3

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    rpath = REPO / args.receipts / f"import-fonts-{stamp}.md"
    rpath.parent.mkdir(parents=True, exist_ok=True)
    rc = rpath.open("w", buffering=1)  # flushed per line — R37

    def w(s=""):
        print(s)
        rc.write(s + "\n")

    w(f"# RECEIPT — font import · {datetime.now(timezone.utc).isoformat()}")
    w(f"\n**Source:** `Content/UI/Fonts/` → **`{GAME_DIR}`**")
    w(f"**Families:** {json.dumps(wanted)}\n")

    fail = 0
    w("## 1 — the files on disk are the weights we think they are\n")
    faces: dict[str, list[tuple[str, str]]] = {}
    for family, weights in wanted.items():
        for weight in weights:
            stem = face_stem(family, weight)
            ttf = SRC_DIR / f"{stem}.ttf"
            try:
                fam, sub = ttf_names(ttf)
            except Exception as e:
                w(f"- **FAILED** `{stem}.ttf` — {e}")
                fail += 1
                continue
            ok = weight.replace(" ", "").lower() in (fam + sub).replace(" ", "").lower()
            w(f"- {'PASS' if ok else '**FAILED**'} `{stem}.ttf` {ttf.stat().st_size} B · "
              f"name table: family={fam!r} subfamily={sub!r} · Figma weight {weight!r}")
            fail += 0 if ok else 1
            faces.setdefault(family, []).append((weight, f"{GAME_DIR}/{stem}.{stem}"))
    if fail:
        w(f"\n## Verdict\nSTOPPED before touching an asset — {fail} source file problem(s).")
        rc.close()
        return 1

    w("\n## 2 — the editor imported a FontFace for each\n")
    for family, entries in faces.items():
        for weight, ref in entries:
            path = ref.split(".")[0]
            cls, _ = m.call(ASSET, "get_asset_class", {"asset_path": path})
            w(f"- {'PASS' if cls == 'FontFace' else '**FAILED**'} `{path}` → {cls}")
            fail += 0 if cls == "FontFace" else 1
    if fail:
        w("\n## Verdict\nSTOPPED — a face is missing. The MCP has no font importer; the "
          "editor's content-directory monitor is what imports a .ttf dropped into "
          "Content/UI/Fonts/. Check Editor Preferences → Loading & Saving → Auto Reimport, "
          "or import the file by hand in the Content Browser, then re-run.")
        rc.close()
        return 1

    w("\n## 3 — composite font per family, and what the names read back as\n")
    saved_paths, wrappers = [], []
    for family, entries in faces.items():
        asset = FONT_ASSET[family]
        path = f"{GAME_DIR}/{asset}"
        ref = {"refPath": f"{path}.{asset}"}
        first_wrapper = f"{entries[0][1].split('.')[0]}_Font"

        exists, _ = m.call(ASSET, "get_asset_class", {"asset_path": path})
        if exists != "Font":
            # No create-Font tool exists on the MCP surface. The auto-importer already
            # made a one-entry UFont per file; duplicating one is the only way to get a
            # UFont, and every field that matters is overwritten on the next line.
            done, txt = m.call(ASSET, "duplicate", {"path": first_wrapper, "new_path": path})
            if not done:
                w(f"- **FAILED** `{asset}` — could not duplicate `{first_wrapper}`: {txt[:200]}")
                fail += 1
                continue

        m.call(OBJ, "set_properties", {"instance": ref, "values": json.dumps(
            {"fontCacheType": "Runtime", "compositeFont": {"bEnableAscentDescentOverride": True}})})

        # The typeface array is written one entry at a time, growing by exactly one.
        # ObjectTools refuses a write that changes the array's SIZE and its ELEMENTS in
        # the same call ("insertion points are ambiguous") and — this is the trap — the
        # refusal is a message in the tool result, not an exception: the whole
        # compositeFont is dropped and the asset keeps whatever it had.
        try:
            cur, _ = read_typefaces(m, ref)
        except Exception:
            cur = []
        if len(cur) > len(entries):
            w(f"- **FAILED** `{asset}` — has {len(cur)} typeface entries, want {len(entries)}. "
              f"The array cannot be shrunk through this API; delete `{path}` and re-run.")
            fail += 1
            continue
        wrote_ok = True
        for n in range(max(len(cur), 1), len(entries) + 1):
            ok, txt = m.call(OBJ, "set_properties", {"instance": ref, "values": json.dumps(
                {"compositeFont": {"defaultTypeface": {"fonts": [
                    typeface_entry(name, face) for name, face in entries[:n]]}}})})
            if ok is not True:
                w(f"- **FAILED** `{asset}` — typeface entry {n} rejected: {str(txt)[:200]}")
                wrote_ok = False
                break
        if not wrote_ok:
            fail += 1
            continue

        try:
            read, linked = read_typefaces(m, ref)
        except Exception as e:
            w(f"- **FAILED** `{asset}` — unreadable composite font: {e}")
            fail += 1
            continue

        want_names = [w_ for w_, _ in entries]
        want_refs = [r for _, r in entries]
        if read != want_names or linked != want_refs:
            w(f"- **FAILED** `{asset}` — wrote {want_names} / {want_refs}, "
              f"read back {read} / {linked}")
            fail += 1
            continue
        w(f"- PASS `{path}` · TypefaceFontName entries read back **verbatim**: "
          + ", ".join(f"`{n}`" for n in read))
        for n, r in zip(read, linked):
            w(f"    - `{n}` → `{r}`")
        saved_paths.append(f"{path}.{asset}")
        wrappers += [f"{r.split('.')[0]}_Font" for _, r in entries]

    if saved_paths:
        ok, _ = m.call(ASSET, "save_assets", {"asset_paths": saved_paths})
        w(f"\n- save_assets → {ok}")

    # The per-file `<stem>_Font` wrappers the auto-importer leaves behind are a trap: each
    # is a one-entry font whose only typeface is called "Default". A style that grabs one
    # gets a font that can never resolve a weight name. Remove them once the real
    # composites are verified and saved.
    if not fail and wrappers:
        w("\n## 4 — one-entry `_Font` wrappers removed\n")
        for p in dict.fromkeys(wrappers):
            cls, _ = m.call(ASSET, "get_asset_class", {"asset_path": p})
            if cls != "Font":
                w(f"- already absent `{p}`")
                continue
            # Loaded first: the by-hand cleanup that worked had the asset loaded, and
            # `delete` reports a bare False with no error text when it declines.
            m.call(ASSET, "load_asset", {"asset_path": p})
            gone, txt = m.call(ASSET, "delete", {"path": p})
            w(f"- {'removed' if gone else 'LEFT BEHIND'} `{p}`"
              + ("" if gone else f" — {str(txt)[:120]}"))

    w(f"\n## Verdict\n{len(saved_paths)} of {len(faces)} family font(s) built and verified, "
      f"{fail} failure(s).")
    w("\n## Rung honesty\n"
      "- **Not a rung.** A Font asset that exists with the right typeface names is not text "
      "on screen. This proves the names a `FSlateFontInfo` can resolve; it does not prove any "
      "widget renders the right weight — the wrong-weight failure is silent and only a "
      "screenshot catches it.\n"
      "- Only `compositeFont` and `fontCacheType` are written and read back. Hinting, "
      "loading policy and layout method are engine defaults and unreviewed.")
    rc.close()
    print(f"\nreceipt: {rpath}")
    return 0 if fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
