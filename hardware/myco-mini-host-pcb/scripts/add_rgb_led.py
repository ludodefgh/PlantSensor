#!/usr/bin/env python3
"""Append the RGB status LED (D1) + 3 series resistors (R7/R8/R9) directly into
the LIVE myco-mini-host-pcb.kicad_sch, without regenerating/touching anything
else in the file. The user is hand-editing this schematic (see decision log
2.5ter / feedback memory), so this script surgically inserts new text at two
locations (end of lib_symbols, end of the top-level symbol/wire/label list)
rather than rebuilding the file from gen_schematic.py's PLACEMENT logic.
"""
import sys, os, uuid as uuidlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sexpr
from sexpr import A, S

PROJ = "myco-mini-host-pcb"
PROJDIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCH_PATH = f"{PROJDIR}/myco-mini-host-pcb.kicad_sch"
LIBDIR = f"{PROJDIR}/libs"

def uid():
    return str(uuidlib.uuid4())

def load_lib(path):
    return sexpr.parse(open(path).read())[0]

device_lib = load_lib('/usr/share/kicad/symbols/Device.kicad_sym')
conn_lib = load_lib('/usr/share/kicad/symbols/Connector_Generic.kicad_sym')
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

GRID = 1.27
def snap(v):
    return round(round(v / GRID) * GRID, 3)

def local_to_abs(ix, iy, lx, ly):
    return (snap(ix + lx), snap(iy - ly))

_DIRS = {0: (1, 0), 90: (0, -1), 180: (-1, 0), 270: (0, 1)}
def dir_for_angle(deg):
    return _DIRS[deg % 360]

instances_text = []
wires_text = []
labels_text = []
NETLIST = []

def place(partdef, ref, value, x, y, footprint=None, extra_props=None):
    x, y = snap(x), snap(y)
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
    (in_bom yes)
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

# ============================================================
# Read the LIVE schematic to get J1's real current position (read-only)
# ============================================================
live_text = open(SCH_PATH).read()
live_tree = sexpr.parse(live_text)[0]
j1_at = None
for s in sexpr.find_all(live_tree, 'symbol'):
    lib_id = sexpr.find_first(s, 'lib_id')
    if not lib_id:
        continue
    ref = None
    for c in s:
        if isinstance(c, list) and c and sexpr.atom_val(c[0]) == 'property' and sexpr.atom_val(c[1]) == 'Reference':
            ref = sexpr.atom_val(c[2])
    if ref == 'J1':
        at = sexpr.find_first(s, 'at')
        j1_at = (float(sexpr.atom_val(at[1])), float(sexpr.atom_val(at[2])))
assert j1_at is not None, "J1 not found in live schematic"
J1_X, J1_Y = j1_at
print(f"J1 real position read from live file: {J1_X}, {J1_Y}")

CONN17 = PartDef('Connector_Generic', 'Conn_01x17', conn_lib)
R_PART = PartDef('Device', 'R', device_lib)
LED_PART = PartDef('myco_host', 'LED_RGB_5050_CA', myco_lib)

def j1_pin_abs(num):
    pd = CONN17.pins[num]
    ax, ay = local_to_abs(J1_X, J1_Y, pd['x'], pd['y'])
    return (ax, ay, pd['rot'], pd['name'], 'J1', num)

# --- J1 free pins 2/3/4 (P1.09/P1.10/P1.11) -> new GPIO nets for the LED ---
stub_and_label(j1_pin_abs('2'), "P1_09_LED_R")
stub_and_label(j1_pin_abs('3'), "P1_10_LED_G")
stub_and_label(j1_pin_abs('4'), "P1_11_LED_B")

# --- RGB LED + series resistors, placed in empty space below existing layout ---
d1 = place(LED_PART, "D1", "LED_RGB_5050_CA", 380, 250,
           footprint="myco_host:LED_SMD5050-6P",
           extra_props={"LCSC": "C2843868",
                        "Note": "Common anode ASSUMED (pinout not verified against datasheet) - see decision log"})
stub_and_label(d1["2"], "VOUT_3V3", label_angle=180)  # common anode pin A (x3)
stub_and_label(d1["4"], "VOUT_3V3", label_angle=180)
stub_and_label(d1["6"], "VOUT_3V3", label_angle=180)
stub_and_label(d1["1"], "LED_G_NODE")   # G cathode
stub_and_label(d1["3"], "LED_R_NODE")   # R cathode
stub_and_label(d1["5"], "LED_B_NODE")   # B cathode

r7 = place(R_PART, "R7", "220", 350, 230, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "LED red channel series resistor - value assumes ~3.3V rail, verify against chosen LED's Vf/If"})
stub_and_label(r7["1"], "LED_R_NODE", label_angle=180)
stub_and_label(r7["2"], "P1_09_LED_R")

r8 = place(R_PART, "R8", "330", 350, 250, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "LED green channel series resistor - value assumes ~3.3V rail, verify against chosen LED's Vf/If"})
stub_and_label(r8["1"], "LED_G_NODE", label_angle=180)
stub_and_label(r8["2"], "P1_10_LED_G")

r9 = place(R_PART, "R9", "330", 350, 270, footprint="Resistor_SMD:R_0603_1608Metric",
           extra_props={"Note": "LED blue channel series resistor - value assumes ~3.3V rail, verify against chosen LED's Vf/If"})
stub_and_label(r9["1"], "LED_B_NODE", label_angle=180)
stub_and_label(r9["2"], "P1_11_LED_B")

# ============================================================
# Splice into the live file text at two precise, verified line boundaries
# (found via paren-depth scan - see chat transcript): do NOT touch anything
# else in the file.
# ============================================================
lines = live_text.split("\n")

# Insert LATER point first so the earlier point's line number (verified
# against the pristine file) is still valid when we use it.
TOP_LEVEL_CLOSE_LINE = 8219  # 1-indexed, verified: '\t)' closing the last top-level symbol, right before (sheet_instances
new_top_level_text = "\n".join(instances_text + wires_text + labels_text)
insert_at2 = TOP_LEVEL_CLOSE_LINE  # 0-indexed position to insert before == (1-indexed line) - 1 + 1
lines[insert_at2:insert_at2] = new_top_level_text.split("\n")  # split so list-index stays 1:1 with line numbers

LIB_SYMBOLS_CLOSE_LINE = 2975  # 1-indexed, verified: '\t)' closing lib_symbols
led_symbol_text = LED_PART.embed_text()
insert_at = LIB_SYMBOLS_CLOSE_LINE - 1  # 0-indexed position to insert before
lines[insert_at:insert_at] = led_symbol_text.split("\n")  # split so list-index stays 1:1 with line numbers

out_text = "\n".join(lines)
open(SCH_PATH, "w").write(out_text)
print(f"Inserted 1 new lib_symbols entry (LED_RGB_5050_CA) and {len(instances_text)} component instances, "
      f"{len(wires_text)} wires, {len(labels_text)} labels into {SCH_PATH}")
print("NETLIST additions:", NETLIST)
