# PropX Tinker — Hardware Reference

Everything the firmware talks to, and how to wire it up safely.

This page is the single source of truth for **what each pin does** and **what
each connector carries**. If you only read one document, read this one.

> 📷 **Image placeholder:** annotated board photo, top view → `images/board-top-annotated.png`
> 📐 **Diagram placeholder:** connector-location map (silkscreen outline) → `images/connector-map.svg`
> 📄 **Schematic placeholder:** full schematic PDF → `images/schematic.pdf`

---

## 1. The microcontroller

**ESP32-S3-WROOM-1 (N16R8)** with integrated PCB antenna.

| Spec | Value |
|---|---|
| Flash | 16 MB |
| PSRAM | 8 MB (octal) |
| Core | Dual-core Xtensa LX7, up to 240 MHz |
| Radio | Wi-Fi 2.4 GHz + Bluetooth LE |
| Logic level | **3.3 V** — the GPIO pins are **not 5 V tolerant** |

> ⚠️ **3.3 V only.** Never connect a raw 5 V or 12 V signal directly to a GPIO.
> The 12 V on this board is always routed through a MOSFET or optocoupler that
> keeps it away from the chip.

### Reserved pins — do not use

These are consumed by the module or the USB interface and are **not** available
on connectors:

| Pins | Used for |
|---|---|
| GPIO26–32 | SPI flash |
| GPIO33–37 | Octal PSRAM (the "R8") |
| GPIO19, GPIO20 | Native USB D− / D+ (USB-C) |
| GPIO0 | BOOT strapping / boot button |

---

## 2. Master pin map

This is every GPIO the board actually uses, with its job and where it appears.

| GPIO | Function | Connector / Location | Notes |
|---:|---|---|---|
| 1  | **UART TX** (TXD0) / GPIO | Connector **A** | Jumper **J1** pull-select |
| 2  | **UART RX** (RXD0) / GPIO | Connector **A** | Jumper **J2** pull-select |
| 3  | GPIO | Connector **C** | ⚠️ strapping pin, **no external pull** |
| 4  | **MOSFET output 1** | Connector **D** | Active-**LOW**, switched ground, 9–12 V load |
| 5  | **MOSFET output 2** | Connector **D** | Active-**LOW**, switched ground, 9–12 V load |
| 6  | **MOSFET output 3** | Connector **E** | Active-**LOW**, switched ground, 9–12 V load |
| 7  | **MOSFET output 4** | Connector **E** | Active-**LOW**, switched ground, 9–12 V load |
| 8  | **I²C SCL** | Connector **B** + Display header | Shared I²C bus |
| 9  | GPIO | Connector **B** | Jumper **J3** pull-select |
| 10 | **I²S DIN** → DAC | (on-board, to PCM5102A) | data to DAC · _verify_ |
| 11 | **I²S BCK** → DAC | (on-board, to PCM5102A) | bit clock · _verify_ |
| 12 | **I²S LRCK/WS** → DAC | (on-board, to PCM5102A) | word clock · _verify_ |
| 13 | **Button 1** | on-board | active-low |
| 14 | **Button 2** | on-board | active-low |
| 15 | **Opto input A** | Opto connector | isolated, 9–12 V field side |
| 16 | **Opto input B** | Opto connector | isolated, 9–12 V field side |
| 17 | GPIO | Connector **B** | **no external pull** |
| 18 | **I²C SDA** | Connector **B** + Display header | Shared I²C bus |
| 21 | **Button 3** | on-board | active-low |
| 38 | **SPI MOSI** | Connector **C** | Shared with microSD |
| 39 | **SPI CLK/SCK** | Connector **C** | Shared with microSD |
| 40 | **SPI MISO** | Connector **C** | Shared with microSD |
| 41 | **SPI CS** (microSD) | on-board microSD | Dedicated to the card |
| 42 | GPIO | Connector **C** | Jumper **J4** pull-select |
| 47 | **Button 4** | on-board | active-low |
| 48 | **NeoPixel RGB** data | on-board LED | WS2812-style, 1 pixel |

---

## 3. Connectors

Each connector also carries **GND** and **VCC** unless noted. **VCC is the
3.3 V logic rail** (good for small sensors/peripherals).

> 📐 **Diagram placeholder:** per-connector pinout drawings (pin 1 marked) →
> `images/connector-pinouts.svg`

### Connector A — UART / GPIO
| Pin | Signal |
|---|---|
| VCC | 3.3 V |
| GND | Ground |
| TX  | GPIO1 (TXD0 — board → device) |
| RX  | GPIO2 (RXD0 — device → board) |

