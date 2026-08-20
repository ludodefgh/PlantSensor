#!/usr/bin/env python3
import sys, os, math, uuid as uuidlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sexpr
from sexpr import A, S

PROJ = "myco-mini-host-pcb"
PROJDIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIBDIR = f"{PROJDIR}/libs"

def uid():
    return str(uuidlib.uuid4())

def load_lib(path):
    return sexpr.parse(open(path).read())[0]

device_lib = load_lib('/usr/share/kicad/symbols/Device.kicad_sym')
conn_lib = load_lib('/usr/share/kicad/symbols/Connector_Generic.kicad_sym')
power_lib = load_lib('/usr/share/kicad/symbols/power.kicad_sym')
myco_lib = load_lib(f'{LIBDIR}/myco_host.kicad_sym')

class PartDef:
    def __init__(self, lib_prefix, symbol_name, lib_root):
        blk = None
        for child in lib_root:
            if isinstance(child, list) and child and sexpr.atom_val(child[0]) == 'symbol' and sexpr.atom_val(child[1]) == symbol_name:
                blk = child
        assert blk is not None, f"symbol {symbol_name} not found in lib"
        self.raw = blk
        self.symbol_name = symbol_name
        self.lib_id = f"{lib_prefix}:{symbol_name}"
        self.pins = {}
        for p in sexpr.find_all(blk, 'pin'):
            at = sexpr.find_first(p, 'at')
            length = sexpr.find_first(p, 'length')
            name = sexpr.find_first(p, 'name')
            number = sexpr.find_first(p, 'number')
            x, y, rot = float(sexpr.atom_val(at[1])), float(sexpr.atom_val(at[2])), int(float(sexpr.atom_val(at[3])))
            self.pins[sexpr.atom_val(number[1])] = dict(
                x=x, y=y, rot=rot, length=float(sexpr.atom_val(length[1])), name=sexpr.atom_val(name[1])
            )

    def embed_text(self):
        node = list(self.raw)
        node[1] = S(self.lib_id)
        return sexpr.serialize(node, indent=2)

PARTS = {
    'R': PartDef('Device', 'R', device_lib),
    'C': PartDef('Device', 'C', device_lib),
    'L': PartDef('Device', 'L', device_lib),
    'CONN17': PartDef('Connector_Generic', 'Conn_01x17', conn_lib),
    'GND': PartDef('power', 'GND', power_lib),
    'SHT41': PartDef('myco_host', 'SHT41-AD1B-R2', myco_lib),
    'BH1750': PartDef('myco_host', 'BH1750FVI-TR', myco_lib),
    'AO3401': PartDef('myco_host', 'AO3401A', myco_lib),
    'BATH': PartDef('myco_host', 'CR2032-BS-6-1', myco_lib),
    'DIPSW': PartDef('myco_host', 'EM-04-Q_C501635', myco_lib),
    'XC9145': PartDef('myco_host', 'XC9145B33CMR-G', myco_lib),
    'PROBE': PartDef('myco_host', 'SOIL_PROBE_2SEG', myco_lib),
    'PWRFLAG': PartDef('power', 'PWR_FLAG', power_lib),
}

used_libids = {}
instances_text = []
wires_text = []
labels_text = []
notes_text = []
gnd_text = []

GRID = 1.27

def snap(v):
    return round(round(v / GRID) * GRID, 3)

def local_to_abs(ix, iy, lx, ly):
    return (snap(ix + lx), snap(iy - ly))

_DIRS = {0: (1, 0), 90: (0, -1), 180: (-1, 0), 270: (0, 1)}

def dir_for_angle(deg):
    return _DIRS[deg % 360]

COMPONENTS = {}  # ref -> dict(lib_id, footprint, value)

