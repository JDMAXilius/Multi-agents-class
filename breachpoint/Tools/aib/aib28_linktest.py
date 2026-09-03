"""AIB28: do generated BN_Drop links exist on the open level? A full path from a roof/gantry
EDGE to the open floor directly beyond it can only exist through a generated drop link.

    py <repo>/Tools/aib/aib28_linktest.py
"""
import unreal

NS = unreal.NavigationSystemV1
EXTENT = unreal.Vector(60, 60, 200)


def project(w, v):
    r = NS.project_point_to_navigation(w, v, None, None, EXTENT)
    if isinstance(r, tuple):
        r = r[1] if r[0] else None
    return r


def path(w, a, b):
    p = NS.find_path_to_location_synchronously(w, a, b, None, None)
    if p is None or not p.is_valid():
        return "NO PATH"
    return "%s len %.0f" % ("partial" if p.is_partial() else "FULL", p.get_path_length())


w = unreal.EditorLevelLibrary.get_editor_world()
tests = {
    "drum roof N edge (1700,2380,410) -> floor (1700,2650,10)": ((1700, 2380, 410), (1700, 2650, 10)),
    "drum roof S edge (2300,1620,410) -> floor (2300,1350,10)": ((2300, 1620, 410), (2300, 1350, 10)),
    "gantry W edge (1010,2000,810) -> floor (650,2000,10)": ((1010, 2000, 810), (650, 2000, 10)),
    "gantry N edge (1200,2190,810) -> floor (1200,2550,10)": ((1200, 2190, 810), (1200, 2550, 10)),
    "mezz deck S edge (2000,1210,410) -> floor (2000,900,10)": ((2000, 1210, 410), (2000, 900, 10)),
}
for name, (a, b) in tests.items():
    pa, pb = project(w, unreal.Vector(*a)), project(w, unreal.Vector(*b))
    if pa is None or pb is None:
        unreal.log("AIB28 link %s: off nav (start %s goal %s)" % (name, pa is not None, pb is not None))
    else:
        unreal.log("AIB28 link %s: %s [start z %.0f goal z %.0f]" % (name, path(w, pa, pb), pa.z, pb.z))
