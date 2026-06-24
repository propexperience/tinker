# Example 07 — Full Hardware Test (Web)

One web app that exercises the **entire board** over its own Wi-Fi hotspot.
Use it to verify a freshly built board: watch inputs, fire outputs, play media,
and drive the RGB — all from your phone or laptop.

> 📷 **Image placeholder:** the three tabs on a phone → `../../docs/images/hwtest-tabs.png`

## Three tabs

| Tab | What it does |
|---|---|
| **I/O** | Live HIGH/LOW status of all 4 buttons + 2 opto inputs; **on/off buttons** to manually trigger each MOSFET output |
| **Media** | The SD-card MP3 player — browse folders, play a file, **play random**, or **play all (inline)**, with volume + next |
| **NeoPixel** | A **color picker + on/off** for each RGB pixel |

## Pins exercised

| Group | GPIO |
|---|---|
| SD (SPI) | MISO 40, MOSI 38, SCK 39, CS 41 |
| Audio (I²S) | DIN 10, BCK 11, LRCK 12 |
| NeoPixel | data 48 |
| Buttons | 13, 14, 21, 47 |
| Opto inputs | 15, 16 |
| MOSFET outputs | 4, 5, 6, 7 (**active-LOW**) |

## How to use

1. (Optional) Put MP3s on a **FAT32** SD card for the Media tab.
2. Flash the sketch; open the Serial Monitor at **115200** to watch it boot.
3. Join the Wi-Fi network:
   - **SSID:** `PropX-Tinker`  **Password:** `tinker123`
4. Open **http://192.168.4.1**
5. Switch tabs and test:
   - **I/O:** press a button or trigger an opto input — its pill flips to
     **ACTIVE / LOW**. Tap **Turn on** to fire a MOSFET output.
   - **Media:** browse and play (audio comes out the DAC — use powered speakers).
   - **NeoPixel:** pick a color and toggle each pixel on/off.

## The inputs read "active = LOW"

Buttons and opto inputs are **active-LOW** (internal pull-up; the signal pulls
them LOW). The I/O tab shows:

- **idle / HIGH** — nothing pressed/applied
- **ACTIVE / LOW** — button pressed or opto input energised

The MOSFET outputs are **active-LOW** too — the page hides that detail and just
shows **ON / off**; internally "on" drives the GPIO LOW.

## NeoPixel: how many lights?

This chain has **2 LEDs** on GPIO48, so the NeoPixel tab shows **two**
independent color controls — one per pixel:

```cpp
#define NUM_PIXELS 2   // 2 LEDs in the chain on GPIO48
```

Change this if you add or remove pixels and the UI updates to match. Each light
has its own **native color picker** (your device's built-in color wheel) plus an
on/off toggle.

## Notes & limits

- **SoftAP** is best for **one** client at a time — it's a test tool, not a
  server.
- Audio is **line level** (PCM5102A) — connect powered speakers / an amp.
- The status display polls every ~1.5 s; input changes appear within that.
- If the **Media** tab is empty, the SD card didn't mount — check the card and
  see [Example 01](../01-sd-card).

## Libraries

- **ESP8266Audio** (Earle F. Philhower)
- **Adafruit NeoPixel**
- `WiFi` + `WebServer` are built into the ESP32 core.