def place(partdef, ref, value, x, y, footprint=None, extra_props=None, in_bom=True):
    x, y = snap(x), snap(y)
    used_libids[partdef.lib_id] = partdef
    if in_bom and footprint:
        COMPONENTS[ref] = dict(lib_id=partdef.lib_id, footprint=footprint, value=value,
                                lcsc=(extra_props or {}).get("LCSC", ""))
    u = uid()
    props = []
    props.append(f'''    (property "Reference" "{ref}"
      (at {x:.2f} {y-6:.2f} 0)
      (effects (font (size 1.27 1.27)))
    )''')
    props.append(f'''    (property "Value" "{value}"
      (at {x:.2f} {y+6:.2f} 0)
      (effects (font (size 1.27 1.27)))
    )''')
    fp = footprint or ""
    props.append(f'''    (property "Footprint" "{fp}"
      (at {x:.2f} {y:.2f} 0)
      (effects (font (size 1.27 1.27)) (hide yes))
    )''')
    if extra_props:
        for k, v in extra_props.items():
            props.append(f'''    (property "{k}" "{v}"
      (at {x:.2f} {y:.2f} 0)
      (effects (font (size 1.27 1.27)) (hide yes))
    )''')
    pin_blocks = []
    by_num = {}
    by_name = {}
    name_counts = {}
    for num, pd in partdef.pins.items():
        ax, ay = local_to_abs(x, y, pd['x'], pd['y'])
        by_num[num] = (ax, ay, pd['rot'], pd['name'], ref, num)
        name_counts[pd['name']] = name_counts.get(pd['name'], 0) + 1
    for num, pd in partdef.pins.items():
        if pd['name'] and name_counts[pd['name']] == 1:
            by_name[pd['name']] = by_num[num]
        pin_blocks.append(f'    (pin "{num}" (uuid "{uid()}"))')
    abs_pins = {**by_name, **by_num}
    inst = f'''  (symbol
    (lib_id "{partdef.lib_id}")
    (at {x:.2f} {y:.2f} 0)
    (unit 1)
    (exclude_from_sim no)
    (in_bom {"yes" if in_bom else "no"})
    (on_board yes)
    (dnp no)
    (uuid "{u}")
{chr(10).join(props)}
{chr(10).join(pin_blocks)}
    (instances
      (project "{PROJ}"
        (path "/"
          (reference "{ref}")
          (unit 1)
        )
      )
    )
  )'''
    instances_text.append(inst)
    return abs_pins

NETLIST = []  # list of (ref, pin_num, net_name)

def stub_and_label(pin_abs, net_name, stub_len=3.81, label_angle=None):
    ax, ay, rot, pname, ref, num = pin_abs
    NETLIST.append((ref, num, net_name))
    dx, dy = dir_for_angle(rot + 180)
    ex, ey = ax + stub_len * dx, ay + stub_len * dy
    wires_text.append(f'''  (wire
    (pts (xy {ax:.2f} {ay:.2f}) (xy {ex:.2f} {ey:.2f}))
    (stroke (width 0) (type default))
    (uuid "{uid()}")
  )''')
    la = label_angle if label_angle is not None else 0
    labels_text.append(f'''  (label "{net_name}"
    (at {ex:.2f} {ey:.2f} {la})
    (effects (font (size 1.27 1.27)) (justify left bottom))
    (uuid "{uid()}")
  )''')

def stub_and_gnd(pin_abs, stub_len=3.81):
    ax, ay, rot, pname, ref, num = pin_abs
    NETLIST.append((ref, num, "GND"))
    dx, dy = dir_for_angle(rot + 180)
    ex, ey = ax + stub_len * dx, ay + stub_len * dy
    wires_text.append(f'''  (wire
    (pts (xy {ax:.2f} {ay:.2f}) (xy {ex:.2f} {ey:.2f}))
    (stroke (width 0) (type default))
    (uuid "{uid()}")
  )''')
    place_gnd(ex, ey)

_gnd_counter = [0]

