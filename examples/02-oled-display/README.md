# Example 02 — SSD1306 OLED Display

Drive a 128×64 SSD1306 OLED over I²C: show the PropX logo and cycle through a
few text screens.

## Pins

| Signal | GPIO |
|---|---|
| SDA | 18 |
| SCL | 8 |

Plug the OLED into the **display header** (or wire to **Connector B**). Both
share the same I²C bus.

## What it does

- Initialises the panel at I²C address `0x3C`.
- Draws the **PropX lockup** (bold "PROP" + a big X).
- Every 2.5 s, rotates between the logo and a couple of auto-sized text screens.

## Configuration

At the top of the sketch:

- `OLED_H` — set to `32` if you have a 128×**32** panel (default `64`).
- `OLED_ADDR` — change to `0x3D` if `0x3C` doesn't initialise.

## Try it

1. Plug in the OLED, flash the sketch, open the Serial Monitor at **115200**.
2. You should see the logo, then rotating text. If it says init failed, flip the
   address to `0x3D`.

## Libraries

- `Adafruit GFX Library`
- `Adafruit SSD1306`
