# Example 03 — NeoPixel RGB

Animate the on-board RGB LED: step through a color palette, then run a smooth
rainbow sweep.

## Pin

| Signal | GPIO |
|---|---|
| Data | 48 |

A single WS2812-style addressable LED on the board.

## What it does

1. Sets a modest brightness (these LEDs are very bright at full power).
2. Steps through 8 named colors.
3. Runs a continuous rainbow using `ColorHSV()` + gamma correction.

## Configuration

- `BRIGHTNESS` — 0–255. Start low (default 60) to avoid eye-searing output and
  high current draw.
- `NUM_PIXELS` — set higher if you chain more pixels off the data line.

## Notes

- Color order is `NEO_GRB` (standard WS2812). If red/green look swapped, change
  it to `NEO_RGB` in the constructor.

## Library

- `Adafruit NeoPixel`