def place_gnd(x, y):
    gp = PARTS['GND']
    used_libids[gp.lib_id] = gp
    u = uid()
    _gnd_counter[0] += 1
    ref = f"#PWR{_gnd_counter[0]:02d}"
    inst = f'''  (symbol
    (lib_id "{gp.lib_id}")
    (at {x:.2f} {y:.2f} 0)
    (unit 1)
    (exclude_from_sim no)
    (in_bom yes)
    (on_board yes)
    (dnp no)
    (uuid "{u}")
    (property "Reference" "{ref}"
      (at {x:.2f} {y+6:.2f} 0)
      (effects (font (size 1.27 1.27)) (hide yes))
    )
    (property "Value" "GND"
      (at {x:.2f} {y+4:.2f} 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Footprint" ""
      (at {x:.2f} {y:.2f} 0)
      (effects (font (size 1.27 1.27)) (hide yes))
    )
    (pin "1" (uuid "{uid()}"))
    (instances
      (project "{PROJ}"
        (path "/"
          (reference "{ref}")
          (unit 1)
        )
      )
    )
  )'''
    instances_text.append(inst)

def wire_direct(p1, p2):
    wires_text.append(f'''  (wire
    (pts (xy {p1[0]:.2f} {p1[1]:.2f}) (xy {p2[0]:.2f} {p2[1]:.2f}))
    (stroke (width 0) (type default))
    (uuid "{uid()}")
  )''')

def text_note(txt, x, y, size=1.5):
    notes_text.append(f'''  (text "{txt}"
    (at {x:.2f} {y:.2f} 0)
    (effects (font (size {size} {size})))
    (uuid "{uid()}")
  )''')

# ============================================================
# LAYOUT
# ============================================================

# --- J1: breakout GPIO_L receptacle ---
j1_x, j1_y = 40, 90
j1 = place(PARTS['CONN17'], "J1", "Conn_01x17_Female",
           j1_x, j1_y,
           footprint="Connector_PinSocket_2.54mm:PinSocket_1x17_P2.54mm_Vertical",
           extra_props={"Note": "Breakout J1 (GPIO_L) - mating female header"})
text_note("J1 = recepteur breakout J1 'GPIO_L' (1x17, 2.54mm)", j1_x-20, j1_y-26, 1.8)

j1_nets = {
    "1": "GND", "6": "P1_13_BOOST_EN", "7": "P1_14_SOIL_SW", "8": "GND",
    "9": "VDD_NRF", "12": "P1_04_SOIL_ADC2", "15": "P1_07_SOIL_ADC1",
}
for num, net in j1_nets.items():
    if net == "GND":
        stub_and_gnd(j1[num])
    else:
        stub_and_label(j1[num], net)

# --- J2: breakout GPIO_R (brief's "J4") receptacle ---
j2_x, j2_y = 40, 190
j2 = place(PARTS['CONN17'], "J2", "Conn_01x17_Female",
           j2_x, j2_y,
           footprint="Connector_PinSocket_2.54mm:PinSocket_1x17_P2.54mm_Vertical",
           extra_props={"Note": "Breakout J4 (GPIO_R) - mating female header"})
text_note("J2 = recepteur breakout J4 'GPIO_R' (1x17, 2.54mm)", j2_x-20, j2_y-26, 1.8)

j2_nets = {
    "3": "P0_03_SCL", "4": "P0_02_SDA",
    "14": "P2_03_DIP3", "15": "P2_02_DIP2", "16": "P2_01_DIP1", "17": "P2_00_DIP0",
}
for num, net in j2_nets.items():
    stub_and_label(j2[num], net)

# --- Power: battery holder + reverse polarity protection ---
bt1_x, bt1_y = 130, 40
bt1 = place(PARTS['BATH'], "BT1", "CR2032",
            bt1_x, bt1_y, footprint="myco_host:BAT-TH_CR2032-BS-6-1",
            extra_props={"LCSC": "C70377"})
# pin1 = BAT+ (assumed, verify vs datasheet), pin2 = BAT- (assumed)
stub_and_label(bt1["1"], "BAT_RAW")
stub_and_gnd(bt1["2"])
pf = place(PARTS['PWRFLAG'], "#FLG01", "PWR_FLAG", bt1_x, bt1_y + 15, in_bom=False)
place_gnd(pf["1"][0], pf["1"][1])

