#!/usr/bin/env python3
"""Generate a 2-segment interdigitated capacitive soil-probe footprint for KiCad.

Straight comb pattern (not the "U" shape mentioned in the brief - that reference
design isn't available in this repo, see decision log). Each segment = 2 custom
pads (comb A / comb B), each pad a fused set of finger rectangles on F.Cu.
"""

PROBE_WIDTH = 10.0       # mm, X extent of each comb segment
SPINE_W = 0.5            # mm, width of the solid bus bar down each side
FINGER_W = 0.3           # mm
GAP = 0.15               # mm, per brief sec.5
PITCH = FINGER_W + GAP   # 0.45mm center-to-center
SEG_HEIGHT = 17.0        # mm, Y extent of one segment
SEG_GAP = 2.0            # mm, isolation gap between segment1 and segment2
MARGIN = 0.5             # mm, top/bottom margin inside a segment before fingers start
FINGER_LEN = 4.3         # mm, each comb's fingers reach this far across (leaves ~0.4mm tip gap)

def gen_comb_pads(pad_a_num, pad_b_num, y_top, seg_index):
    """Generate two custom pads (comb A left-fed, comb B right-fed) for one segment.
    y_top = Y coordinate (footprint-local) of the segment's top edge.
    Fingers run horizontally; alternate rows assigned to A / B."""
    usable_h = SEG_HEIGHT - 2 * MARGIN
    n_fingers = int(usable_h // PITCH)

    a_prims = []
    b_prims = []
    # Spine (bus bar) for each comb, full segment height
    a_prims.append(f'        (gr_rect (start 0 {y_top:.3f}) (end {SPINE_W:.3f} {y_top+SEG_HEIGHT:.3f}) (width 0) (fill yes))')
    b_prims.append(f'        (gr_rect (start {PROBE_WIDTH-SPINE_W:.3f} {y_top:.3f}) (end {PROBE_WIDTH:.3f} {y_top+SEG_HEIGHT:.3f}) (width 0) (fill yes))')

    y = y_top + MARGIN
    for i in range(n_fingers):
        y0 = y + i * PITCH
        y1 = y0 + FINGER_W
        if i % 2 == 0:
            # comb A finger: from left spine rightward
            x0, x1 = SPINE_W, SPINE_W + FINGER_LEN
            a_prims.append(f'        (gr_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) (width 0) (fill yes))')
        else:
            # comb B finger: from right spine leftward
            x0, x1 = PROBE_WIDTH - SPINE_W - FINGER_LEN, PROBE_WIDTH - SPINE_W
            b_prims.append(f'        (gr_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) (width 0) (fill yes))')

    # Anchor pad position: center of each comb's spine, vertically centered in segment
    anchor_a = (SPINE_W / 2, y_top + SEG_HEIGHT / 2)
    anchor_b = (PROBE_WIDTH - SPINE_W / 2, y_top + SEG_HEIGHT / 2)

    def relative(prims, ax, ay):
        out = []
        import re
        for p in prims:
            nums = re.findall(r'[-\d.]+', p)
            # gr_rect (start x1 y1) (end x2 y2) ...
            x1, y1, x2, y2 = (float(n) for n in nums[:4])
            out.append(f'        (gr_rect (start {x1-ax:.3f} {y1-ay:.3f}) (end {x2-ax:.3f} {y2-ay:.3f}) (width 0) (fill yes))')
        return out

    a_rel = relative(a_prims, *anchor_a)
    b_rel = relative(b_prims, *anchor_b)

    pad_a = f'''    (pad "{pad_a_num}" smd custom
        (at {anchor_a[0]:.3f} {anchor_a[1]:.3f})
        (size 0.3 0.3)
        (layers "F.Cu")
        (options (clearance outline) (anchor circle))
        (primitives
{chr(10).join(a_rel)}
        )
    )'''
    pad_b = f'''    (pad "{pad_b_num}" smd custom
        (at {anchor_b[0]:.3f} {anchor_b[1]:.3f})
        (size 0.3 0.3)
        (layers "F.Cu")
        (options (clearance outline) (anchor circle))
        (primitives
{chr(10).join(b_rel)}
        )
    )'''
    return pad_a, pad_b, n_fingers

seg1_top = 0.0
seg2_top = SEG_HEIGHT + SEG_GAP

pad1a, pad1b, n1 = gen_comb_pads("1", "2", seg1_top, 1)
pad2a, pad2b, n2 = gen_comb_pads("3", "4", seg2_top, 2)

total_len = 2 * SEG_HEIGHT + SEG_GAP

fp = f'''(footprint "SOIL_PROBE_2SEG"
    (version 20250114)
    (generator "myco_host_gen")
    (generator_version "1.0")
    (layer "F.Cu")
    (descr "2-segment interdigitated capacitive soil moisture probe, straight comb, 0.15mm finger gap, 0.3mm finger width. Draft geometry - see docs/pcb-design-decisions.md")
    (tags "soil moisture capacitive probe interdigitated")
    (property "Reference" "PROBE1"
        (at {PROBE_WIDTH/2:.3f} -3 0)
        (layer "F.SilkS")
        (effects (font (size 1 1) (thickness 0.15)))
    )
    (property "Value" "SOIL_PROBE_2SEG"
        (at {PROBE_WIDTH/2:.3f} {total_len+3:.3f} 0)
        (layer "F.Fab")
        (effects (font (size 1 1) (thickness 0.15)))
    )
    (fp_rect (start -0.2 -0.2) (end {PROBE_WIDTH+0.2:.3f} {total_len+0.2:.3f})
        (layer "F.CrtYd") (width 0.05) (fill none))
    (fp_line (start 0 {seg1_top:.3f}) (end {PROBE_WIDTH:.3f} {seg1_top:.3f}) (layer "F.SilkS") (width 0.1))
    (fp_line (start 0 {seg1_top+SEG_HEIGHT:.3f}) (end {PROBE_WIDTH:.3f} {seg1_top+SEG_HEIGHT:.3f}) (layer "F.SilkS") (width 0.1))
    (fp_line (start 0 {seg2_top:.3f}) (end {PROBE_WIDTH:.3f} {seg2_top:.3f}) (layer "F.SilkS") (width 0.1))
    (fp_line (start 0 {seg2_top+SEG_HEIGHT:.3f}) (end {PROBE_WIDTH:.3f} {seg2_top+SEG_HEIGHT:.3f}) (layer "F.SilkS") (width 0.1))
    (fp_text user "SEG1 (surface) - {n1} fingers"
        (at {PROBE_WIDTH/2:.3f} {seg1_top+SEG_HEIGHT/2:.3f} 90)
        (layer "Cmts.User") (effects (font (size 0.8 0.8) (thickness 0.1))))
    (fp_text user "SEG2 (profondeur) - {n2} fingers"
        (at {PROBE_WIDTH/2:.3f} {seg2_top+SEG_HEIGHT/2:.3f} 90)
        (layer "Cmts.User") (effects (font (size 0.8 0.8) (thickness 0.1))))
{pad1a}
{pad1b}
{pad2a}
{pad2b}
)
'''

import os
out_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "libs", "myco_host.pretty", "SOIL_PROBE_2SEG.kicad_mod")
with open(out_path, "w") as f:
    f.write(fp)
print("wrote", out_path)
print(f"segment height {SEG_HEIGHT}mm, total probe length {total_len}mm, fingers/segment ~{n1}")
