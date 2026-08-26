"""BN11 — additive HUD slots. Idempotent helpers over the in-editor MCP."""
import json, sys, types
from pathlib import Path

# TRANSPORT BINDING (BN16). These four BN11 drivers were written against an `mcp` module that
# lived in the BN11 session scratchpad, and it did NOT survive the git mv into Tools/bn — see
# TICKET_BN11's contract_gap: `Tools/` was outside that packet's owner_path, so the drivers were
# moved without it. As committed, `import mcp` raised ModuleNotFoundError and bn11_lib,
# bn11_killfeed, bn11_death and bn11_matchband were ALL unrunnable. Bound here to the project's
# one committed transport rather than restoring a second copy of it.
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'mcp-ui' / 'gen_ui'))
from mcp import MCP as _MCP

_M = None


def _call(toolset, tool, **kwargs):
    """kwargs -> the transport's arguments dict; returns the parsed returnValue.

    A dict for the tree reads, a JSON *string* for ObjectTools property reads — which is why
    every caller json.loads() what get()/props() hand back. Raises on failure: the transport
    reports a refusal as a None returnValue with the text in `raw`, and a driver that treats
    that as data writes nonsense into a live asset.
    """
    global _M
    if _M is None:                      # lazy: importing this file must not need an editor
        _M = _MCP()
        _M.init()
    v, raw = _M.call(toolset, tool, kwargs)
    if v is None:
        raise RuntimeError('%s.%s failed: %s' % (toolset, tool, raw))
    return v


mcp = types.SimpleNamespace(call=_call)   # keeps every `mcp.call(...)` below unchanged

UMG = 'UMGToolSet.UMGToolSet'
OBJ = 'editor_toolset.toolsets.object.ObjectTools'
AST = 'editor_toolset.toolsets.asset.AssetTools'

def wbp(name):
    return {'refPath': '/Game/BN/UI/%s.%s' % (name, name)}

def widgets(name):
    r = mcp.call(UMG, 'GetWidgetDescription', widgetBlueprint=wbp(name),
                 startWidget=None, maxDepth=-1)
    return {w['widgetName']: w for w in r['widgets']}, r['description']

def ensure(name, display, cls, parent, index=-1):
    """Add the widget only if a widget of that name is not already in the tree."""
    ws, _ = widgets(name)
    if display in ws:
        print('  exists: %s' % display)
        return ws[display]
    r = mcp.call(UMG, 'AddWidget', widgetBlueprint=wbp(name),
                 widgetClass={'refPath': cls}, widgetDisplayName=display,
                 parentWidget={'refPath': parent}, childIndex=index)
    print('  added: %s -> %s' % (display, json.dumps(r)[:200]))
    ws, _ = widgets(name)
    return ws[display]

def props(ref):
    return mcp.call(OBJ, 'list_properties', instance=ref)

def get(ref, names):
    return mcp.call(OBJ, 'get_properties', instance=ref, properties=names)

def setp(ref, values):
    # `values` is a JSON *string* in this tool's schema, not an object. Passing a dict
    # returns False and writes nothing — the silent no-op the toolset warns about.
    return mcp.call(OBJ, 'set_properties', instance=ref, values=json.dumps(values))

def compile_and_save(name):
    c = mcp.call(UMG, 'CompileWidgetBlueprint', widgetBlueprint=wbp(name))
    s = mcp.call(AST, 'save_assets', asset_paths=['/Game/BN/UI/%s' % name])
    return c, s


def font_sized(ref, size):
    """Read the widget's own SlateFontInfo and return it with only Size changed —
    a partial {"size": N} write would drop the typeface and the font object."""
    f = json.loads(get(ref, ['font']))['font']
    f['size'] = size
    return f