c1_x, c1_y = 150, 40
c1 = place(PARTS['C'], "C1", "10uF", c1_x, c1_y, footprint="Capacitor_SMD:C_0805_2012Metric")
stub_and_label(c1["1"], "BAT_RAW", label_angle=180)
stub_and_gnd(c1["2"])

q1_x, q1_y = 175, 45
q1 = place(PARTS['AO3401'], "Q1", "AO3401A", q1_x, q1_y,
           footprint="myco_host:SOT-23_L2.9-W1.3-P1.90-LS2.4-BR",
           extra_props={"LCSC": "C15127", "Note": "Reverse polarity protect: S=BAT_RAW D=VDD_NRF G=GND"})
stub_and_label(q1["S"], "BAT_RAW")
stub_and_label(q1["D"], "VDD_NRF")
stub_and_gnd(q1["G"])

c2_x, c2_y = 200, 40
c2 = place(PARTS['C'], "C2", "10uF", c2_x, c2_y, footprint="Capacitor_SMD:C_0805_2012Metric",
           extra_props={"Note": "Boost input cap, near U1"})
stub_and_label(c2["1"], "VDD_NRF", label_angle=180)
stub_and_gnd(c2["2"])

c3_x, c3_y = 60, 60
c3 = place(PARTS['C'], "C3", "100nF", c3_x, c3_y, footprint="Capacitor_SMD:C_0603_1608Metric",
           extra_props={"Note": "Local VDD_NRF bypass near J1 pin9 (conditional per brief sec.2) - HF filtering, complements C6"})
stub_and_label(c3["1"], "VDD_NRF", label_angle=180)
stub_and_gnd(c3["2"])

c6_x, c6_y = 75, 60
c6 = place(PARTS['C'], "C6", "100uF", c6_x, c6_y, footprint="Capacitor_SMD:C_0805_2012Metric",
           extra_props={"Note": "Bulk reservoir cap near J1 pin9 (VDD_NRF) - absorbs radio TX current transient given rising CR2032 ESR at end of life, complements C3 (100nF, HF bypass) rather than replacing it. See github issue #18 / decision log."})
stub_and_label(c6["1"], "VDD_NRF", label_angle=180)
stub_and_gnd(c6["2"])

# --- Boost converter ---
l1_x, l1_y = 225, 30
l1 = place(PARTS['L'], "L1", "4.7uH", l1_x, l1_y, footprint="Inductor_SMD:L_1008_2520Metric",
           extra_props={"Note": "Shielded inductor recommended (datasheet note)"})
stub_and_label(l1["1"], "VDD_NRF", label_angle=180)

u1_x, u1_y = 250, 45
u1 = place(PARTS['XC9145'], "U1", "XC9145B33CMR-G", u1_x, u1_y,
           footprint="myco_host:SOT-25-5_L2.9-W1.6-P0.95-LS2.8-BR",
           extra_props={"LCSC": "C19261414", "Note": "SOT-25, hand-solderable, in stock LCSC - see decision log 3.2"})
stub_and_label(u1["BAT"], "VDD_NRF", label_angle=180)
stub_and_gnd(u1["2"])
stub_and_label(u1["CE"], "P1_13_BOOST_EN", label_angle=180)
lx_pin = u1["LX"]
wire_direct((lx_pin[0], lx_pin[1]), (l1["2"][0], l1["2"][1]))
NETLIST.append((lx_pin[4], lx_pin[5], "LX_NODE"))
NETLIST.append((l1["2"][4], l1["2"][5], "LX_NODE"))
stub_and_label(u1["VOUT"], "VOUT_3V3")

r6_x, r6_y = 250, 70
r6 = place(PARTS['R'], "R6", "100k", r6_x, r6_y, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "CE pull-down - datasheet: do not leave CE open"})
stub_and_label(r6["1"], "P1_13_BOOST_EN")
stub_and_gnd(r6["2"])

