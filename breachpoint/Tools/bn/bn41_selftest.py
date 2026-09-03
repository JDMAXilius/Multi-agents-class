"""BN41 selftest — the front end's BindWidget contract, with no editor.

    python3 Tools/bn/bn41_selftest.py            # exit 0 = the contract is sound
    python3 Tools/bn/bn41_selftest.py --manifest # also rewrite the bind manifest doc

WHAT THIS CHECKS, AND WHY IT CHANGED (3 Sep).

The original job: a `bind: True` widget in `bn41_frontend_wbps.py`'s plan MUST be a
`meta=(BindWidget[Optional])` member on the C++ parent, and every NON-optional C++ bind
must exist in the plan. Desync there is UE's nastiest UI failure — rung 1 stays green and
the screen is simply EMPTY in PIE — so catching it in text mode, on any machine, was worth
a script.

That check went red on 3 Sep, correctly, aimed at a dead artifact. Between 1 and 3 Sep the
terminal built both screens for real and PIE-verified them (4c1a2eaf): the C++ grew
`ScoreLimitButton` / `TimeLimitButton`, dropped `BackButton` / `BreakdownText`, and moved
from raw UMG to composed components (`UBRButton`, `UBRHighlightButton`, `UBNPromptButton`,
`UBNSettingsPanel`, `UBNProfileBar`, `UBNTeamRoster`, `UBRPageTitle`). THE LIVE ASSETS ARE
THE TRUTH NOW; the plan in `bn41_frontend_wbps.py` is the 1-Sep M1 shape, kept as history.

So the plan comparison is **INFO, never a failure** for a screen listed in `RETIRED_PLANS`
below. Reconstructing that plan to match would mean inventing a widget tree and geometry
the terminal derived from founder crops this session never saw — a green check bought with
a fiction, which is worse than a red one.

What still FAILS the run, because it is still true and still checkable:
  · a screen whose C++ declares NO required binds at all (a header that lost its contract)
  · a duplicate bind name inside one header
  · a bind whose C++ type is not a UMG widget type at all
  · for a NON-retired plan: the full original comparison, unchanged

And it emits the thing an editor session actually wants: the authoritative per-screen bind
manifest (name · required/optional · C++ type), which `CompileWidgetBlueprint` enforces
but never lists. `--manifest` writes it to docs/ui/ue-frontend/FRONTEND-BIND-MANIFEST.md.
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


# The screens whose LIVE ASSET is the truth: their plan comparison is INFO, not a gate.
# Removing a screen from this set re-arms the full original check for it.
RETIRED_PLANS = {W.FRONTEND, W.PLAYSETUP}

SCREENS = [
    (W.FRONTEND, 'BNScreen_FrontEnd.h', W.frontend_plan),
    (W.PLAYSETUP, 'BNScreen_PlaySetup.h', W.playsetup_plan),
]


def contract_check(name, header, cpp):
    """What is still true and still checkable with no editor."""
    fails = []
    if not cpp:
        fails.append('%s declares NO BindWidget members — the screen lost its contract' % header)
    if not any(not optional for optional, _t in cpp.values()):
        fails.append('%s declares no REQUIRED bind — nothing forces the asset to carry anything' % header)
    for member, (_optional, t) in sorted(cpp.items()):
        # A bind must be a widget. A non-widget member with the meta tag never binds and
        # fails at asset load, which is exactly the class of bug this file exists for.
        if not (t.endswith('Button') or t.endswith('Text') or t.endswith('TextBlock')
                or t.endswith('Image') or t.endswith('Bar') or t.endswith('Panel')
                or t.endswith('Roster') or t.endswith('Title') or t.endswith('Stack')
                or t.endswith('Box') or t.endswith('Widget') or t.endswith('Border')
                or t.endswith('Prompt') or t.endswith('View') or t.endswith('Overlay')):
            fails.append('%s: %s is a U%s — not recognisably a widget type' % (header, member, t))
    req = sorted(m for m, (o, _t) in cpp.items() if not o)
    opt = sorted(m for m, (o, _t) in cpp.items() if o)
    print('%s  %s: %d binds (%d required, %d optional)'
          % ('PASS' if not fails else 'FAIL', name, len(cpp), len(req), len(opt)))
    for f in fails:
        print('   - ' + f)
    return not fails


def manifest_lines():
    out = ['# Front end — the authoritative BindWidget manifest',
           '',
           '> GENERATED by `Tools/bn/bn41_selftest.py --manifest` from the C++ headers.',
           "> This is what `CompileWidgetBlueprint` enforces and never lists: a WBP missing a",
           '> REQUIRED name fails at asset load, with rung 1 still green and the screen empty',
           '> in PIE. Re-generate after any header change; never hand-edit.', '']
    for name, header, _planf in SCREENS:
        cpp = cpp_binds(header)
        out += ['## `%s`  —  `%s`' % (name, header), '',
                '| Bind name | Required | C++ type |', '|---|---|---|']
        for member, (optional, t) in sorted(cpp.items(), key=lambda kv: (kv[1][0], kv[0])):
            out.append('| `%s` | %s | `U%s` |'
                       % (member, 'no (optional)' if optional else '**YES**', t))
        out.append('')
    return out


ok = True
for name, header, planf in SCREENS:
    cpp = cpp_binds(header)
    ok = contract_check(name, header, cpp) and ok
    if name in RETIRED_PLANS:
        # INFO only — see the module docstring. Printed rather than dropped: the delta is
        # the honest record of how far the shipped screen moved past the M1 fallback.
        plan_binds = {n for (n, _c, _p, _s, _w, b, _f) in planf() if b}
        gone = sorted(plan_binds - set(cpp))
        added = sorted(m for m, (o, _t) in cpp.items() if not o and m not in plan_binds)
        print('      plan RETIRED (live asset is truth) — fallback drift: '
              '%d binds it still names that the C++ dropped %s, '
              '%d required binds the C++ gained %s'
              % (len(gone), gone or '[]', len(added), added or '[]'))
    else:
        ok = check(name, header, planf()) and ok

if '--manifest' in sys.argv:
    out = HERE.parents[1] / 'docs' / 'ui' / 'ue-frontend' / 'FRONTEND-BIND-MANIFEST.md'
    out.write_text('\n'.join(manifest_lines()) + '\n', encoding='utf-8')
    print('wrote %s' % out.relative_to(HERE.parents[2]))

sys.exit(0 if ok else 1)
