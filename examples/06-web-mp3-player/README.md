# Example 06 — Web MP3 Player

A small web app that ties together the **SD card**, the **PCM5102A audio DAC**,
and **Wi-Fi**. The board hosts its own Wi-Fi hotspot; you connect a phone or
laptop, open a web page, browse the SD card's folders, and play music.

> 📷 **Image placeholder:** screenshot of the web UI on a phone → `../../docs/images/web-mp3-ui.png`

## What you can do

- **Browse** the SD card's folder structure and navigate in/out of folders.
- **Play a single MP3** by tapping it.
- **Play random** — shuffle every MP3 in the current folder.
- **Play inline** — play every MP3 in the current folder, in order.
- Adjust **volume** and skip to the **next** track.

## Pins

| Role | GPIO |
|---|---|
| SD MISO / MOSI / SCK / CS | 40 / 38 / 39 / 41 |
| I²S DIN / BCK / LRCK | 10 / 11 / 12 |

## How to use

1. Put some `.mp3` files (in folders if you like) on a **FAT32** SD card.
2. Flash the sketch and open the Serial Monitor at **115200** to watch it boot.
3. On your phone/laptop, join the Wi-Fi network:
   - **SSID:** `PropX-Tinker`
   - **Password:** `tinker123`
4. Open a browser to **http://192.168.4.1**
5. Browse and play. Audio comes out of the **DAC** (connect powered speakers /
   an amp).

## Configuration (top of the sketch)

| Setting | Default | Notes |
|---|---|---|
| `AP_SSID` | `PropX-Tinker` | hotspot name |
| `AP_PASS` | `tinker123` | ≥ 8 chars, or `""` for an open network |
| `I2S_DIN/BCK/LRCK` | 10 / 11 / 12 | DAC pins |
| `volume` | 8 | 1–20 startup volume |

## How it works

- **SoftAP:** `WiFi.softAP()` creates a self-contained hotspot at
  `192.168.4.1` — no router needed.
- **Web server:** the built-in `WebServer` serves one HTML page plus a small
  JSON API:

  | Endpoint | Purpose |
  |---|---|
  | `/api/list?path=/dir` | folders + MP3s in a directory (JSON) |
  | `/api/play?file=/x.mp3` | play one file |
  | `/api/playrandom?dir=/d` | shuffle a folder |
  | `/api/playinline?dir=/d` | play a folder in order |
  | `/api/next` | skip to next track |
  | `/api/stop` | stop |
  | `/api/vol?v=12` | set volume |
  | `/api/status` | what's playing (polled by the page) |

- **Audio + web together:** `mp3->loop()` is pumped every `loop()` alongside
  `server.handleClient()`, so playback keeps streaming while the page responds.
- **Playlists:** for inline/random, the sketch scans the folder into a list and
  advances automatically when a track ends (`onTrackEnd()`).

## Limits & notes

- It's a **DAC** (line level) — use powered speakers / an amp.
- Up to `MAX_FILES` (256) MP3s per folder are listed; raise it if needed.
- Folder scan is **non-recursive** (one level at a time, as you navigate).
- Hidden files (names starting with `.`) are skipped.
- SoftAP serves one client comfortably; it's a demo, not a media server.

## Library

- **ESP8266Audio** (Earle F. Philhower). `WiFi` and `WebServer` are built in.
