<!-- ▸ Banner placeholder — replace with the PropX Tinker logo/board hero shot -->
<!-- docs/images/banner.png -->
> 📷 **Image placeholder:** project banner / board hero shot → `docs/images/banner.png`

# PropX Tinker

**An open-source ESP32-S3 tinkering board for learning real-world I/O.**

PropX Tinker is a small development board built around the **ESP32-S3-WROOM-1
(N16R8)**. It breaks the microcontroller out into friendly, labelled connectors
so you can experiment with the things real products are made of: switched 12 V
power outputs, isolated inputs, audio, a display, an SD card, buttons, and an
RGB LED — all driven from a simple serial console.

It is meant for people who are **new to electronics but not afraid of it**: if
you can plug in a USB-C cable and type a command, you can use this board.

---

## What's on the board

| Feature | Detail |
|---|---|
| **MCU** | ESP32-S3-WROOM-1 **N16R8** — 16 MB flash, 8 MB PSRAM, Wi-Fi/BLE, PCB antenna |
| **Power / programming** | USB-C (5 V in, also the programming + serial console port) |
| **Storage** | microSD card slot (SPI) |
| **Audio** | PCM5102A I²S DAC → line-level analog out |
| **Display** | SSD1306 OLED header (I²C) |
| **RGB** | One NeoPixel (WS2812-style) on GPIO48 |
| **Buttons** | 4 on-board push buttons (GPIO13, 14, 21, 47) |
| **Power outputs** | 4 × opto-isolated MOSFET switches (GPIO4–7), 9–12 V loads |
| **Isolated inputs** | 2 × bidirectional optocoupler inputs (GPIO15, 16) |
| **Breakouts** | UART, I²C, and SPI connectors + several jumper-selectable GPIO |

> 📷 **Image placeholder:** annotated top-down board photo with every connector
> labelled → `docs/images/board-top-annotated.png`

---

## Quick start

1. Plug the board into your computer with a **USB-C cable**.
2. Install [PlatformIO](https://platformio.org/) (VS Code extension is easiest).
3. Build & flash the firmware, then open the serial monitor:
   ```bash
   pio run -t upload -t monitor
   ```
4. At the `>` prompt, type `gpio` to list the configurable pins, or `box` to
   open the SD-card MP3 player.

Full walkthrough → **[docs/GETTING-STARTED.md](docs/GETTING-STARTED.md)**

---

## Documentation

| Doc | What's in it |
|---|---|
| **[Hardware reference](docs/HARDWARE.md)** | Every pin, every connector, jumpers, power, and safety |
| **[Getting started](docs/GETTING-STARTED.md)** | Toolchain, flashing, and the serial console command reference |
| **[Contributing](CONTRIBUTING.md)** | How to file issues and propose changes |

---

## Repository layout

```
tinker/
├── README.md            ← you are here
├── platformio.ini       ← build configuration
├── src/
│   └── main.cpp         ← firmware (serial console + all peripherals)
└── docs/
    ├── HARDWARE.md      ← pin & connector reference
    ├── GETTING-STARTED.md
    └── images/          ← photos, diagrams, schematics (placeholders)
```

---

## Project status

🚧 **Early / actively tinkering.** Pin assignments are stabilising; a few are
still being confirmed against the schematic — see
[Known discrepancies](docs/HARDWARE.md#known-discrepancies--to-verify).

This documentation will eventually become the project website.

---

## License

> ⚠️ **Placeholder — choose before first public release.**
> Suggested: **hardware** under [CERN-OHL-S or -W](https://ohwr.org/cernohl),
> **firmware/docs** under MIT or Apache-2.0. Add a `LICENSE` file once decided.

---

## Credits

PropX Tinker is part of the **PropX** project. Contributions welcome — see
[CONTRIBUTING.md](CONTRIBUTING.md).
