#!/usr/bin/env python3
"""Generate a 2-segment interdigitated capacitive soil-probe footprint for KiCad.

Classic IDC (interdigitated capacitor) topology: comb A's spine runs along the
TOP of each segment with fingers hanging DOWN; comb B's spine runs along the
BOTTOM with fingers pointing UP. Fingers interleave side-by-side across the
width (A, gap, B, gap, A, gap, B...) and each finger spans nearly the full
segment height (stopping just short of the opposite spine), so neighbouring
A/B fingers run parallel and close together along almost their whole length -
that's what actually generates the fringing-field capacitance through the
soil. (An earlier version of this script alternated whole ROWS between combs
instead of interleaving COLUMNS - that gave a comb-shaped footprint with very
little real parallel-edge proximity between A and B. Fixed here.)
"""

PROBE_WIDTH = 10.0       # mm, X extent of each comb segment
SPINE_W = 0.5            # mm, width of the top/bottom bus bar
FINGER_W = 0.3           # mm
GAP = 0.15               # mm, per brief sec.5
PITCH = FINGER_W + GAP   # 0.45mm center-to-center across X
SEG_HEIGHT = 17.0        # mm, Y extent of one segment
SEG_GAP = 2.0            # mm, isolation gap between segment1 and segment2
MARGIN_X = 0.3           # mm, left/right margin before fingers start
TIP_GAP = 0.4            # mm, clearance between a finger's free tip and the opposite spine

def gen_comb_pads(pad_a_num, pad_b_num, y_top):
    """Generate two custom pads (comb A = top spine/fingers down, comb B = bottom
    spine/fingers up) for one segment, interleaved across X."""
    usable_w = PROBE_WIDTH - 2 * MARGIN_X
    n_cols = int(usable_w // PITCH)

    finger_len = SEG_HEIGHT - 2 * SPINE_W - TIP_GAP  # nearly the full segment height

    a_prims = []
    b_prims = []
    # Spines (bus bars), full segment width
    a_prims.append(f'        (gr_rect (start 0 {y_top:.3f}) (end {PROBE_WIDTH:.3f} {y_top+SPINE_W:.3f}) (width 0) (fill yes))')
    b_prims.append(f'        (gr_rect (start 0 {y_top+SEG_HEIGHT-SPINE_W:.3f}) (end {PROBE_WIDTH:.3f} {y_top+SEG_HEIGHT:.3f}) (width 0) (fill yes))')

    x0col = MARGIN_X
    for i in range(n_cols):
        x0 = x0col + i * PITCH
        x1 = x0 + FINGER_W
        if i % 2 == 0:
            # comb A finger: hangs down from the top spine
            y0, y1 = y_top + SPINE_W, y_top + SPINE_W + finger_len
            a_prims.append(f'        (gr_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) (width 0) (fill yes))')
        else:
            # comb B finger: rises up from the bottom spine
            y0, y1 = y_top + SEG_HEIGHT - SPINE_W - finger_len, y_top + SEG_HEIGHT - SPINE_W
            b_prims.append(f'        (gr_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) (width 0) (fill yes))')

    n_a = sum(1 for i in range(n_cols) if i % 2 == 0)
    n_b = n_cols - n_a

    # Anchor pad position: center of each comb's spine
    anchor_a = (PROBE_WIDTH / 2, y_top + SPINE_W / 2)
    anchor_b = (PROBE_WIDTH / 2, y_top + SEG_HEIGHT - SPINE_W / 2)

    def relative(prims, ax, ay):
        import re
        out = []
        for p in prims:
            nums = re.findall(r'[-\d.]+', p)
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
    return pad_a, pad_b, n_cols, finger_len

seg1_top = 0.0
seg2_top = SEG_HEIGHT + SEG_GAP

pad1a, pad1b, n1, flen = gen_comb_pads("1", "2", seg1_top)
pad2a, pad2b, n2, _ = gen_comb_pads("3", "4", seg2_top)

total_len = 2 * SEG_HEIGHT + SEG_GAP

fp = f'''(footprint "SOIL_PROBE_2SEG"
    (version 20250114)
    (generator "myco_host_gen")
    (generator_version "1.1")
    (layer "F.Cu")
    (descr "2-segment interdigitated capacitive soil moisture probe, classic IDC comb (top/bottom spines, interleaved full-length fingers), 0.15mm finger gap, 0.3mm finger width. Draft geometry - see docs/pcb-design-decisions.md")
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
    (fp_text user "SEG1 (surface) - {n1} fingers x {flen:.1f}mm"
        (at {PROBE_WIDTH/2:.3f} {seg1_top+SEG_HEIGHT/2:.3f} 90)
        (layer "Cmts.User") (effects (font (size 0.8 0.8) (thickness 0.1))))
    (fp_text user "SEG2 (profondeur) - {n2} fingers x {flen:.1f}mm"
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
print(f"segment height {SEG_HEIGHT}mm, total probe length {total_len}mm, {n1} fingers/segment, finger length {flen:.2f}mm")
