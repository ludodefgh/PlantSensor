#!/usr/bin/env python3
"""Generate a 2-segment interdigitated capacitive soil-probe footprint for KiCad.

Classic IDC (interdigitated capacitor) topology: comb A's spine runs along the
LEFT edge of each segment with fingers pointing RIGHT; comb B's spine runs
along the RIGHT edge with fingers pointing LEFT. Fingers interleave row-by-row
down the segment height (A, gap, B, gap, A, gap, B...) and each finger spans
nearly the full probe width (stopping just short of the opposite spine), so
neighbouring A/B fingers run parallel and close together along almost their
whole length - that's what actually generates the fringing-field capacitance
through the soil.

(Rotated 90 degrees from the original top/bottom-spine version: total
capacitance is ~equal either way (~2% difference, see chat/decision log), but
with the probe now 60mm long, top/bottom-spine fingers would be long
cantilevered copper strips anchored at only one end - fragile against the
mechanical stress of pushing the probe into soil. Left/right-spine fingers
are bounded by the probe width instead of the segment height, same
electrical performance, sturdier copper.

FINGER_W/GAP widened from the brief's original 0.3/0.15mm to 0.75/0.75mm:
field penetration depth into soil scales with pitch (depth ~ lambda/3,
lambda = 2*pitch) - the original 0.45mm pitch only reached ~0.3mm into the
soil (surface-film sensing, not real bulk moisture). PROBE_WIDTH widened
10mm -> 18mm to compensate the resulting drop in finger count/capacitance
(fewer, coarser fingers need to be longer to keep signal strength up). See
decision log 2.7 for the full depth-vs-capacitance tradeoff table.
"""

PROBE_WIDTH = 18.0       # mm, X extent of each comb segment (widened from 10mm to keep
                         # capacitance up after widening the pitch below - see decision log 2.7)
SPINE_W = 0.5            # mm, width of the left/right bus bar
FINGER_W = 0.75          # mm, widened from 0.3mm for field penetration depth - see decision log 2.7
GAP = 0.75               # mm, widened from the brief's original 0.15mm - see decision log 2.7
PITCH = FINGER_W + GAP   # 0.45mm center-to-center down the height
SEG_HEIGHT = 29.0        # mm, Y extent of one segment (2x29+2=60mm total, per user request)
SEG_GAP = 2.0            # mm, isolation gap between segment1 and segment2
MARGIN_Y = 0.3           # mm, top/bottom margin (within a segment) before fingers start
TIP_GAP = 0.4            # mm, clearance between a finger's free tip and the opposite spine

def gen_comb_pads(pad_a_num, pad_b_num, y_top):
    """Generate two custom pads (comb A = left spine/fingers right, comb B =
    right spine/fingers left) for one segment, interleaved down the height."""
    usable_h = SEG_HEIGHT - 2 * MARGIN_Y
    n_rows = int(usable_h // PITCH)

    finger_len = PROBE_WIDTH - 2 * SPINE_W - TIP_GAP  # nearly the full probe width

    a_prims = []
    b_prims = []
    # Spines (bus bars), full segment height, left/right edges
    a_prims.append(f'        (gr_rect (start 0 {y_top:.3f}) (end {SPINE_W:.3f} {y_top+SEG_HEIGHT:.3f}) (width 0) (fill yes))')
    b_prims.append(f'        (gr_rect (start {PROBE_WIDTH-SPINE_W:.3f} {y_top:.3f}) (end {PROBE_WIDTH:.3f} {y_top+SEG_HEIGHT:.3f}) (width 0) (fill yes))')

    y0row = y_top + MARGIN_Y
    for i in range(n_rows):
        y0 = y0row + i * PITCH
        y1 = y0 + FINGER_W
        if i % 2 == 0:
            # comb A finger: extends right from the left spine
            x0, x1 = SPINE_W, SPINE_W + finger_len
            a_prims.append(f'        (gr_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) (width 0) (fill yes))')
        else:
            # comb B finger: extends left from the right spine
            x0, x1 = PROBE_WIDTH - SPINE_W - finger_len, PROBE_WIDTH - SPINE_W
            b_prims.append(f'        (gr_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) (width 0) (fill yes))')

    n_a = sum(1 for i in range(n_rows) if i % 2 == 0)
    n_b = n_rows - n_a

    # Anchor pad position: center of each comb's spine
    anchor_a = (SPINE_W / 2, y_top + SEG_HEIGHT / 2)
    anchor_b = (PROBE_WIDTH - SPINE_W / 2, y_top + SEG_HEIGHT / 2)

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
    return pad_a, pad_b, n_rows, finger_len

seg1_top = 0.0
seg2_top = SEG_HEIGHT + SEG_GAP

pad1a, pad1b, n1, flen = gen_comb_pads("1", "2", seg1_top)
pad2a, pad2b, n2, _ = gen_comb_pads("3", "4", seg2_top)

total_len = 2 * SEG_HEIGHT + SEG_GAP

fp = f'''(footprint "SOIL_PROBE_2SEG"
    (version 20250114)
    (generator "myco_host_gen")
    (generator_version "1.2")
    (layer "F.Cu")
    (descr "2-segment interdigitated capacitive soil moisture probe, classic IDC comb (left/right spines, interleaved full-width fingers), {GAP}mm finger gap, {FINGER_W}mm finger width (widened from an original 0.3/0.15mm for field penetration depth, see decision log 2.6-2.7). See docs/pcb-design-decisions.md")
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
print(f"segment height {SEG_HEIGHT}mm, total probe length {total_len}mm, {n1} fingers/segment, finger length {flen:.2f}mm (was 27.6mm before rotation)")