c4_x, c4_y = 280, 40
c4 = place(PARTS['C'], "C4", "10uF", c4_x, c4_y, footprint="Capacitor_SMD:C_0805_2012Metric")
stub_and_label(c4["1"], "VOUT_3V3", label_angle=180)
stub_and_gnd(c4["2"])

c5_x, c5_y = 300, 40
c5 = place(PARTS['C'], "C5", "10uF", c5_x, c5_y, footprint="Capacitor_SMD:C_0805_2012Metric")
stub_and_label(c5["1"], "VOUT_3V3", label_angle=180)
stub_and_gnd(c5["2"])

# --- Soil VCC switch ---
q2_x, q2_y = 330, 45
q2 = place(PARTS['AO3401'], "Q2", "AO3401A", q2_x, q2_y,
           footprint="myco_host:SOT-23_L2.9-W1.3-P1.90-LS2.4-BR",
           extra_props={"LCSC": "C15127", "Note": "Soil probe VCC switch: S=VOUT_3V3 D=SOIL_VCC G=P1_14"})
stub_and_label(q2["S"], "VOUT_3V3")
stub_and_label(q2["D"], "SOIL_VCC")
stub_and_label(q2["G"], "P1_14_SOIL_SW")

r5_x, r5_y = 330, 70
r5 = place(PARTS['R'], "R5", "100k", r5_x, r5_y, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "Gate pull-up, default-off before GPIO init"})
stub_and_label(r5["1"], "VOUT_3V3")
stub_and_label(r5["2"], "P1_14_SOIL_SW")

# --- Sensors ---
u2_x, u2_y = 110, 130
u2 = place(PARTS['SHT41'], "U2", "SHT41-AD1B-R2", u2_x, u2_y,
           footprint="myco_host:DFN-4_L1.5-W1.5-P0.8-TL-EP",
           extra_props={"LCSC": "C7461861", "Note": "Same package/pinout as SHT40, better RH accuracy at extremes - see decision log"})
stub_and_label(u2["SDA"], "P0_02_SDA", label_angle=180)
stub_and_label(u2["SCL"], "P0_03_SCL", label_angle=180)
stub_and_label(u2["VDD"], "VOUT_3V3")
stub_and_gnd(u2["VSS"])
stub_and_gnd(u2["EP"])

u3_x, u3_y = 175, 135
u3 = place(PARTS['BH1750'], "U3", "BH1750FVI-TR", u3_x, u3_y,
           footprint="myco_host:WSOF-6_L2.6-W1.6-P0.50-TL-EP",
           extra_props={"LCSC": "C78960", "Note": "ADDR=GND -> I2C addr 0x23"})
stub_and_label(u3["VCC"], "VOUT_3V3", label_angle=180)
stub_and_gnd(u3["ADDR"])
stub_and_gnd(u3["GND"])
stub_and_label(u3["SDA"], "P0_02_SDA")
stub_and_label(u3["DVI"], "VOUT_3V3")
stub_and_label(u3["SCL"], "P0_03_SCL")
stub_and_gnd(u3["EP"])

r1_x, r1_y = 130, 100
r1 = place(PARTS['R'], "R1", "4.7k", r1_x, r1_y, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "I2C SDA pull-up - moved to VOUT_3V3, see decision log 2.5ter (both I2C devices now on switched rail)"})
stub_and_label(r1["1"], "VOUT_3V3")
stub_and_label(r1["2"], "P0_02_SDA")

r2_x, r2_y = 150, 100
r2 = place(PARTS['R'], "R2", "4.7k", r2_x, r2_y, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "I2C SCL pull-up - moved to VOUT_3V3, see decision log 2.5ter (both I2C devices now on switched rail)"})
stub_and_label(r2["1"], "VOUT_3V3")
stub_and_label(r2["2"], "P0_03_SCL")

