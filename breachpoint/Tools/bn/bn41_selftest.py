"""BN41 selftest — the BindWidget contract, checked with no editor.

    python3 Tools/bn/bn41_selftest.py      # exit 0 = the plan and the C++ agree

The old wbp_plan.py's load-bearing idea, applied to the BN front end: a `bind: True`
widget in the plan MUST be a meta=(BindWidget[Optional]) member on the C++ parent, and
every NON-optional C++ bind must exist in the plan. Desync here is UE's nastiest UI
failure — it compiles green and the screen is simply empty in PIE — so it is caught as a
text-mode error on any machine, before an editor is ever opened.

Also asserts the plan's shape: unique names, parents declared before children, exactly
one root, and every referee box inside the 1280x720 canvas.
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / 'Source' / 'BreachpointNext' / 'UI'

# import the plans without bn11_lib's editor transport firing (it is lazy, so safe)
sys.path.insert(0, str(HERE))
import bn41_frontend_wbps as W  # noqa: E402

BIND_RE = re.compile(r'meta\s*=\s*\(\s*(BindWidgetOptional|BindWidget)\s*\)')
MEMBER_RE = re.compile(r'TObjectPtr<\s*U(\w+)\s*>\s+(\w+)\s*;')


def cpp_binds(header):
    """{'MemberName': (optional?, 'UMGType')} parsed from UPROPERTY(meta=(BindWidget...))."""
    text = (SRC / header).read_text(encoding='utf-8')
    out = {}
    for block in text.split('UPROPERTY(')[1:]:
        m = BIND_RE.search(block.split(')')[0] + ')')
        if not m:
            continue
        mem = MEMBER_RE.search(block)
        if mem:
            out[mem.group(2)] = (m.group(1) == 'BindWidgetOptional', mem.group(1))
    return out


def check(name, header, plan):
    fails = []
    names = [n for (n, *_r) in plan]
    if len(names) != len(set(names)):
        fails.append('duplicate widget names: %s' % sorted({n for n in names if names.count(n) > 1}))
    seen, roots = set(), 0
    for (n, _cls, parent, sprops, _w, _b, _f) in plan:
        if parent is None:
            roots += 1
        elif parent not in seen:
            fails.append('%s declared before its parent %s' % (n, parent))
        seen.add(n)
        layout = (sprops or {}).get('layoutData')
        if layout:
            o, a = layout['offsets'], layout['anchors']['maximum']
            if a == {'x': 0.0, 'y': 0.0}:  # absolute boxes only; stretch is exempt
                if not (0 <= o['left'] <= 1280 and 0 <= o['top'] <= 720
                        and o['left'] + o['right'] <= 1280.01 and o['top'] + o['bottom'] <= 720.01):
                    fails.append('%s box (%g,%g %gx%g) leaves the 1280x720 canvas'
                                 % (n, o['left'], o['top'], o['right'], o['bottom']))
    if roots != 1:
        fails.append('%d roots (want exactly 1)' % roots)

    binds = {n for (n, _c, _p, _s, _w, b, _f) in plan if b}
    cpp = cpp_binds(header)
    for b in sorted(binds - set(cpp)):
        fails.append('plan binds %s but %s declares no BindWidget of that name' % (b, header))
    for member, (optional, _t) in sorted(cpp.items()):
        if not optional and member not in binds:
            fails.append('%s REQUIRES %s and the plan does not create it' % (header, member))
    for member in sorted(binds & set(cpp)):
        cls = next(c for (n, c, *_r) in plan if n == member).rsplit('.', 1)[-1]
        want = cpp[member][1]
        # A member is satisfied by its own class OR by a SUBCLASS of it. Two such pairs
        # exist in this project, and both are real derivations, not fudges:
        #   CommonTextBlock -> UTextBlock
        #   WBP_ButtonDefault_C -> UBRButton  (the measured Menu Row; verified in the editor,
        #                                      GetWidgets reports its parentClass as BRButton)
        DERIVES = {('TextBlock', 'CommonTextBlock'),
                   ('BRButton', 'WBP_ButtonDefault_C')}
        if cls != want and (want, cls) not in DERIVES:
            fails.append('%s is a %s in the plan but a U%s in %s' % (member, cls, want, header))

    tag = 'PASS' if not fails else 'FAIL'
    print('%s  %s: %d widgets, %d binds vs %d C++ binds'
          % (tag, name, len(plan), len(binds), len(cpp)))
    for f in fails:
        print('   - ' + f)
    return not fails


ok = check(W.FRONTEND, 'BNScreen_FrontEnd.h', W.frontend_plan())
ok = check(W.PLAYSETUP, 'BNScreen_PlaySetup.h', W.playsetup_plan()) and ok
sys.exit(0 if ok else 1)