Use it as a **serial port** (e.g. to talk to another microcontroller or a
GPS/sensor module) or as **two plain GPIO**. Cross the lines: the other
device's RX goes to this **TX**, and its TX goes to this **RX**.

### Connector B — I²C / GPIO
| Pin | Signal |
|---|---|
| VCC | 3.3 V |
| GND | Ground |
| SDA | GPIO18 |
| SCL | GPIO8 |
| GPIO17 | general I/O (no external pull) |
| GPIO9  | general I/O (jumper **J3**) |

The I²C bus here is the **same bus** as the display header — you can chain
multiple I²C devices as long as their addresses don't clash.

### Connector C — SPI / GPIO
| Pin | Signal |
|---|---|
| VCC | 3.3 V |
| GND | Ground |
| MOSI | GPIO38 |
| CLK  | GPIO39 |
| MISO | GPIO40 |
| GPIO3  | general I/O (no external pull, strapping pin) |
| GPIO42 | general I/O (jumper **J4**) |

This breaks out the **same SPI bus** the microSD card uses. To add a second SPI
device, give it its own chip-select — **GPIO3 or GPIO42** are the natural
choices — and the firmware can select it without disturbing the card (CS =
GPIO41 stays dedicated to the SD slot).

### Display header — I²C OLED
| Pin | Signal |
|---|---|
| VCC | 3.3 V |
| GND | Ground |
| SDA | GPIO18 |
| SCL | GPIO8 |

Designed for a common **SSD1306 128×64 OLED** (I²C address `0x3C`, sometimes
`0x3D`). The firmware draws the PropX logo here at boot.

> 📷 **Image placeholder:** OLED plugged into the display header → `images/display-header.png`

### Power connector
| Pin | Signal |
|---|---|
| 5V  | 5 V rail (from USB-C) |
| VCC | 3.3 V logic rail |
| GND | Ground |

A convenience tap for powering external circuitry. **Mind the current limits**
of the on-board regulator (_specify once known_).

### Connectors D & E — MOSFET power outputs
Opto-isolated, low-side MOSFET switches for driving **9–12 V loads** (LED
strips, solenoids, relays, small motors).

| Connector | Pins |
|---|---|
| **D** | 12 V (common) · OUT (GPIO4) · OUT (GPIO5) |
| **E** | 12 V (common) · OUT (GPIO6) · OUT (GPIO7) |

**How it works (active-LOW):** each GPIO has a **10 kΩ pull-up**, so it idles
HIGH and the output is **off**. Pulling the GPIO **LOW** turns on an
optocoupler, which switches the MOSFET on and completes the load's path to
ground.

```
        drive GPIO LOW ──► optocoupler ON ──► MOSFET ON ──► load energised
        GPIO idle HIGH  ──► optocoupler OFF ─► MOSFET OFF ─► load off

  +12V ──●──────────────┐
         │            [ LOAD ]   (e.g. LED strip)
         │              │
  OUT ───┴── MOSFET ────┘   (switched ground)
```

Wire your load between the connector's **12 V** pin and an **OUT** pin.

| GPIO state | Optocoupler | MOSFET | Load |
|---|---|---|---|
| HIGH (idle, pulled up) | off | off | **off** |
| LOW (driven) | on | on | **ON** |

> ⚠️ **Power is supplied externally** (9–12 V) through the connector's 12 V pin.
> Respect the per-channel current rating of the MOSFET (_specify once known_)
> and add flyback protection for inductive loads (motors, solenoids, relays).
> 💡 Because the gate is opto-isolated and pulled up, the output stays **off**
> while the ESP32 is resetting or unprogrammed — a safe default.

> 📷 **Image placeholder:** an LED strip wired to Connector D → `images/mosfet-output-wiring.png`

### Optocoupler input connector — isolated inputs
Two **bidirectional optocoupler** inputs (GPIO15, GPIO16). Because they're
optically isolated, the field side can switch **either +12 V or GND** (9–12 V)
without any electrical connection to the ESP32.

6-pin connector:

| Pins | Signal |
|---|---|
| 2 pins | +12 V and GND (field-side reference) |
| 4 pins | two isolated input **pairs** — one pair per optocoupler |

Each input pair drives one optocoupler → one GPIO (15 and 16). Use these to
sense external switches, buttons, or signals that live on a 12 V system,
safely.

> 📐 **Diagram placeholder:** optocoupler input wiring example → `images/opto-input-wiring.svg`

---

## 4. Jumpers (pull-up / pull-down select)

Four **3-pin jumpers**, each with a **10 kΩ** resistor. The centre pin connects
to the GPIO; the two outer pins go to **VCC** and **GND**. Where you place the
shunt sets the pin's resting logic level when nothing else is driving it.

