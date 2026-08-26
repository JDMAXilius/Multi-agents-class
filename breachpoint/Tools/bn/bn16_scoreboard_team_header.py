"""BN16 — WBP_BNScreen_Scoreboard: the two team-ledger header readouts.

WHY THIS SCRIPT EXISTS. `UBNScreen_Scoreboard` declares `MyTeamScoreText` and
`EnemyTeamScoreText` as BindWidgetOptional, and the WBP places neither. Because both binds
are Optional the asset compiles clean, no log line fires, and `RefreshTeamScores` takes its
opening early-out forever:

    if (!MyTeamScoreText && !EnemyTeamScoreText) { return; }

so BN16's team-score header cannot render in any match, silently. This is the ONLY new asset
obligation in the whole BN16 packet. Evidence and the read-back live in
`Tools/bn/bn16_audit_team_ui.py`.

PLACEMENT, and where the numbers come from. BoardCanvas is absolute top-left offsets — the
idiom every other leaf on this board already uses. The occupied rects in that band, read off
the live tree (see OCCUPIED below), leave x 465..900 free between the column headers and the
table's top rule. My side goes FIRST and leftmost, aligned to the table's own left edge at
x=465; theirs sits beside it. Font 26 is BannerText's size — the board's other match-level
readout — not a new type scale invented here.

NO COLOUR IS STORED. Both `SetColorAndOpacity` calls live in `BNScreen_Scoreboard.cpp`
(Ally for mine, Threat for theirs). The audit established that this board storing no leaf
colour is exactly what makes the C++ the only tint authority; these two do not become the
exception, and ASSET-RULES §5 forbids typing a hex into a WBP regardless.

BOTH START COLLAPSED. `RefreshTeamScores` sets visibility on every call, but the stored
default is what an FFA board shows before the first Refresh — and a placeholder "Text Block"
flashing on a free-for-all scoreboard is precisely the "no team strip" the OFF-case box
promises. Collapsed makes that structural rather than dependent on refresh timing, and
matches the header's own comment ("both Collapsed outside teams mode").

Idempotent: `ensure()` skips a widget already in the tree and every write is a fixed target.

    python3 Tools/bn/bn16_scoreboard_team_header.py             # needs the live editor
    python3 Tools/bn/bn16_scoreboard_team_header.py --selftest  # geometry only, no editor
"""
import json
import sys

import bn11_lib as B

WBP = 'WBP_BNScreen_Scoreboard'
CANVAS = '/Game/BN/UI/%s.%s:WidgetTree.BoardCanvas' % (WBP, WBP)
TEXT = '/Script/CommonUI.CommonTextBlock'
FONT_SIZE = 26.0


def topleft(x, y, w, h):
    return {"offsets": {"left": x, "top": y, "right": w, "bottom": h},
            "anchors": {"minimum": {"x": 0.0, "y": 0.0}, "maximum": {"x": 0.0, "y": 0.0}},
            "alignment": {"x": 0.0, "y": 0.0}}


# (x, y, w, h) — mine first, and to the left of theirs. That order is the whole point.
PLAN = [
    ('MyTeamScoreText',    465.0, 141.0, 80.0, 32.0),
    ('EnemyTeamScoreText', 553.0, 141.0, 80.0, 32.0),
]

# Read off the live BoardCanvas tree on 26 Aug — the neighbours these two must not collide
# with. Used only by --selftest, which is the one check available with no editor running.
OCCUPIED = [
    ('BannerText',       100.0,  34.0, 700.0, 33.0),
    ('HeaderRule',       100.0,  67.0, 1059.0, 2.0),
    ('ColKills',         900.0, 157.0, 100.0, 15.0),
    ('ColDeaths',       1066.5, 157.0, 100.0, 15.0),
    ('HeaderRuleStrong', 465.0, 173.0, 694.0, 2.0),
    ('ListTopRule',      465.0, 191.0, 693.0, 1.0),
    ('RowContainer',     465.0, 202.0, 694.0, 272.0),
]


def _overlaps(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def selftest():
    """The placement is the one judgement call in this file — so it is the one thing checked.

    Catches a rect that would land on top of a neighbour, and the ordering law (mine LEFT of
    theirs) that a careless edit to PLAN would silently invert.
    """
    rects = {n: (x, y, w, h) for n, x, y, w, h in PLAN}
    for name, rect in rects.items():
        for other, ox, oy, ow, oh in OCCUPIED:
            assert not _overlaps(rect, (ox, oy, ow, oh)), \
                '%s %s overlaps %s' % (name, rect, other)
    a, b = rects['MyTeamScoreText'], rects['EnemyTeamScoreText']
    assert not _overlaps(a, b), 'the two readouts overlap each other'
    assert a[0] < b[0], 'MY side must be LEFT of theirs — the relative-presentation law'
    print('selftest OK: %d placements, no collision, mine left of theirs' % len(PLAN))


def main():
    for display, x, y, w, h in PLAN:
        print('== %s' % display)
        info = B.ensure(WBP, display, TEXT, CANVAS, -1)
        widget, slot = info['widget'], info['slot']

        # font_sized reads the widget's OWN SlateFontInfo back and changes only Size — a
        # partial {"size": N} write drops the typeface and the font object with it.
        wprops = {"visibility": "Collapsed", "font": B.font_sized(widget, FONT_SIZE)}
        sprops = {"layoutData": topleft(x, y, w, h), "bAutoSize": False, "zOrder": 0}

        print('  before w:', B.get(widget, list(wprops)))
        print('  before s:', B.get(slot, list(sprops)))
        print('  set    w:', B.setp(widget, wprops))
        print('  set    s:', B.setp(slot, sprops))
        print('  after  w:', B.get(widget, list(wprops)))
        print('  after  s:', B.get(slot, list(sprops)))

    print('== compile + save')
    print(B.compile_and_save(WBP))
    print('== readback')
    print(B.widgets(WBP)[1])


if __name__ == '__main__':
    if '--selftest' in sys.argv:
        selftest()
    else:
        selftest()   # geometry is asserted before anything touches the asset
        main()
