# Example 01 — microSD Card

Mount the microSD card over SPI, print card info, list the root directory, and
read a text file.

## Pins

| Signal | GPIO |
|---|---|
| MISO | 40 |
| MOSI | 38 |
| SCK / CLK | 39 |
| CS | 41 |

These are also broken out on **Connector C** (except CS, which stays dedicated
to the on-board microSD slot).

## What it does

1. Initialises a dedicated SPI bus on the pins above.
2. Mounts the card at 4 MHz and prints its type and size.
3. Lists everything in the root directory (folders and files).
4. If a file named `/hello.txt` exists, prints its contents.

## Try it

1. Format a microSD card as **FAT32** and (optionally) create a `hello.txt` on
   it with some text.
2. Insert the card, flash the sketch, open the Serial Monitor at **115200**.
3. At the `>` prompt:
   - `ls` — re-list the root directory
   - `mount` — retry mounting (after swapping cards)

## Notes

- **FAT32 only.** exFAT is not supported by the Arduino SD library.
- Start at **4 MHz**; once reliable you can raise the speed in `SD.begin()`.
- If mounting fails: check CS wiring, that the card is seated, and that a
  pull-up is present on the CS line (most breakouts include one).

## Libraries

None beyond the built-in ESP32 **SD** library.
