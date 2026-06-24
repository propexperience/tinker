# Example 05 — Audio DAC (PCM5102A over I²S)

Play an MP3 from the SD card through the PCM5102A I²S DAC.

## Pins

| Role | GPIO |
|---|---|
| SD MISO | 40 |
| SD MOSI | 38 |
| SD SCK | 39 |
| SD CS | 41 |
| I²S DIN (data) | 10 |
| I²S BCK (bit clock) | 11 |
| I²S LRCK (word clock) | 12 |

> ℹ️ The DAC pins (10/11/12) match the current firmware. If your board differs,
> update `I2S_DIN/BCK/LRCK` at the top of the sketch. See the
> [hardware reference](../../docs/HARDWARE.md#known-discrepancies--to-verify).

## What it does

1. Mounts the SD card.
2. Initialises the I²S output to the DAC.
3. Plays `/test.mp3` (or any file you name) on command.

## Try it

1. Put an MP3 named **`test.mp3`** in the **root** of a FAT32 SD card.
2. Flash, open the Serial Monitor at **115200**, and type:
   - `play` — play `/test.mp3`
   - `play /music/song.mp3` — play a specific file
   - `stop`
   - `vol <1-20>` — volume (20 = loudest)

## Important: it's a DAC, not an amp

The PCM5102A outputs **line level** audio. Connect:

- powered (active) speakers, **or**
- headphones, **or**
- an external amplifier

A bare passive speaker will be far too quiet and can load the DAC.

## Library

- **ESP8266Audio** (by Earle F. Philhower) — install via Library Manager. It
  works on the ESP32 despite the name.

## Troubleshooting

- **No sound:** confirm the file is a real MP3, check the DAC pin numbers, and
  verify your speakers/amp are powered.
- **Distorted:** lower the volume (`vol 4`).
- **Won't mount:** see [Example 01](../01-sd-card) for SD troubleshooting.