| Jumper | GPIO | Position → VCC | Position → GND | Removed |
|---|---|---|---|---|
| **J1** | GPIO1 | pull-**up** (idles HIGH) | pull-**down** (idles LOW) | floating |
| **J2** | GPIO2 | pull-**up** | pull-**down** | floating |
| **J3** | GPIO9 | pull-**up** | pull-**down** | floating |
| **J4** | GPIO42 | pull-**up** | pull-**down** | floating |

**GPIO3 and GPIO17 have no jumper and no external pull resistor** — left as
plain inputs they *float* (their reading is undefined). For those two, either
enable the chip's **internal** pull in firmware (`gpio 3 up` / `gpio 17 down`)
or provide your own pull resistor.

> 📷 **Image placeholder:** close-up of a jumper showing VCC / GND positions → `images/jumper-detail.png`

> 💡 The jumper's external 10 kΩ and the chip's internal pull can both be on at
> once — that's fine, but if you jumper to GND **and** enable an internal
> pull-up, the pin sits LOW while a small current flows through the resistors.
> Pick one source of pull when it matters.

---

## 5. Peripherals at a glance

| Peripheral | Pins | Notes |
|---|---|---|
| **microSD** | MOSI 38, CLK 39, MISO 40, CS 41 | FAT32; firmware mounts at 4 MHz |
| **OLED (SSD1306)** | SDA 18, SCL 8 | I²C `0x3C` |
| **Audio DAC (PCM5102A)** | DIN 10, BCK 11, LRCK 12 | I²S, **line-level out** — needs powered speakers/amp · _verify_ |
| **NeoPixel** | data 48 | 1 × WS2812-style RGB |
| **Buttons** | 13, 14, 21, 47 | active-low |
| **MOSFET outputs** | 4, 5, 6, 7 | active-LOW, 9–12 V, switched ground |
| **Opto inputs** | 15, 16 | isolated, 9–12 V field side |

> ℹ️ The **PCM5102A is a DAC, not an amplifier** — its output is line level.
> Connect it to powered speakers, headphones, or an external amplifier.

---

## 6. Power architecture

> 📐 **Diagram placeholder:** power tree (USB-C 5 V → 3.3 V regulator → VCC; 12 V
> external → MOSFET/opto domain) → `images/power-tree.svg`

- **USB-C 5 V** powers the board and is regulated down to the **3.3 V** logic
  rail (**VCC = 3.3 V**) used by the MCU and the connector VCC pins.
- **9–12 V** is a **separate, external** domain used only by the MOSFET outputs
  and the optocoupler field side. It never touches the 3.3 V logic.
- Grounds are common between 5 V/3.3 V; the optocoupler inputs keep their
  field-side ground **isolated** from logic ground.

> ⚠️ Confirm where the 9–12 V enters the board and its maximum current before
> connecting a supply.

---

## 7. Known discrepancies — to verify

A couple of open items remain where the current firmware and the confirmed
hardware don't fully agree. They're called out so nobody trusts a wrong
assumption. Update this section as the schematic is confirmed.

1. **I²S / DAC pins (GPIO10/11/12).** Early notes said DIN=9, CLK=10, LRCK=11;
   the firmware now uses DIN=10, BCK=11, LRCK=12, and GPIO9 is a general-I/O
   connector pin. Confirm the three DAC pins against the schematic.
2. **Firmware button/output mapping is stale.** `src/main.cpp` still treats
   GPIO15/16 as buttons mapped to "outputs" GPIO6/7. On the real board those
   are the **opto inputs** and the MOSFETs. The buttons are **GPIO13, 14, 21,
   47**; the MOSFET outputs (GPIO4–7) are **active-LOW**. The firmware should
   be reconciled to match (the docs here describe the hardware, not the current
   firmware).
3. **Current limits to fill in:** per-channel MOSFET rating, 9–12 V entry point,
   and the 3.3 V regulator's max current.

### Confirmed (resolved)
- ✅ **Buttons** are GPIO **13, 14, 21, 47**.
- ✅ **Opto inputs** are GPIO **15, 16** (isolated only — not buttons).
- ✅ **MOSFET outputs** are **active-LOW**: a 10 kΩ pull-up idles them off;
  driving the GPIO LOW switches an optocoupler that turns the MOSFET on.
- ✅ **VCC = 3.3 V**; the high-power domain is **9–12 V**.

---

## See also

- [Getting started & serial console reference](GETTING-STARTED.md)
- [Contributing](../CONTRIBUTING.md)
