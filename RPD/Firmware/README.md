# RPD Firmware

Flight firmware for the **RPD (Rocket Position Data recorder)**, AstraNova's rideshare flight data logger. It runs on a **Raspberry Pi Pico 2 (RP2350)** under the Arduino framework, logging a 10-DOF IMU, a barometer, and a DHT22 to micro-SD through the flight, and reporting peak altitude on a 2.9" e-paper screen after landing.

> The flight-day failure and the planned changes are documented in [`../Documentation/Avionics_RPD_and_Flight_Postmortem.md`](../Documentation/Avionics_RPD_and_Flight_Postmortem.md); see [Known issues](#known-issues-and-planned-fixes) below.

## Files

| File | Purpose |
| --- | --- |
| `FlightLogger.ino` | Main firmware; flight state machine and SD logging |
| `Waveshare_10Dof-D.*` | IMU and barometer driver (ICM-20948 + BMP280) |
| `epd2in9_V2.*` | 2.9" e-paper V2 driver |
| `epdpaint.*` | Framebuffer and drawing primitives |
| `epdif.*` | e-paper SPI interface and pin definitions |
| `fonts.h`, `font8/12/16/20/24.cpp` | Bitmap fonts for the display |

External dependency: an Arduino DHT sensor library (for the DHT22).

## Pin map

| Function | Pin | Bus |
| --- | --- | --- |
| ARM button | GP0 | GPIO |
| ZERO button | GP1 | GPIO |
| Status LED | GP28 | GPIO |
| Buzzer | GP7 | GPIO |
| SD card detect | GP20 | GPIO |
| DHT22 | GP8 | 1-wire |
| IMU SDA / SCL | GP26 / GP27 | I2C1 |
| SD MISO / CS / SCK / MOSI | GP16 / GP17 / GP18 / GP19 | SPI0 |
| e-paper CS / SCK / MOSI | GP9 / GP10 / GP11 | SPI1 |
| e-paper RST / BUSY / DC / PWR | GP12 / GP13 / GP14 / GP15 | SPI1 |

## Build and flash

1. Install the [arduino-pico](https://github.com/earlephilhower/arduino-pico) core and select **Raspberry Pi Pico 2** as the board.
2. Install an Arduino DHT22 library through the Library Manager.
3. Keep the driver files in this folder alongside `FlightLogger.ino` (or copy them into your Arduino libraries directory).
4. Compile and flash over USB; hold **BOOTSEL** while connecting for the first flash.
5. Insert a FAT-formatted micro-SD card.

## Flight state machine

`IDLE` to `ARMED` to `ASCENT` to `DESCENT` to `LANDED`, all autonomous once armed.

| Transition | Condition |
| --- | --- |
| IDLE to ARMED | ARM button; sets the ground baseline and opens the log |
| ARMED to ASCENT | acceleration > 3 g, debounced 150 ms |
| ASCENT to DESCENT | barometric altitude falling for 500 ms (apogee) |
| DESCENT to LANDED | AGL < 330 ft, altitude stable within a 10 ft band, ~1 g, held 5 s |
| any flight state to LANDED | 180 s time failsafe |
| LANDED to IDLE | ARM button (reset) |

The accelerometer is configured to **+/-16 g** full-scale, since boost peaks would clip lower settings.

## Buttons

- **ARM** (GP0): in `IDLE`, arms the recorder and begins logging; in `LANDED`, resets to `IDLE`.
- **ZERO** (GP1): rebaselines the displayed relative altitude; does not touch the log.

## Log format

Written to `flightlog.csv`, flushed every second. A `STARTUP` calibration row is written at power-on, `LOG` rows once armed:

```
TimeMs,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,DHTTempC,DHTHum,IMUTempC,AltitudeM,Event
```

A Python script (see the requirement record for RPD-003) turns the CSV into an altitude-versus-time plot in feet.

## Known issues and planned fixes

From the post-mortem, the recorder logged no flight data on the day due to a shared-source power brownout. The firmware-side changes that follow from it:

- Set the armed flag and **start logging before any display call**, so a display problem can never gate logging.
- Put a **timeout on every e-paper busy-wait**; the stock driver waits on the BUSY pin with no timeout, which can hang the loop if the panel is unresponsive.
- Optionally run the display on the RP2350's **second core**, so it physically cannot stall the state machine or SD logging.

These pair with the hardware fix (an independent supply for the RPD); see the [full write-up](/Documentation/Avionics_RPD_and_Flight_Postmortem.md).
