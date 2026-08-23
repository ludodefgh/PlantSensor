#!/usr/bin/env python3
# =============================================================================
#  *** DO NOT RUN THIS SCRIPT ***            (banner added 2026-08-23)
# =============================================================================
#  myco-mini-host-pcb.kicad_pcb is now the SOURCE OF TRUTH. This generator
#  builds a board from an EMPTY board object, so running it would wipe the
#  user's own placement AND all hand-routed copper - including the soil-probe
#  routing rework (SEG2 moved to B.Cu, backside ground plane pulled off the
#  blade) that exists nowhere else.
#
#  The data below (board outline, U2 thermal cutout, PLACEMENT) is a READ-ONLY
#  CAPTURE of the live board, refreshed 2026-08-23. It documents what IS there;
#  it is not a plan to re-impose.
#
#  To change the PCB: edit it in the KiCad GUI, or script against the live file
#  with pcbnew.LoadBoard(...) + targeted edits + board.Save(), never
#  CreateEmptyBoard(). Snapshot first, and re-run `kicad-cli pcb drc` after.
# =============================================================================
import json
import os
import pcbnew

PROJDIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MYCOLIB = f"{PROJDIR}/libs/myco_host.pretty"
STD_CONN = "/usr/share/kicad/footprints/Connector_PinSocket_2.54mm.pretty"
STD_R = "/usr/share/kicad/footprints/Resistor_SMD.pretty"
STD_C = "/usr/share/kicad/footprints/Capacitor_SMD.pretty"
STD_L = "/usr/share/kicad/footprints/Inductor_SMD.pretty"

data = json.load(open(f"{PROJDIR}/netlist_export.json"))
components = data["components"]
connections = data["connections"]

def mm(v):
    return pcbnew.FromMM(v)

def lib_dir_for(footprint_ref, fp_name):
    if fp_name.startswith("PinSocket"):
        return STD_CONN
    if fp_name.startswith("R_0603") or fp_name.startswith("R_0805"):
        return STD_R
    if fp_name.startswith("C_0603") or fp_name.startswith("C_0805"):
        return STD_C
    if fp_name.startswith("L_"):
        return STD_L
    return MYCOLIB

board = pcbnew.CreateEmptyBoard()

# Global min clearance must accommodate the soil probe's deliberate 0.15mm
# finger gap (docs/host-pcb-design-brief.md sec.5). Default netclass rule
# (0.2mm) would flag it as a DRC error even though it's intentional and within
# JLCPCB's real capability. Set to 0.127mm (5mil, standard fab minimum) for margin.
_design_settings = board.GetDesignSettings()
_design_settings.m_MinClearance = mm(0.127)
_design_settings.m_NetSettings.GetDefaultNetclass().SetClearance(mm(0.127))

# ---- Board outline ----
# Captured from the user's validated manual placement milestone (2026-08-11) via
# a read-only pcbnew.LoadBoard() extraction of myco-mini-host-pcb.kicad_pcb.
# Irregular symmetric hexagon (not a rect) - symmetric about the J1/J2 centerline
# (X=77.75mm), tapering toward PROBE1 at the bottom. See decision log 2.11 for how
# this was derived (per-band half-width needed to clear every footprint + 1.5mm
# margin, verified programmatically before applying - NOT hand-drawn numbers).
OUTLINE_POINTS_MM = [
    (91.250, 151.822495), (91.250, 89.475), (103.28, 71.03), (104.48, 20.5),
    (51.02, 20.5), (52.22, 71.03), (64.6, 89.575), (64.125, 151.756963), (77.75, 158.9),
]
outline = pcbnew.PCB_SHAPE(board)
outline.SetShape(pcbnew.SHAPE_T_POLY)
poly = pcbnew.SHAPE_POLY_SET()
poly.NewOutline()
for x, y in OUTLINE_POINTS_MM:
    poly.Append(pcbnew.VECTOR2I(mm(x), mm(y)))
outline.SetPolyShape(poly)
outline.SetLayer(pcbnew.Edge_Cuts)
outline.SetWidth(mm(0.15))
board.Add(outline)

