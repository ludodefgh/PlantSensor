# Myco Mini — Prototype History & Legacy Decisions (Proto A / Proto B)

> Migrated from Google Drive (`myco-hardware-design.md` Parts 1–2, `myco-decision-log.md`) on 2026-09-01,
> as part of consolidating all project documentation into this repo. This content predates the
> AN54LQ-15 breakout / host PCB work and has no equivalent elsewhere in the repo — it's kept here
> as frozen historical reference, not as an actively maintained doc.
>
> For current hardware state, see `docs/pcb-design-decisions.md` and `docs/host-pcb-design-brief.md`.
> For open questions and current-session notes, see `docs/plant-sensor-ideas.md`.
> Drive originals have been reduced to pointers back to this repo.

---

## Part 1 — Prototype A: Field Units (XIAO nRF52840 × 2)

These were the two units originally deployed in plants and reporting to Home Assistant, before the
project moved to the AN54LQ-15 breakout track.

### BOM

| Component | Part | Notes |
| :---- | :---- | :---- |
| MCU | Seeed XIAO BLE nRF52840 | USB native, UF2 bootloader, 21×17.5mm — DigiKey SKU 102110448 |
| Soil sensor | Capacitive v2.0 (AliExpress) | Waterproofed with transparent varnish. Draws 5mA continuously when powered — switched off between readings via GPIO D10 |
| Temp/Humidity | SHT40 module I2C | ±0.2°C, ±1.8% RH, addr 0x44 |
| Light | BH1750 module I2C | Lux, ONE_TIME mode, addr 0x23 |
| Battery | CR2032 | 3V nominal, ~220 mAh |
| Battery holder | CR2032 holder | AliExpress or Amazon.ca, ~0.50 CAD |
| Boost converter | Mini 600mA 3.3V (AliExpress) | Input 0.8–3.3V, Iq 13µA |
| Wiring | 30 AWG wire | |
| Enclosure | PLA 3D printed | v1: rectangular; ventilation redesign tracked as issue #4 |

**Total BOM cost:** ~38 CAD/unit

### Power Architecture

```
CR2032+ ──→ Boost VIN
Boost VOUT (3.3V) ──┬──→ XIAO 3V3
                     ├──→ SHT40 VCC
                     └──→ BH1750 VCC
XIAO GPIO D10 ──→ Soil sensor VCC (switched — OFF between readings)
```

No AO3401 on these units — boost converter runs always-on. Soil sensor VCC is the only switched
rail. This was the simpler configuration used for field validation, not the low-power optimized
version (see Proto B).

### GPIO Pinout (XIAO nRF52840)

| Pin | nRF52840 pad | Function |
| :---- | :---- | :---- |
| A1 | P0.03 | Soil sensor AOUT |
| A2 | P0.28 | CR2032 voltage monitoring |
| D4 | P0.04 | I2C SDA (SHT40 + BH1750 shared) |
| D5 | P0.05 | I2C SCL (SHT40 + BH1750 shared) |
| D6 | P1.11 | UART TX (debug serial, 115200 baud) |
| D7 | P1.12 | UART RX (debug serial) |
| D10 | P0.10 | Soil sensor VCC power control |

I2C pull-ups rely on MCU internal pull-ups — confirmed not a meaningful contributor to quiescent
current.

---

## Part 2 — Prototype B: Battery / Boost Test Unit (XIAO nRF52840 × 1)

Used to characterize battery life and validate the AO3401 boost-cutoff approach that later informed
the XC9145 true-load-disconnect choice on the host PCB.

### BOM (differences from Proto A)

| Component | Part | Notes |
| :---- | :---- | :---- |
| MCU | Seeed XIAO BLE nRF52840 | Connected directly to CR2032 — not via boost |
| Boost control | AO3401 P-ch MOSFET SOT-23 | No external pull-up resistor — MCU GPIO internal pull-up used |
| Boost converter | Mini 600mA 3.3V (AliExpress) | Controlled via AO3401 on VIN path |

All other components identical to Proto A.

### Power Architecture

```
CR2032+ ──┬──→ XIAO 3V3 (direct, always on)
          └──→ AO3401 Source
AO3401 Gate ──→ GPIO D9 (MCU internal pull-up, active LOW = boost ON)
AO3401 Drain ──→ Boost VIN
Boost VOUT (3.3V) ──┬──→ SHT40 VCC
                     └──→ BH1750 VCC
XIAO GPIO D10 ──→ Soil sensor VCC (same as Proto A)
```

**Key note — body diode leak:** Even with AO3401 gate HIGH (boost nominally OFF), the body diode
conducts ~0.6V from Source to Drain, so boost VIN sees ~CR2032V − 0.6V in "off" state rather than
true 0V. This is the source of the measured ~4.5µA idle current. True load disconnect (as later
implemented with the XC9145 on the host PCB) eliminates this completely.

