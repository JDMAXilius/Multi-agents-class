#!/usr/bin/env python3
"""Import a folder of PNGs into UE as UI textures with the four settings that matter.

    python3 Tools/gen_ui/import_textures.py Content/UI/Icons/Glyphs /Game/UI/Icons/Glyphs

Requires a LIVE editor with the MCP server. ONE DRIVER ONLY (R29.2) — two sessions
importing at once interleave with no transaction boundary and there is no rollback,
because a .uasset is binary.

The four settings, from ASSET-PIPELINE.md §4. Defaults are wrong for UI and the failure
is subtle — blurry icons and a texture budget three times bigger than it needs to be:

  lODGroup           TEXTUREGROUP_UI       UI must not be downscaled by texture-quality
                                           scalability. The default World group means
                                           low-spec players get a blurry HUD.
  compressionSettings TC_EditorIcon        UserInterface2D (RGBA). Preserves alpha edges;
                                           the default DXT block-compresses and fringes
                                           1px strokes and small glyphs.
  mipGenSettings     TMGS_NoMipmaps        UI draws at ~1:1; mips waste memory and soften.
  sRGB               True for colour       A mask read as sRGB gives wrong values.

Property names are camelCase and are NOT guessable — `ObjectTools.list_properties` was run
against a real imported texture to get them. `lODGroup` really is spelled with that capital
run; it comes from UHT's mangling of `LODGroup`.
"""
from __future__ import annotations

import json, sys, urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
URL = "http://127.0.0.1:8000/mcp"
TEX = "editor_toolset.toolsets.texture.TextureTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"

# TC_EditorIcon is UE's enum name for the "UserInterface2D (RGBA)" entry in the editor UI.
UI_TEXTURE_SETTINGS = {
    "lODGroup": "TEXTUREGROUP_UI",
    "compressionSettings": "TC_EditorIcon",
    "mipGenSettings": "TMGS_NoMipmaps",
    "sRGB": True,
}


class MCP:
    def __init__(self):
        self.sid = None
        self.n = 0

    def _post(self, payload):
        req = urllib.request.Request(URL, data=json.dumps(payload).encode(), method="POST")
        req.add_header("Content-Type", "application/json")
        req.add_header("Accept", "application/json, text/event-stream")
        if self.sid:
            req.add_header("Mcp-Session-Id", self.sid)
        with urllib.request.urlopen(req, timeout=120) as r:
            self.sid = r.headers.get("Mcp-Session-Id") or self.sid
            body = r.read().decode()
        if body.lstrip().startswith(("event:", "data:")):
            body = "\n".join(l[5:].strip() for l in body.splitlines() if l.startswith("data:"))
        return json.loads(body) if body.strip() else None

    def init(self):
        self._post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "protocolVersion": "2025-06-18", "capabilities": {},
            "clientInfo": {"name": "breachpoint-import-textures", "version": "1"}}})
        try:
            self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})
        except Exception:
            pass

    def call(self, toolset, tool, args):
        self.n += 1
        r = self._post({"jsonrpc": "2.0", "id": 200 + self.n, "method": "tools/call",
                        "params": {"name": "call_tool", "arguments": {
                            "toolset_name": toolset, "tool_name": tool, "arguments": args}}})
        res = (r or {}).get("result", {})
        txt = "\n".join(c["text"] for c in res.get("content", []) if c.get("type") == "text")
        if "error" in (r or {}) or res.get("isError"):
            return None, txt or json.dumps((r or {}).get("error", {}))
        try:
            return json.loads(txt).get("returnValue"), txt
        except Exception:
            return txt, txt


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    src_dir = (REPO / sys.argv[1]).resolve()
    game_dir = sys.argv[2].rstrip("/")
    pngs = sorted(src_dir.glob("*.png"))
    if not pngs:
        print(f"no PNGs in {src_dir}")
        return 2

    m = MCP()
    try:
        m.init()
    except Exception as e:
        print(f"BLOCKED — no MCP server at {URL}: {e}")
        return 3

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    rpath = REPO / "docs/ui/receipts" / f"import-textures-{stamp}.md"
    rpath.parent.mkdir(parents=True, exist_ok=True)
    # encoding="utf-8" is REQUIRED. `Path.open` defaults to the locale encoding, which is
    # cp1252 on a stock Windows box, and this receipt's own header contains an em dash and a
    # `->` arrow. Without it the run dies with UnicodeEncodeError partway through the header,
    # AFTER the editor connection is live and possibly after assets have been imported --
    # i.e. it fails with the work half-done and no receipt saying so. Same latent bug as
    # `build_wbp.py` had; fixed there 3 Aug 2026 and missed here.
    rc = rpath.open("w", buffering=1, encoding="utf-8")     # flushed per line — R37

    def w(s=""):
        print(s)
        rc.write(s + "\n")

    w(f"# RECEIPT — texture import · {datetime.now(timezone.utc).isoformat()}")
    w(f"\n**Source:** `{sys.argv[1]}` → **`{game_dir}`** · {len(pngs)} PNG(s)")
    w(f"**Settings applied:** `{json.dumps(UI_TEXTURE_SETTINGS)}`\n")

    ok_n = fail_n = 0
    for p in pngs:
        name = p.stem
        _, txt = m.call(TEX, "import_file", {
            "folder_path": game_dir, "asset_name": name, "source_file": str(p)})
        ref = {"refPath": f"{game_dir}/{name}.{name}"}

        # An import that "succeeded" but produced no readable texture is not an import.
        size, _ = m.call(TEX, "get_size", {"texture": ref})
        if not isinstance(size, dict) or not size.get("x"):
            w(f"- **FAILED** `{name}` — no texture after import: {txt[:160]}")
            fail_n += 1
            continue

        applied, _ = m.call(OBJ, "set_properties",
                            {"instance": ref, "values": json.dumps(UI_TEXTURE_SETTINGS)})
        got, _ = m.call(OBJ, "get_properties",
                        {"instance": ref, "properties": list(UI_TEXTURE_SETTINGS)})
        try:
            back = json.loads(got) if isinstance(got, str) else got
        except Exception:
            back = None
        # Read back and compare. A wrong camelCase name fails SILENTLY — an unverified
        # write is not a write, and a blurry HUD six months from now is the cost.
        drift = [k for k, v in UI_TEXTURE_SETTINGS.items()
                 if not back or back.get(k) != v] if back else list(UI_TEXTURE_SETTINGS)
        if drift:
            w(f"- **FAILED** `{name}` {size['x']}x{size['y']} — settings did not stick: {drift}")
            fail_n += 1
        else:
            w(f"- PASS `{name}` {size['x']}x{size['y']} — 4/4 settings verified")
            ok_n += 1

    saved, _ = m.call(ASSET, "save_assets",
                      {"asset_paths": [f"{game_dir}/{p.stem}.{p.stem}" for p in pngs]})
    w(f"\n- save_assets → {saved}")
    w(f"\n## Verdict\n{ok_n} imported and verified, {fail_n} failed, of {len(pngs)}.")
    w("\n## Rung honesty\n"
      "- **Not a rung.** An imported texture is not a rendered one. This proves the asset "
      "exists with the right settings; it does not prove it looks right in a widget.\n"
      "- Settings are verified by read-back, not assumed — but only the four above. Anything "
      "else on the texture is engine default and unreviewed.")
    rc.close()
    print(f"\nreceipt: {rpath.relative_to(REPO)}")
    return 0 if fail_n == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