# ---- U2 (SHT41) thermal-relief slot (decision log 2.11) ----
# Originally 3 simple closed rectangles (my first pass); the user later redrew it
# by hand with rounded corners (more realistic to an actual end-mill path) -
# captured here as SEGMENT/ARC pairs exactly as found on the live board
# (2026-08-11), not the original rectangles. (kind, x1,y1,x2,y2) for SEGMENT;
# (kind, x1,y1, midx,midy, x2,y2) for ARC (start,mid,end - KiCad's own arc form).
U2_CUTOUT_WIDTH_MM = 0.05
U2_CUTOUT_SHAPES = [
    ("SEG", 54.700, 34.850, 54.700, 37.650),
    ("ARC", 54.700, 34.850, 54.993, 34.143, 55.700, 33.850),
    ("ARC", 55.700, 38.650, 54.993, 38.357, 54.700, 37.650),
    ("SEG", 55.700, 38.650, 60.300, 38.650),
    ("SEG", 55.950, 33.850, 55.700, 33.850),
    ("SEG", 55.950, 37.125, 55.950, 33.850),
    ("ARC", 56.950, 38.125, 56.243, 37.832, 55.950, 37.125),
    ("SEG", 59.050, 38.125, 56.950, 38.125),
    ("SEG", 60.050, 33.850, 60.050, 37.125),
    ("ARC", 60.050, 37.125, 59.757, 37.832, 59.050, 38.125),
    ("SEG", 60.300, 33.850, 60.050, 33.850),
    ("ARC", 60.300, 33.850, 61.007, 34.143, 61.300, 34.850),
    ("ARC", 61.300, 37.650, 61.007, 38.357, 60.300, 38.650),
    ("SEG", 61.300, 37.650, 61.300, 34.850),
]
for shape in U2_CUTOUT_SHAPES:
    kind = shape[0]
    s = pcbnew.PCB_SHAPE(board)
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(mm(U2_CUTOUT_WIDTH_MM))
    if kind == "SEG":
        _, x1, y1, x2, y2 = shape
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
        s.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
    else:
        _, x1, y1, mx, my, x2, y2 = shape
        s.SetShape(pcbnew.SHAPE_T_ARC)
        s.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
        s.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
        s.SetArcGeometry(pcbnew.VECTOR2I(mm(x1), mm(y1)), pcbnew.VECTOR2I(mm(mx), mm(my)), pcbnew.VECTOR2I(mm(x2), mm(y2)))
    board.Add(s)

# ---- Placement plan (mm) ----
# READ-ONLY CAPTURE of the live board as of 2026-08-23, refreshed after the
# user's own placement/routing passes. This is a record of what IS on the
# board, not a plan the script should impose - see the DO NOT RUN banner at
# the top of this file.
# J1/J2 mate with the an54lq-15-breakout's J1 ("GPIO_L") / J4 ("GPIO_R")
# headers, 25.4mm apart center-to-center (docs/host-pcb-design-brief.md sec.2).
PLACEMENT = {
    "BT1": (77.4, 43.85, -90),  # mounted on back side
    "C1": (76.85, 50.4, 0),
    "C2": (71.1, 66.2, 0),
    "C3": (68.05, 45.605, 90),
    "C4": (79.05, 66.8, 90),
    "C5": (81.6, 66.8, 90),
    "C6": (70.975, 45.43, 90),
    "D1": (96.9, 63.175, 90),
    "J1": (65.05, 26.06, 0),
    "J2": (90.45, 26.06, 0),
    "L1": (75.4256, 62.85, 0),
    "PROBE1": (69.6125, 89.7125, 0),
    "Q1": (76.8, 54.1, 90),
    "Q2": (69.95, 76.05, 0),
    "Q3": (60.175, 45.23, -90),
    "Q4": (70.5725, 37.2, 180),
    "R1": (61.0, 31.14, 0),
    "R10": (95.225, 26.06, 0),
    "R11": (74.1, 36.4, 90),
    "R12": (70.6, 34.1, 180),
    "R2": (60.9, 34.2, 0),
    "R3": (68.7, 80.4, -90),
    "R4": (66.2, 80.4, -90),
    "R5": (74.25, 75.95, -90),
    "R7": (96.9, 57.775, 90),
    "R8": (94.9, 57.675, 90),
    "R9": (99.075, 57.8, 90),
    "SW1": (84.795, 76.775, 180),
    "SW2": (97.2, 32.15, -90),
    "U1": (75.3956, 66.6, 180),
    "U2": (56.4, 26.12, 90),
    "U3": (55.7, 32.4, 0),
    "U4": (75.65, 57.95, 0),
}

pad_lookup = {}  # (ref, pad_num) -> PAD object
net_cache = {}

def get_net(name):
    if name in net_cache:
        return net_cache[name]
    n = pcbnew.NETINFO_ITEM(board, name)
    board.Add(n)
    net_cache[name] = n
    return n

placed = {}
for ref, comp in components.items():
    fp_name = comp["footprint"].split(":")[-1]
    lib_dir = lib_dir_for(ref, fp_name)
    fp = pcbnew.FootprintLoad(lib_dir, fp_name)
    if fp is None:
        print("WARNING: could not load footprint", fp_name, "from", lib_dir, "for", ref)
        continue
    fp.SetReference(ref)
    fp.SetValue(comp["value"])
    x, y, rot = PLACEMENT.get(ref, (20, 20, 0))
    fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    if rot:
        fp.SetOrientationDegrees(rot)
    board.Add(fp)
    placed[ref] = fp
    for pad in fp.Pads():
        pad_lookup[(ref, pad.GetNumber())] = pad

missing = []
for ref, num, net_name in connections:
    pad = pad_lookup.get((ref, num))
    if pad is None:
        missing.append((ref, num, net_name))
        continue
    pad.SetNet(get_net(net_name))

print(f"placed {len(placed)} footprints, {len(pad_lookup)} pads, {len(net_cache)} nets")
if missing:
    print("MISSING PAD LINKS:", missing)

out_path = f"{PROJDIR}/myco-mini-host-pcb.kicad_pcb"
board.Save(out_path)
print("wrote", out_path)