### Power Consumption (measured on Proto B)

| State | Current | Notes |
| :---- | :---- | :---- |
| nRF52840 deep sleep (RTC2) | 0.4 µA | Spec |
| SHT40 sleep | 0.15 µA | Spec |
| BH1750 ONE_TIME (between readings) | ~1 µA | Spec |
| AO3401 body diode leak into boost | ~2–3 µA | Measured residual — body diode, not gate leakage |
| QSPI Flash deep power-down | 0.5 µA | Must be explicitly put to sleep in firmware |
| **Total idle (measured)** | **~4.5 µA** | Real measurement |
| Active read cycle (BLE TX peak) | ~130 mA | 1–2 seconds per cycle |

Estimated battery life at 4.5µA idle: ~1.5–2 years on CR2032 at 1hr read interval. Longer-term
autonomy testing tracked as issue #3.

### Calibrations (Proto A & B)

```cpp
// Soil moisture — capacitive sensor on A1
#define SOIL_DRY 3500   // ADC value in air (dry)
#define SOIL_WET 1260   // ADC value submerged in water

// Battery voltage on A2 — CR2032: 3.0V = 100%, 2.0V = 0%
int batteryPercent = map((int)(voltage * 100), 200, 300, 0, 100);
```

Soil calibration values are position-dependent — recalibrate if sensor is moved. Use trends, not
absolute values. Fix sensor at ~2/3 depth, midway between center and pot edge.

---

## Shared — Firmware & HA Integration (Proto A & B)

| Layer | Choice |
| :---- | :---- |
| Framework | Arduino (Adafruit nRF52 package) |
| IDE | VSCode + PlatformIO |
| BLE | Bluefruit library — BTHome v2 custom advertising |
| Sensors | Adafruit SHT4x, claws/BH1750 |
| Sleep | systemOff / RTC2 deep sleep |

Power optimizations implemented: soil sensor powered only during read window, BH1750 ONE_TIME
auto power-down, QSPI flash explicit deep power-down, onboard LEDs disabled, debug serial gated,
configurable read interval.

**Home Assistant:** 5 entities auto-discovered via BTHome (moisture, temperature, humidity,
illuminance, battery), zero manual configuration. Note: HA creates two entities both named
"Humidity" — rename manually to "Soil Moisture" / "Air Humidity".

This firmware stack is Phase 1/2 and predates the host PCB's power architecture (no `SENSOR_EN` /
switched-rail awareness) — see issue #19. Not expected to run as-is on the host PCB.

---

## Legacy Decisions — still valid, not superseded by the PCB v1 track

These are decisions from the original Drive decision log that were never reopened and have no
newer equivalent in `docs/pcb-design-decisions.md`. Decisions about the AN54LV-15/AN54LQ-15 MCU
choice, the custom boost IC selection, and the PCB-embedded capacitive trace geometry are
**omitted here** — they're superseded by the real, current record in `docs/pcb-design-decisions.md`
and shouldn't be trusted from this file.

### MCU for Prototype

**Context:** Needed a BLE MCU working with Arduino/PlatformIO, supporting BTHome, running on CR2032.

| Option | Verdict |
| :---- | :---- |
| ESP32-C3 + WiFi | ❌ Rejected — brownout on CR2032 during WiFi TX (peak >200mA), tested and confirmed |
| Seeed XIAO BLE nRF52840 | ✅ Chosen — USB native, UF2 bootloader, Arduino support, CR2032 compatible |

**Key learning:** ESP32 + WiFi on CR2032 causes brownout — WiFi TX peaks are incompatible with
CR2032's internal resistance at low SoC.

### Boost Converter — Module vs Bare IC (Prototype)

BH1750 requires minimum 2.4V; CR2032 drops below this before the battery is depleted, wasting
30–40% of capacity without a boost converter. Decision: use a boost converter.

### Boost Converter Always-On vs Switched (Prototype)

AliExpress boost module (13µA quiescent) running 24/7 while BH1750 is only active ~500ms/hour —
13µA continuous was the dominant drain.

**Decision:** AO3401 P-channel MOSFET controlling boost VIN, driven by GPIO — ~3× battery life
improvement for ~2 CAD BOM addition. This is the circuit validated on Proto B above, and the
motivation that later led to the XC9145 true-load-disconnect choice on the host PCB.

### Temperature / Humidity Sensor

| Option | Verdict |
| :---- | :---- |
| DHT22 | ❌ Lower accuracy, no I2C |
| BME280 / AHT20 | 🟡 Acceptable |
| SHT40 | ✅ Chosen — best accuracy in class (±0.2°C, ±1.8% RH), native Zephyr driver available |

### Light Sensor