# --- Soil probe ---
probe_x, probe_y = 350, 150
probe = place(PARTS['PROBE'], "PROBE1", "SOIL_PROBE_2SEG",
              probe_x, probe_y, footprint="myco_host:SOIL_PROBE_2SEG",
              extra_props={"Note": "PCB copper only - straight interdigitated comb, see decision log"})
stub_and_label(probe["1"], "P1_07_SOIL_ADC1", label_angle=180)  # SEG1_A
stub_and_gnd(probe["2"])                                        # SEG1_B
stub_and_label(probe["3"], "P1_04_SOIL_ADC2")                    # SEG2_A
stub_and_gnd(probe["4"])                                         # SEG2_B

r3_x, r3_y = 300, 130
r3 = place(PARTS['R'], "R3", "220k", r3_x, r3_y, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "Bias resistor, segment 1 - lowered from 1M, see decision log 2.6"})
stub_and_label(r3["1"], "SOIL_VCC")
stub_and_label(r3["2"], "P1_07_SOIL_ADC1")

r4_x, r4_y = 300, 170
r4 = place(PARTS['R'], "R4", "220k", r4_x, r4_y, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "Bias resistor, segment 2 - lowered from 1M, see decision log 2.6"})
stub_and_label(r4["1"], "SOIL_VCC")
stub_and_label(r4["2"], "P1_04_SOIL_ADC2")

# --- DIP switches ---
sw1_x, sw1_y = 430, 190
sw1 = place(PARTS['DIPSW'], "SW1", "EM-04-Q", sw1_x, sw1_y,
            footprint="myco_host:SW-SMD_8P-L10.1-W6.0-P2.54-LS9.8",
            extra_props={"LCSC": "C501635", "Note": "Pin pairing 1-8,2-7,3-6,4-5 verified from EasyEDA symbol geometry"})
stub_and_label(sw1["1"], "P2_00_DIP0", stub_len=3.81)
stub_and_label(sw1["2"], "P2_01_DIP1", stub_len=7.62)
stub_and_label(sw1["3"], "P2_02_DIP2", stub_len=11.43)
stub_and_label(sw1["4"], "P2_03_DIP3", stub_len=15.24)
for n in ["5", "6", "7", "8"]:
    stub_and_gnd(sw1[n])

text_note("Myco Mini - Host PCB (proto validation) - Premier jet, non revise - voir docs/pcb-design-decisions.md", 30, 15, 2.5)
text_note("Pins headers non utilisees dans ce brouillon = laissees non cablees (GPIO libres, voir docs/host-pcb-design-brief.md)", 30, 250, 1.6)

# ============================================================
# ASSEMBLE FILE
# ============================================================

lib_symbols_block = "  (lib_symbols\n" + "\n".join(pd.embed_text() for pd in used_libids.values()) + "\n  )"

sch = f'''(kicad_sch
  (version 20250114)
  (generator "myco_host_gen")
  (generator_version "10.0")
  (uuid "{uid()}")
  (paper "A2")
  (title_block
    (title "Myco Mini - Host PCB (validation proto)")
    (date "2026-08-06")
    (rev "v0.1-draft")
    (comment 1 "Recoit le breakout an54lq-15-breakout - voir docs/host-pcb-design-brief.md")
  )
{lib_symbols_block}
{chr(10).join(instances_text)}
{chr(10).join(wires_text)}
{chr(10).join(labels_text)}
{chr(10).join(notes_text)}
  (sheet_instances
    (path "/"
      (page "1")
    )
  )
  (embedded_fonts no)
)
'''

out_path = f"{PROJDIR}/myco-mini-host-pcb.kicad_sch"
with open(out_path, "w") as f:
    f.write(sch)
print("wrote", out_path, len(sch), "bytes")
print("parts placed:", len(instances_text))

import json
netlist_out = f"{PROJDIR}/netlist_export.json"
with open(netlist_out, "w") as f:
    json.dump({
        "components": COMPONENTS,
        "connections": NETLIST,
    }, f, indent=1)
print("wrote", netlist_out, "components:", len(COMPONENTS), "pin-net links:", len(NETLIST))
