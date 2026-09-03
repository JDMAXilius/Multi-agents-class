"""Observation runs: raise the game mode's default ScoreLimit for THIS editor session only (the
ini stays at 7), so a PIE match runs to its time limit instead of ending on 7 kills.

    py <repo>/Tools/aib/aib_pie_scorelimit.py 200
"""
import sys
import unreal

limit = int(sys.argv[1]) if len(sys.argv) > 1 else 200
for path in ("/Script/BreachpointNext.BNGameMode", "/Game/BN/Core/BP_BNGameMode.BP_BNGameMode_C"):
    cls = unreal.load_class(None, path)
    if cls is None:
        unreal.log("AIBSL: no class at %s" % path)
        continue
    cdo = unreal.get_default_object(cls)
    before = cdo.get_editor_property("score_limit")
    cdo.set_editor_property("score_limit", limit)
    unreal.log("AIBSL: %s score_limit %s -> %s" % (path, before, cdo.get_editor_property("score_limit")))