| Option | Verdict |
| :---- | :---- |
| LDR (photoresistor) | ❌ Non-linear, uncalibrated, no lux output |
| BH1750FVI-TR | ✅ Chosen — digital I2C lux, ONE_TIME mode auto power-down, addr 0x23, 2.4V min |
| VEML7700 | 🟡 Not evaluated in depth |

### Soil Moisture Sensing: Resistive vs Capacitive

| Option | Verdict |
| :---- | :---- |
| Resistive (electrode corrosion) | ❌ Rejected — electrodes corrode rapidly, unreliable after weeks |
| Capacitive external module | ✅ Used for prototype |

**Key learning:** capacitive soil readings are highly position-dependent. Absolute values vary with
placement; trends are what matter. Standard placement: ~2/3 depth, midway between pot center and
edge. Do not move sensor after calibration. (The later PCB-embedded trace geometry work is tracked
in `docs/pcb-design-decisions.md` §2.6–2.7 and issue #24 — this entry only covers the prototype
external-module decision.)

### Wireless Protocol

| Option | Verdict |
| :---- | :---- |
| WiFi (ESP32) | ❌ Rejected — brownout on CR2032 |
| BLE BTHome v2 | ✅ Chosen for Mini — native auto-discovery, zero config, lowest BLE power profile |
| BLE standard GATT | ❌ No auto-discovery in HA |
| ESPHome BLE | ❌ No nRF52840 support |

Zigbee/Thread for a possible Pro variant is an active, evolving question — see
`docs/plant-sensor-ideas.md` and the `feature/an54lq15-*` branches (dual-protocol firmware, BLE/Zigbee
switch, companion app), not this historical entry.

### Firmware Framework (Prototype vs PCB)

| Stage | Framework | Rationale |
| :---- | :---- | :---- |
| Prototype (nRF52840) | Arduino + PlatformIO + Adafruit nRF52 | Fast iteration, USB flashing, well-documented |
| PCB custom (nRF54L15) | Zephyr RTOS | nRF54L15 has no Arduino support — Zephyr only |

### Enclosure Decisions

- **Material:** PLA for prototype only (not humidity-resistant long-term); PETG is the production
  target (better humidity resistance, still printable on P1S); injection moulding deferred to >200
  units.
- **Ventilation:** Lateral slots, top AND bottom, for convection — without airflow, SHT40 measures
  enclosure temperature rather than ambient air. Tracked as issue #4.
- **IP rating:** none required — indoor use only, and sealing would work against the ventilation
  needed for accurate temperature readings.

### Manufacturing

**Decision:** JLCPCB for fab + PCBA, LCSC for components, DigiKey for consigned parts not on LCSC.
**Key learning:** JLCPCB Extended Part fees are per-order, not per-PCB — minimizing unique Extended
Parts matters even for small batches. (Shipping, not components, has since proven to be the actual
dominant cost driver — see `docs/cost-tracking.md`.)

---

## Archived / Not Pursued (prototype-era)

| Idea | Why Rejected / Deferred |
| :---- | :---- |
| OTA firmware via BLE (Nordic DFU) | Too complex with Arduino/PlatformIO; USB access sufficient for prototype |
| Decoupling capacitor 220–470µF external | Likely already present on XIAO PCB; not needed if prototype stable |
| Adaptive read intervals (day/night) | Marginal battery gain; idle current dominates |
| TPS61220 as boost converter | Internal diode creates leak path in shutdown — unsuitable for CR2032 |
| Si2301 MOSFET for boost control | RDS(on) not well-specified at VGS = −3V; AO3401 preferred |

## Bugs & Corrections Log (prototype-era)

| Issue | Root Cause | Fix |
| :---- | :---- | :---- |
| Soil sensor always reading 0 | Wrong pin — A0 used instead of A1 (P0.03) | Corrected to A1; recalibrated |
| Battery always showing 0% | Wrong pin — A1 used instead of A2 (P0.28) | Corrected to A2 |
| No auto-discovery in HA | Standard BLE GATT used instead of BTHome format | Switched to BTHome v2 advertising |
| ESPHome not working with nRF52840 | ESPHome has no nRF52840 support | Switched to Arduino + BTHome library |
| AO3401 body diode orientation (Proto B) | Initial understanding had anode/cathode reversed | Corrected: Drain→Source, verified against datasheet — the same class of error later recurred on the host PCB's Q1 (see `docs/pcb-design-decisions.md` §2.15 and issue #17) |

## External References

| Project | Relevance |
| :---- | :---- |
| Chirp / PlantWateringAlarm (Miceuz / Catnip Electronics) | Reference for GPIO-driven RC capacitive soil moisture sensing |
| Cave Pearl Project | Reference for adapting capacitive soil sensor to 3.3V operation |
