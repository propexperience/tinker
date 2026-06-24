# PropX Tinker — Example Sketches

Standalone Arduino sketches, one per major component, so you can test and learn
each part of the board on its own. Copy a folder into your Arduino sketchbook,
open the `.ino`, install the listed libraries, and flash.

> These are **Arduino IDE** sketches. PlatformIO versions may follow later.

## Board target

- **Board:** ESP32-S3 (select *ESP32S3 Dev Module* in Arduino IDE)
- **USB CDC On Boot:** **Enabled** (so the Serial Monitor works over USB-C)
- **Upload speed / port:** your USB-C port
- **Serial Monitor:** 115200 baud

## The examples

| # | Folder | Component | Libraries needed |
|---|---|---|---|
| 1 | [`01-sd-card`](01-sd-card) | microSD card (SPI) | *(built-in SD)* |
| 2 | [`02-oled-display`](02-oled-display) | SSD1306 OLED (I²C) | Adafruit GFX, Adafruit SSD1306 |
| 3 | [`03-neopixel-rgb`](03-neopixel-rgb) | NeoPixel RGB | Adafruit NeoPixel |
| 4 | [`04-io-control`](04-io-control) | Buttons + opto inputs + MOSFET outputs | *(built-in)* |
| 5 | [`05-audio-dac`](05-audio-dac) | PCM5102A audio (I²S) | ESP8266Audio |
| 6 | [`06-web-mp3-player`](06-web-mp3-player) | SD + audio + Wi-Fi web UI | ESP8266Audio |

Each folder has its own `README.md` with wiring, pins, and usage.

## Installing libraries (Arduino IDE)

**Sketch → Include Library → Manage Libraries…**, then search and install:

- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `Adafruit NeoPixel`
- `ESP8266Audio` (by Earle F. Philhower — works on ESP32 too)

## Pin cheat-sheet

| Component | Pins |
|---|---|
| microSD (SPI) | MISO 40, MOSI 38, SCK 39, CS 41 |
| OLED (I²C) | SDA 18, SCL 8 |
| NeoPixel | data 48 |
| Buttons | 13, 14, 21, 47 |
| Opto inputs | 15, 16 |
| MOSFET outputs | 4, 5, 6, 7 (**active-LOW**) |
| Audio DAC (I²S) | DIN 10, BCK 11, LRCK 12 |

See the full [hardware reference](../docs/HARDWARE.md) for wiring and safety.
