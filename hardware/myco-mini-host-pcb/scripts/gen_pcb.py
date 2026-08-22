#!/usr/bin/env python3
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
# Captured from the user's validated manual placement milestone (2026-08-11) via
# a read-only pcbnew.LoadBoard() extraction of myco-mini-host-pcb.kicad_pcb -
# supersedes the 2026-08-09 capture below. J1/J2 mate with the an54lq-15-breakout's
# J1 ("GPIO_L") / J4 ("GPIO_R") headers, 25.4mm apart center-to-center (see
# docs/host-pcb-design-brief.md sec.2) - still respected in this captured layout.
# NOTE: D1/R7/R8/R9/C6 are NOT yet in netlist_export.json (they were added to the
# live .kicad_sch via one-off surgical scripts - add_rgb_led.py, add_c6.py - never
# folded into gen_schematic.py's main PARTS/placement logic). Their PLACEMENT
# entries below are captured for reference but a real run of this script won't
# place them until gen_schematic.py + netlist_export.json know about them too.
PLACEMENT = {
    "BT1": (77.4000, 43.8500, 90),
    "C1": (76.3500, 41.5000, 0),
    "C2": (71.1000, 66.2000, 0),
    "C3": (68.0500, 45.6050, 90),
    "C4": (79.0500, 66.8000, 90),
    "C5": (81.6000, 66.8000, 90),
    "C6": (70.9750, 45.4300, 90),
    "D1": (96.9000, 63.1750, 90),
    "J1": (65.0500, 26.0600, 0),
    "J2": (90.4500, 26.0600, 0),
    "L1": (75.4256, 62.8500, 0),
    "PROBE1": (69.6125, 89.7125, 0),
    "Q1": (76.3000, 45.2000, 90),
    "Q2": (69.9500, 76.0500, 0),
    "R1": (60.9000, 28.1500, 0),
    "R2": (61.0000, 41.6000, 0),
    "R3": (66.0500, 74.3500, 180),
    "R4": (65.8000, 77.0000, 180),
    "R5": (74.2500, 75.9500, -90),
    "R6": (71.4750, 59.3000, 180),
    "R7": (96.9000, 57.7750, 90),
    "R8": (94.9000, 57.6750, 90),
    "R9": (99.0750, 57.8000, 90),
    "SW1": (84.8700, 76.6500, -90),
    "U1": (75.3956, 66.6000, 180),
    "U2": (57.9000, 35.3200, 90),
    "U3": (58.1700, 31.1000, 0),
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
