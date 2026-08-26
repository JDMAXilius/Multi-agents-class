"""BN15 TEAMS -- tag the open level's PlayerStarts Team0/Team1 (law 7: never hand-edited).

Run inside the UE 5.8 editor (Python Editor Script Plugin):

    py "Tools/bn/tag_team_starts.py"

Finds every APlayerStart in the open level, sorts them DETERMINISTICALLY by object
name, and alternates PlayerStartTag Team0/Team1 -- 4/4 for the arena's 8, and an even
split at any other count. ABNGameMode::ChoosePlayerStart reads exactly these tags when
bTeamsEnabled is on; an untagged start (NAME_None) serves anyone, so a level this
script never touched keeps working. Idempotent: the sort is the assignment, so a
second run converges to the same tags and re-audits.

The audit table is the proof, not "the script ran" -- every start is read BACK from
the live editor after the write and printed intent-vs-actual (the setup_r1_testmap
discipline). Exits non-zero if the read-back diffs or the save fails, so a caller can
gate on it.
"""
import sys

import unreal

TEAM_TAGS = ("Team0", "Team1")


# 5.8 prefers subsystems; EditorLevelLibrary still exists but is deprecated.
# Every editor call goes subsystem-first with the library as fallback.
def _subsystem(cls):
    try:
        return unreal.get_editor_subsystem(cls)
    except Exception:
        return None


def _all_actors():
    eas = _subsystem(unreal.EditorActorSubsystem)
    if eas is not None:
        return eas.get_all_level_actors()
    return unreal.EditorLevelLibrary.get_all_level_actors()


def _save_level():
    les = _subsystem(unreal.LevelEditorSubsystem)
    if les is not None:
        return les.save_current_level()
    return unreal.EditorLevelLibrary.save_current_level()


def _player_starts():
    """Every APlayerStart, sorted by OBJECT name -- labels are editable and would make
    two runs disagree; the object name is what makes the alternation deterministic."""
    starts = [a for a in _all_actors() if isinstance(a, unreal.PlayerStart)]
    starts.sort(key=lambda a: a.get_name())
    return starts


def main():
    starts = _player_starts()
    if not starts:
        unreal.log_error("NO PlayerStart in the open level -- nothing to tag. "
                         "Open the arena map first.")
        return 1

    # The write: index parity IS the side. 8 starts land 4/4.
    for index, start in enumerate(starts):
        tag = TEAM_TAGS[index % len(TEAM_TAGS)]
        start.set_editor_property("player_start_tag", unreal.Name(tag))
        unreal.log("tag_team_starts: %-28s -> %s" % (start.get_name(), tag))

    if not _save_level():
        unreal.log_error("SAVE FAILED -- the tags above are in memory only.")
        return 1

    # The read-back audit: intent from the same sort, actual from the live editor.
    unreal.log("== AUDIT: %d PlayerStart(s) ==" % len(starts))
    unreal.log("%-28s | %-8s | %-8s | %s" % ("PLAYERSTART", "INTENT", "ACTUAL", "STATUS"))
    diffs = 0
    for index, start in enumerate(_player_starts()):
        intent = TEAM_TAGS[index % len(TEAM_TAGS)]
        actual = str(start.get_editor_property("player_start_tag"))
        ok = intent == actual
        diffs += 0 if ok else 1
        unreal.log("%-28s | %-8s | %-8s | %s"
                   % (start.get_name(), intent, actual, "OK" if ok else "DIFF"))
    unreal.log("== AUDIT %s: %d/%d tags =="
               % ("PASSED" if diffs == 0 else "FAILED", len(starts) - diffs, len(starts)))
    return 0 if diffs == 0 else 1


sys.exit(main())
