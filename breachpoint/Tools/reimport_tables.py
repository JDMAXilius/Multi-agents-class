"""BREACHPOINT - DataTable CSV reimport, headless.

The macOS/Python counterpart of Tools/reimport-tables.ps1, whose ImportAssets commandlet
is marked UNPROVEN in its own header. Same contract, different mechanism: this uses the
DataTableFunctionLibrary path the ue-editor skill documents (SKILL.md section 5).

    UnrealEditor-Cmd Breachpoint.uproject -run=pythonscript \
        -script="Tools/reimport_tables.py DT_Weapons" -stdout -unattended -nosplash

Content/Data/DT_*.csv is the SOURCE; the matching .uasset is its projection. This
regenerates the projection. A row struct that gained a column in C++ but was never
reimported leaves the live table silently missing that column -- the C++ reads the default
and nothing anywhere says so, which is exactly the failure this script exists to close.

Exits non-zero if any table fails, so a caller can gate on it.
"""
import os
import sys

import unreal

CONTENT_DATA = unreal.Paths.project_content_dir() + "Data/"


def reimport(table_name):
    """Refill one DT_<name> from Content/Data/DT_<name>.csv. Returns True on success."""
    csv_path = CONTENT_DATA + table_name + ".csv"
    asset_path = "/Game/Data/" + table_name

    if not os.path.isfile(csv_path):
        unreal.log_error("MISSING CSV: %s" % csv_path)
        return False

    table = unreal.EditorAssetLibrary.load_asset(asset_path)
    if table is None:
        unreal.log_error("MISSING TABLE: %s" % asset_path)
        return False

    # Returns False on a row-struct/schema mismatch -- the case that matters, because it is
    # the one that otherwise lands as a green run against a stale table.
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, csv_path):
        unreal.log_error(
            "REIMPORT FAILED: %s - row struct/schema mismatch. Check the row struct in "
            "Source/Breachpoint/Data/BRDataRows.h against the CSV header." % table_name)
        return False

    if not unreal.EditorAssetLibrary.save_asset(asset_path):
        unreal.log_error("SAVE FAILED: %s reimported but did not write to disk." % asset_path)
        return False

    unreal.log("REIMPORTED: %s (%d rows)" % (table_name, len(table.get_row_names())))
    return True


def main():
    # Named tables only, by default NONE: a reimport re-saves the .uasset even when the CSV
    # did not change, and law 7 gives each binary asset one owner per ticket. Touching six
    # tables to land a change to one is exactly the collateral that law exists to prevent.
    #   -script="Tools/reimport_tables.py DT_Weapons"        one table
    #   -script="Tools/reimport_tables.py ALL"               every table, deliberately
    args = [a for a in sys.argv[1:] if not a.startswith("-")]

    if not args:
        unreal.log_error(
            "Name the tables to reimport, e.g. 'DT_Weapons', or 'ALL' to mean every one. "
            "Reimporting everything by default would re-save assets this change does not own.")
        return 1

    if args == ["ALL"]:
        names = sorted(
            os.path.splitext(f)[0]
            for f in os.listdir(CONTENT_DATA)
            if f.startswith("DT_") and f.endswith(".csv")
        )
    else:
        names = args

    if not names:
        unreal.log_error("No Content/Data/DT_*.csv found. Nothing to reimport.")
        return 1

    failed = [n for n in names if not reimport(n)]

    unreal.log("== REIMPORT SUMMARY == %d/%d tables" % (len(names) - len(failed), len(names)))
    if failed:
        unreal.log_error("FAILED: %s" % ", ".join(failed))
        return 1
    return 0


sys.exit(main())
