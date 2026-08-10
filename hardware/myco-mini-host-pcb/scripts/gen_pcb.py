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
# Captured from the user's validated manual placement milestone (2026-08-09) via
# a read-only pcbnew.LoadBoard() extraction of myco-mini-host-pcb.kicad_pcb.
BOARD_ORIGIN_X, BOARD_ORIGIN_Y = 44.975, 13.985
BOARD_W, BOARD_H = 81.95, 115.15
outline = pcbnew.PCB_SHAPE(board)
outline.SetShape(pcbnew.SHAPE_T_RECT)
outline.SetStart(pcbnew.VECTOR2I(mm(BOARD_ORIGIN_X), mm(BOARD_ORIGIN_Y)))
outline.SetEnd(pcbnew.VECTOR2I(mm(BOARD_ORIGIN_X + BOARD_W), mm(BOARD_ORIGIN_Y + BOARD_H)))
outline.SetLayer(pcbnew.Edge_Cuts)
outline.SetWidth(mm(0.15))
board.Add(outline)

# ---- Placement plan (mm) ----
# Captured from the user's validated manual placement milestone (2026-08-09) via
# a read-only pcbnew.LoadBoard() extraction of myco-mini-host-pcb.kicad_pcb -
# NOT the original schematic-zone guess. J1/J2 mate with the an54lq-15-breakout's
# J1 ("GPIO_L") / J4 ("GPIO_R") headers, 25.4mm apart center-to-center (see
# docs/host-pcb-design-brief.md sec.2) - still respected in this captured layout.
# Boost cluster (U1/L1/C2/C4/C5) deliberately kept clear of the breakout's antenna
# keepout zone (see decision log / chat: host coords X~72.9-82.4, Y~21.7-25).
PLACEMENT = {
    "BT1": (77.7500, 37.1100, 90),
    "C1": (75.2000, 38.7000, 90),
    "C2": (72.9256, 62.6000, 0),
    "C3": (61.0500, 44.9850, 90),
    "C4": (79.7256, 62.8000, 90),
    "C5": (81.9256, 62.8000, 90),
    "J1": (65.0500, 26.0600, 0),
    "J2": (90.4500, 26.0600, 0),
    "L1": (76.6256, 58.8500, 0),
    "PROBE1": (71.7500, 90.0600, 0),
    "Q1": (79.1500, 41.9000, 90),
    "Q2": (72.5000, 83.6500, 0),
    "R1": (95.0500, 33.8100, 180),
    "R2": (94.9250, 31.3100, 180),
    "R3": (68.5250, 82.5750, 180),
    "R4": (68.5250, 84.2750, 180),
    "R5": (76.8000, 83.5500, -90),
    "R6": (73.0506, 60.4000, 180),
    "SW1": (76.8900, 72.5500, -90),
    "U1": (76.5956, 62.6000, 180),
    "U2": (107.6700, 79.8100, 0),
    "U3": (116.3300, 79.5600, 0),
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
