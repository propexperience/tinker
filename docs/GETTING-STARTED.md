# Getting Started with PropX Tinker

This guide gets you from "board in a box" to typing commands at a live prompt.
It assumes you can use a terminal but doesn't assume any electronics background.

> 📷 **Image placeholder:** board connected to a laptop over USB-C → `images/usb-connected.png`

---

## 1. What you need

- A **PropX Tinker** board
- A **USB-C cable** (must be a data cable, not charge-only)
- A computer with **[PlatformIO](https://platformio.org/)** installed
  (the VS Code extension is the easiest path)
- *(optional)* a microSD card formatted **FAT32** for the audio/file features
- *(optional)* an **SSD1306 OLED** for the display header

---

## 2. Install the toolchain

1. Install [VS Code](https://code.visualstudio.com/).
2. In VS Code, install the **PlatformIO IDE** extension.
3. Open this `tinker/` folder in VS Code. PlatformIO will detect
   `platformio.ini` and download the ESP32 toolchain and libraries on first
   build (this can take a few minutes).

The board target is already configured:

```ini
[env:esp32-s3-devkitc-1]
platform      = espressif32
board         = esp32-s3-devkitc-1
framework     = arduino
monitor_speed = 115200
```

---

## 3. Build, flash, and connect

Plug in the board and run:

```bash
pio run -t upload -t monitor
```

This compiles the firmware, flashes it over USB-C, and opens the serial
monitor at **115200 baud**. You should see a banner and a `>` prompt.

> 💡 **If upload fails:** hold the **BOOT** button, tap **RESET**, release
> BOOT, then upload again to force the chip into download mode.

> 🖥️ The serial console runs over the **USB-C port** (USB CDC). The UART pins on
> Connector A (GPIO1/2) are a *separate* hardware serial port for talking to
> other devices.

---

## 4. The serial console

Everything on the board is driven from this prompt. Commands are not
case-sensitive; arguments (like file names) are.

### Top-level commands

| Command | What it does |
|---|---|
| `ls` | List the SD card's root directory |
| `mount` | (Re)mount the SD card |
| `box` | Enter the **MP3 player / file browser** |
| `gpio` | List the configurable GPIO and their state |
| `gpio <pin> <action>` | Configure or drive a GPIO (see below) |

### Configurable GPIO — `gpio`

Pins **1, 2, 3, 9, 17, 42** can be reconfigured live:

| Action | Meaning |
|---|---|
| `gpio` | list all configurable pins with mode + level |
| `gpio <pin> in` | input, high-Z (let a jumper/external signal set it) |
| `gpio <pin> up` | input with internal **pull-up** |
| `gpio <pin> down` | input with internal **pull-down** |
| `gpio <pin> out` | output (starts LOW) |
| `gpio <pin> hi` / `lo` | drive an output HIGH / LOW |
| `gpio <pin> tog` | toggle an output |
| `gpio <pin> rd` | read the current level |

Example — make GPIO17 an output and turn it on:
```
gpio 17 out
gpio 17 hi
```

Reminder: **GPIO3 and GPIO17 have no external pull**, so use `up`/`down` (or a
jumper) if you want a defined level on an input. See
[Jumpers](HARDWARE.md#4-jumpers-pull-up--pull-down-select).

### MP3 player — `box`

Type `box` to enter the file browser (requires a mounted SD card). Inside:

| Command | What it does |
|---|---|
| `ls` | re-list the current folder (sub-folders, then numbered MP3s) |
| `cd <dir>` | enter a sub-folder |
| `cd ..` | go up one folder |
| `play <n>` | play numbered track `n` |
| `stop` | stop playback |
| `vol <1-20>` | set volume (20 = loudest) |
| `exit` | leave the browser |

Hidden files (names starting with `.`, e.g. macOS `._track.mp3`) are skipped.

> 🔊 Audio comes out of the **PCM5102A DAC** at line level — connect powered
> speakers or an amp, not a bare speaker.

### Buttons & RGB (automatic)

- The board has **4 buttons** (GPIO13, 14, 21, 47); each prints a line when
  pressed/released.
- Two of them also **cycle the NeoPixel** through a 10-colour palette.

> ℹ️ The demo firmware's button→output mapping is still being reconciled with
> the final hardware (the MOSFET outputs are active-LOW and opto-isolated). See
> [Known discrepancies](HARDWARE.md#known-discrepancies--to-verify).

---

## 5. Next steps

- Read the **[Hardware reference](HARDWARE.md)** to understand each connector.
- Wire up a 12 V load on a MOSFET output (mind the
  [safety notes](HARDWARE.md#connectors-d--e--mosfet-power-outputs)).
- Want to change behaviour? The firmware is one file: `src/main.cpp`.

Stuck or found a bug? See **[CONTRIBUTING.md](../CONTRIBUTING.md)**.
