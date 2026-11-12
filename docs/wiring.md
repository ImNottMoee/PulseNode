# PulseNode wiring

All parts share one I2C bus, powered from the ESP32's 3.3 V regulator through the breadboard rails.

## Connections

| Signal | ESP32 (30-pin dev board) | Breadboard | SSD1306 OLED | MAX30102 |
|---|---|---|---|---|
| 3.3 V | 3V3 | + rail | VCC | VIN |
| Ground | GND | − rail | GND | GND |
| Data (SDA) | GPIO 21 (D21) | — | SDA | SDA |
| Clock (SCL) | GPIO 22 (D22) | — | SCL | SCL |
| Interrupt | not used | — | — | INT (leave unconnected) |

The MAX30102's IRD and RD pins are left unconnected.

## I2C addresses

| Device | Address |
|---|---|
| SSD1306 OLED | `0x3C` (some boards use `0x3D`) |
| MAX30102 | `0x57` |

Run an I2C scanner sketch after wiring; both addresses should appear.

## Pin notes

- **Use 3V3, not 5V/VIN.** Both modules run at 3.3 V, which keeps the whole bus at the ESP32's logic level.
- **30-pin board placement:** on the 30-pin ESP32 dev board, 3V3, GND, D21 and D22 are all on the same side. Seat the board so that side has free breadboard holes next to it; the other side can be fully covered.
- **One sensor per bus:** both MAX30102 modules in a 2-pack share address `0x57`, so only one can be on the bus at a time.
- **MAX30102 header pins** usually ship loose and must be soldered. Pins pressed into the holes without solder give intermittent I2C errors and noisy readings.
- **MH-ET LIVE pull-up jumper:** many black MH-ET LIVE boards pull SDA/SCL up to 1.8 V by default. If the sensor doesn't show up at `0x57`, move the board's 1V8/3V3 solder jumper to 3V3.
- **Split power rails:** some large breadboards break the + and − rails in the middle. Keep every rail connection on one side of the break, or bridge the halves with two short jumpers.
- **Two-color OLED:** the 0.96" display used here has a yellow band (rows 0–15) and a blue body (rows 16–63). The colors are fixed in hardware; the firmware puts the header in the yellow band.

## Wiring orientation used in this build

```
                 ┌───────────────┐
   USB  ◄────────┤ ESP32 (30-pin)│
                 │          3V3 ─┼──► + rail ──► OLED VCC, MAX30102 VIN
                 │          GND ─┼──► − rail ──► OLED GND, MAX30102 GND
                 │          D21 ─┼──► SDA bus ─► OLED SDA, MAX30102 SDA
                 │          D22 ─┼──► SCL bus ─► OLED SCL, MAX30102 SCL
                 └───────────────┘
```
