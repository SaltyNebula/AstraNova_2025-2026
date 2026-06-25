# RPD Hardware

The custom 2-layer PCB for the **RPD**, AstraNova's rideshare flight data logger. Designed in **Fusion 360 (Eagle)** and fabricated at **JLCPCB**, with SMD assembly done in-house with engineering lab-tech support. Installed mass is about **102 g** including the sled.

> The flight-day power failure and the board-level fixes are in [`../Documentation/Avionics_RPD_and_Flight_Postmortem.md`](../Documentation/Avionics_RPD_and_Flight_Postmortem.md); see [Known issues](#known-issues-and-planned-fixes).

## Components

| Subsystem | Part | Interface |
| --- | --- | --- |
| MCU | Raspberry Pi Pico 2 (RP2350), SMD-mounted | ; |
| IMU + barometer | Waveshare 10-DOF (ICM-20948 + BMP280) | I2C1 |
| Storage | micro-SD card | SPI0 |
| Display | 2.9" e-paper V2 (296 x 128) | SPI1 |
| Environment | DHT22 | 1-wire |
| Level shifter | CD74HC4050 (Rev V2 only) | ; |
| Indicators | buzzer, green/red status LEDs, ARM / ZERO buttons | GPIO |
| Customer payload supply | 4 A buck converter, 5 V, female XT30 | ; |
| Rideshare supply | LD1117 LDO chain | ; |
| Power in | 2S Li-ion (18650CA-2S-3J, 7.4 V 2200 mAh) | ; |

The PCB carries a **silkscreened team logo** (requirement RPD-005).

## Power architecture

```
2S Li-ion --+-- buck converter (5V, 4A) ----------------- customer payload (XT30)
            |
            +-- LD1117S50 (5V) -- LD1117S33 (3.3V) ------ RPD rail
```

The customer payload and the RPD share only the **battery input terminal**; downstream each has its own regulation. This shared source, combined with the regulator cascade and a lack of bulk decoupling, is what produced the flight-day brownout.

## Board revisions

- **Rev V2, flight board.** ENIG finish on black soldermask; includes the CD74HC4050 level-shifter path. This is the board that flew, and it survived the ballistic impact intact.
- **Rev B, test article.** HASL finish; level shifter removed (redundant for all-3.3 V logic) and the SD card wired directly. Built to validate the simplification before committing it to a flight spin.

## Fabrication notes

- 2-layer board, bottom-layer ground plane, hand-routed critical nets (paired I2C, SPI to SD and e-paper, 0.5 to 1 mm power traces).
- JLCPCB standard 2-layer process; confirm design rules before ordering (min trace and clearance 0.127 mm, min via 0.3 mm hole / 0.6 mm annular ring).
- Power section soldered and voltage-checked first, before populating the rest.

## What lives in this folder

Drop the following here as you export them:

```
hardware/
  schematic.pdf          exported schematic
  layout/                board renders and screenshots
  gerbers/               fabrication outputs (Rev V2 ENIG, Rev B HASL)
  bom.csv                bill of materials
  source/                Fusion 360 / Eagle design files (.sch, .brd)
```

Board photos used by the top-level README go in `../figures/` (for example `avionics.jpg`).

## Known issues and planned fixes

The RPD recorded no flight data due to a shared-source brownout. The hardware changes, in order of effect:

1. **Independent battery for the RPD**, decoupled from the customer rail; this alone breaks the failure chain.
2. **Drop the 5 V to 3.3 V cascade** for a single regulator off the pack, to recover the sag margin the high-dropout LD1117 chain gave away.
3. **Add bulk capacitance** at the board input and next to the SD card and e-paper, for transient ride-through.

See the [full write-up](../Documentation/Avionics_RPD_and_Flight_Postmortem.md) for the reasoning.
