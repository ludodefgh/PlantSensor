# PlantSensor

Autonomous plant sensor (soil moisture, temp/humidity, lux, battery) broadcasting over BLE via BTHome v2. Solo hobbyist project, hardware-heavy — most work here is KiCad schematic/PCB, not code.

For accumulated KiCad process discipline and pcbnew/tooling gotchas (rail-topology-change checklist, library-vs-instance desync, `pcbnew.Save()` diff bomb, IPC-4761 via syntax, etc.), see the `kicad-hardware-design-process` skill — that knowledge is intentionally kept there (reusable across projects) rather than duplicated here.

## Current state (Phase 3)

Two parallel tracks on the AN54LQ-15 module (nRF54L15), replacing an earlier from-scratch custom-module plan (`task.md` — superseded, kept for history only):

- **Host PCB** (`hardware/myco-mini-host-pcb/`, branch `feature/myco-mini-host-pcb`) — sensors/power validation board. **v1.0.0 routed, DRC clean, merged to `main`, already ordered from JLCPCB.** This is the actively current hardware.
- **Breakout firmware** (branches `feature/an54lq15-*`) — BLE/Zigbee/dual-protocol validation on a hand-made breakout board, plus a GATT config mode and a companion Android app. Separate from the host PCB; not yet integrated with it.

`src/` (Arduino/PlatformIO) and `zephyr/` (Zephyr on the XIAO nRF54L15 dev board) are **Phase 1/Phase 2 firmware — both predate the host PCB's power architecture** (no `SENSOR_EN`/switched-rail awareness yet, see issue #19). Don't assume either one runs as-is on the host PCB; firmware for the new power architecture hasn't been written.

There's also a separate, standalone repo for the breakout hardware itself at `/home/ludovic/Documents/Projects/NRF54L_breakout` — not part of this repo.

## Hard rules — do not violate

- **Never run `hardware/myco-mini-host-pcb/scripts/gen_pcb.py` or `gen_schematic.py`.** Both carry a `DO NOT RUN THIS SCRIPT` banner (added 2026-08-23). All placement/layout on the live PCB is done by hand by the user going forward — these scripts' output would clobber that.
- **The user does all PCB placement/routing/layout arrangement personally.** Propose geometry changes and describe exactly what needs to move; don't move it yourself unless explicitly asked to make a specific, scoped edit (e.g. "convert these 3 vias to plugged+capped").
- **Never call `pcbnew.Save()` on the live `.kicad_pcb`.** Even a pure round-trip with zero content changes produces hundreds of lines of diff (KiCad reformats on save). Use surgical exact-string `Edit` calls on the S-expression source, verify with a paren-balance check and a fresh `kicad-cli pcb drc` — every time, not just on request.
- **Preserve existing label/text alignment exactly** when editing the schematic — verify via UUID diff (0 moved is the bar) if unsure whether an edit shifted something incidentally.
- **A library fix (`.kicad_mod`/`.kicad_sym`) does not propagate to already-placed PCB/schematic instances.** Always fix and re-verify both sides — this has silently reverted "confirmed fixed" items before.

## Where things live

- `docs/pcb-design-decisions.md` — the real decision log; check here before trusting any older doc's claim about the current design (e.g. finger pitch, MOSFET orientation) — several have gone stale after a fix landed here but not everywhere else.
- `docs/host-pcb-design-brief.md` — original brief; **historically less reliable than the decision log**, has been caught stale more than once (fixed as found, but treat any specific number here with more skepticism than the decision log).
- `docs/cost-tracking.md` — real, dated prices only (no estimates once a real quote exists). Mirrored to the GitHub wiki (`Cost-Tracking` page) — keep both in sync when updating. Wiki clone lives in the session scratchpad, not in this repo; re-clone `https://github.com/ludodefgh/PlantSensor.wiki.git` if a fresh session needs it. Recurring finding worth remembering: shipping has dominated every small-batch order so far (68–126% of merchandise cost) — batching orders is the actual lever, not further BOM micro-optimization.
- `docs/plant-sensor-ideas.md` — freeform ideas backlog.
- GitHub Issues (`ludodefgh/PlantSensor`) is the task tracker. Close with a comment explaining what was actually verified (file/line or DRC result), not just "done" — several issues have been found already-fixed-on-the-board-but-still-open, or the reverse.
- `hardware/myco-mini-host-pcb/jlcpcb_export/` and the loose per-layer `*.svg` renders are gitignored build artifacts — regenerate, don't try to preserve/diff them.

## Conventions

- Commit messages and PR bodies in whichever language the conversation is in (this project's history is French); code/comments in English following the surrounding file's own style.
- When exporting gerbers or a PCB render, always verify freshness (export mtime vs. board mtime, no edits in between) before handing it over — a stale export has been delivered by mistake before.
