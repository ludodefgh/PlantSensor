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

# ---- Board outline ----
BOARD_W, BOARD_H = 190.0, 115.0
outline = pcbnew.PCB_SHAPE(board)
outline.SetShape(pcbnew.SHAPE_T_RECT)
outline.SetStart(pcbnew.VECTOR2I(mm(0), mm(0)))
outline.SetEnd(pcbnew.VECTOR2I(mm(BOARD_W), mm(BOARD_H)))
outline.SetLayer(pcbnew.Edge_Cuts)
outline.SetWidth(mm(0.15))
board.Add(outline)

# ---- Placement plan (mm), roughly mirroring schematic functional zones ----
# J1/J2 mate with the an54lq-15-breakout's J1 ("GPIO_L") and J4 ("GPIO_R") headers,
# which sit on the breakout PCB as two PARALLEL rows 25.4mm apart center-to-center
# (see docs/host-pcb-design-brief.md sec.2). Same here: same Y (pin 1 aligned),
# X offset by exactly 25.4mm.
J_ROW_X = 20.0
J_ROW_Y = 12.0
PLACEMENT = {
    "J1": (J_ROW_X, J_ROW_Y, 0),
    "J2": (J_ROW_X + 25.4, J_ROW_Y, 0),
    "C3": (J_ROW_X, J_ROW_Y + 55, 0),
    "BT1": (85, 15, 0),
    "C1": (110, 15, 0),
    "Q1": (125, 15, 0),
    "C2": (140, 15, 0),
    "L1": (155, 12, 0),
    "U1": (170, 15, 0),
    "R6": (170, 25, 0),
    "C4": (182, 30, 0),
    "C5": (182, 42, 0),
    "Q2": (155, 30, 0),
    "R5": (155, 40, 0),
    "U2": (80, 45, 0),
    "U3": (110, 45, 0),
    "R1": (80, 60, 0),
    "R2": (95, 60, 0),
    "R3": (140, 55, 0),
    "R4": (140, 70, 0),
    "PROBE1": (165, 60, 0),
    "SW1": (110, 85, 0),
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
