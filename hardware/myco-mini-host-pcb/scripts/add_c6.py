#!/usr/bin/env python3
"""Append C6 (100uF bulk/reservoir cap on VDD_NRF, github issue #18 fix) directly
into the LIVE myco-mini-host-pcb.kicad_sch, surgically - same approach as
add_rgb_led.py, since the user hand-edits this schematic. Device:C is already
embedded in lib_symbols (used by C1-C5), so only a new component instance +
wires + labels are needed, no new lib_symbols entry."""
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
gnd_text = []
NETLIST = []
_gnd_counter = [100]  # start high to avoid colliding with existing #PWR01..23

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
    for num, pd in partdef.pins.items():
        ax, ay = local_to_abs(x, y, pd['x'], pd['y'])
        by_num[num] = (ax, ay, pd['rot'], pd['name'], ref, num)
        pin_blocks.append(f'    (pin "{num}" (uuid "{uid()}"))')
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
    return by_num

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
    _gnd_counter[0] += 1
    ref_p = f"#PWR{_gnd_counter[0]:02d}"
    u = uid()
    inst = f'''  (symbol
    (lib_id "power:GND")
    (at {ex:.2f} {ey:.2f} 0)
    (unit 1)
    (exclude_from_sim no)
    (in_bom yes)
    (on_board yes)
    (dnp no)
    (uuid "{u}")
    (property "Reference" "{ref_p}"
      (at {ex:.2f} {ey+6:.2f} 0)
      (effects (font (size 1.27 1.27)) (hide yes))
    )
    (property "Value" "GND"
      (at {ex:.2f} {ey+4:.2f} 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Footprint" ""
      (at {ex:.2f} {ey:.2f} 0)
      (effects (font (size 1.27 1.27)) (hide yes))
    )
    (pin "1" (uuid "{uid()}"))
    (instances
      (project "{PROJ}"
        (path "/"
          (reference "{ref_p}")
          (unit 1)
        )
      )
    )
  )'''
    instances_text.append(inst)

C_PART = PartDef('Device', 'C', device_lib)

c6 = place(C_PART, "C6", "100uF", 75.0, 60.0, footprint="Capacitor_SMD:C_0805_2012Metric",
           extra_props={"Note": "Bulk reservoir cap near J1 pin9 (VDD_NRF) - absorbs radio TX current transient given rising CR2032 ESR at end of life, complements C3 (100nF, HF bypass) rather than replacing it. See github issue #18 / decision log."})
stub_and_label(c6["1"], "VDD_NRF", label_angle=180)
stub_and_gnd(c6["2"])

live_text = open(SCH_PATH).read()
lines = live_text.split("\n")

# find the insertion point fresh (line right before "(sheet_instances" at col0-with-1-tab)
insert_line = None
for i, l in enumerate(lines):
    if l.strip() == "(sheet_instances" and l.startswith("\t("):
        insert_line = i  # 0-indexed position right before this line
        break
assert insert_line is not None, "could not find sheet_instances anchor"

new_text = "\n".join(instances_text + wires_text + labels_text)
lines[insert_line:insert_line] = new_text.split("\n")

open(SCH_PATH, "w").write("\n".join(lines))
print(f"Inserted C6 (100uF) + GND symbol + wires/labels at line {insert_line}")
print("NETLIST additions:", NETLIST)
