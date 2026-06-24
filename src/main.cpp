// SD Card Listing + Button/Output Harness — ESP32-S3 (SPI)
//
// SD wiring (SPI):
//   MISO → GPIO40   MOSI → GPIO38   SCK → GPIO39   CS → GPIO41
//
// Buttons (external 10k pull-up → active LOW when pressed):
//   GPIO 13, 14, 15, 16, 21, 47
//
// Outputs (external 10k pull-up → idle HIGH, driven LOW when active):
//   GPIO 4, 5, 6, 7
//
// Button → output mapping:
//   13→4   14→5   15→6   16→7   (21 and 47 are logged only)
//
// Serial console @ 115200:  'ls' re-list root, 'mount' retry SD mount.

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Adafruit_NeoPixel.h>
#include <AudioFileSourceSD.h>   // earlephilhower/ESP8266Audio
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

// ── SD card SPI pins ────────────────────────────────────────────────
static constexpr int SD_MISO = 40;
static constexpr int SD_MOSI = 38;
static constexpr int SD_SCK  = 39;
static constexpr int SD_CS   = 41;

static SPIClass spiSD(FSPI);
static bool g_mounted = false;

// ── OLED (SSD1306, I2C) ─────────────────────────────────────────────
static constexpr int OLED_SDA   = 18;
static constexpr int OLED_SCL   = 8;
static constexpr int OLED_W     = 128;
static constexpr int OLED_H     = 64;   // use 32 if you have a 128x32 panel
static constexpr uint8_t OLED_ADDR = 0x3C;  // common; some are 0x3D

static Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
static bool g_oledOk = false;

// ── RGB (NeoPixel) ──────────────────────────────────────────────────
static constexpr int RGB_PIN    = 48;
static constexpr int NUM_PIXELS = 2;
static Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// 10-color cycle palette
static const uint32_t PALETTE[] = {
    0xFF0000,  // red
    0xFF7F00,  // orange
    0xFFFF00,  // yellow
    0x00FF00,  // green
    0x00FFFF,  // cyan
    0x0000FF,  // blue
    0x8B00FF,  // violet
    0xFF00FF,  // magenta
    0xFFFFFF,  // white
    0x000000,  // off
};
static constexpr int PALETTE_LEN = sizeof(PALETTE) / sizeof(PALETTE[0]);

// ── I2S Audio (MP3 player) ──────────────────────────────────────────
static constexpr int I2S_DOUT = 10;    // amp DIN  (data out of ESP32)
static constexpr int I2S_BCLK = 11;   // bit clock (CLK)
static constexpr int I2S_LRC  = 12;   // word/LR clock (LRCK)

static AudioGeneratorMP3* g_mp3  = nullptr;
static AudioFileSourceSD* g_file = nullptr;
static AudioOutputI2S*    g_out  = nullptr;

// "box" file browser state
static bool   g_box = false;          // browser mode active
static String g_cwd = "/";            // current directory
static constexpr int MAX_TRACKS = 64;
static String g_tracks[MAX_TRACKS];   // numbered .mp3 files in g_cwd
static int    g_trackCount = 0;
static int    g_playing = -1;         // track index currently playing, or -1
static int    g_volume  = 6;          // 1..20 (maps to AudioOutputI2S gain)

// ── Buttons & outputs ───────────────────────────────────────────────
struct Button {
    int  inGpio;       // button input pin (active LOW)
    int  outGpio;      // mapped output pin, or -1 if none
    int  pixel;        // NeoPixel index to cycle, or -1 if none
    int  colorIdx;     // current palette index for that pixel
    bool pressed;      // debounced state: true = currently pressed
    int  lastRaw;      // last raw read (HIGH/LOW)
    unsigned long lastChangeMs;
};

static Button g_buttons[] = {
    {13, 4,  -1, 0, false, HIGH, 0},
    {14, 5,  -1, 0, false, HIGH, 0},
    {15, 6,  -1, 0, false, HIGH, 0},
    {16, 7,  -1, 0, false, HIGH, 0},
    {21, -1,  0, 0, false, HIGH, 0},   // cycles NeoPixel 0
    {47, -1,  1, 0, false, HIGH, 0},   // cycles NeoPixel 1
};
static constexpr int NUM_BUTTONS = sizeof(g_buttons) / sizeof(g_buttons[0]);
static constexpr unsigned long DEBOUNCE_MS = 25;

// Output idle/active levels (10k pull-up → idle HIGH, active LOW)
static constexpr int OUT_IDLE   = HIGH;
static constexpr int OUT_ACTIVE = LOW;

// ── General-purpose I/O (jumper- and/or code-configurable pull) ─────
// Free pins for tinkering: set direction + internal pull from the console.
// NOTE: GPIO3 and GPIO17 have NO external pull resistor — they float when
// left as plain INPUT, so use 'up'/'down' (internal pull) or a jumper.
static const int GP_PINS[] = {1, 2, 3, 9, 17, 42};
static constexpr int NUM_GP = sizeof(GP_PINS) / sizeof(GP_PINS[0]);
static int g_gpMode[NUM_GP];   // current Arduino pin mode per GP pin

// ── SD helpers ──────────────────────────────────────────────────────
static const char* cardTypeName(uint8_t type) {
    switch (type) {
        case CARD_NONE: return "NONE";
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SDSC";
        case CARD_SDHC: return "SDHC";
        default:        return "UNKNOWN";
    }
}

static bool mountCard() {
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, spiSD, 4000000)) {
        Serial.println("  ✗ SD.begin() failed — check wiring, CS pin, and card seating.");
        return false;
    }
    uint8_t type = SD.cardType();
    if (type == CARD_NONE) {
        Serial.println("  ✗ No SD card detected.");
        return false;
    }
    uint64_t sizeMB = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.printf("  ✓ Mounted %s card, %llu MB\n", cardTypeName(type), sizeMB);
    return true;
}

static void listRoot() {
    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("  ✗ Failed to open root directory.");
        if (root) root.close();
        return;
    }
    Serial.println("  Root directory contents:");
    Serial.println("  TYPE  SIZE        NAME");
    Serial.println("  ----  ----------  ------------------------------");
    int count = 0;
    for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
        if (entry.isDirectory())
            Serial.printf("  DIR   %10s  %s/\n", "-", entry.name());
        else
            Serial.printf("  FILE  %10u  %s\n", (unsigned)entry.size(), entry.name());
        count++;
        entry.close();
    }
    root.close();
    Serial.printf("  %d entr%s in root.\n", count, count == 1 ? "y" : "ies");
}

// ── OLED helpers ────────────────────────────────────────────────────
static bool initOled() {
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("  ✗ SSD1306 init failed — check SDA/SCL wiring and I2C address.");
        return false;
    }
    return true;
}

// Draw the PropX lockup: bold "PROP" on the left, a large stylized "X" on the right.
static void drawPropXLogo() {
    if (!g_oledOk) return;
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);

    // Geometry of the "X" block and the gap to "PROP".
    const int Xw  = 46;   // width of the X
    const int t   = 12;   // stroke thickness
    const int yt  = 6;    // top of the X
    const int yb  = 58;   // bottom of the X
    const int gap = 3;    // space between PROP and X

    // Measure "PROP" so we can center the whole lockup as one unit.
    oled.setFont(&FreeSansBold9pt7b);
    oled.setTextSize(1);
    int16_t bx, by; uint16_t bw, bh;
    oled.getTextBounds("PROP", 0, 0, &bx, &by, &bw, &bh);

    const int total = (int)bw + gap + Xw;
    int startX = (OLED_W - total) / 2;
    if (startX < 0) startX = 0;

    // ── "PROP" on the left, vertically centered ──
    int16_t px = startX - bx;
    int16_t py = (OLED_H - (int)bh) / 2 - by;
    oled.setCursor(px, py);
    oled.print("PROP");
    oled.setFont(nullptr);  // restore default font

    // ── Big "X" immediately to the right of PROP ──
    const int xl = startX + (int)bw + gap;
    const int xr = xl + Xw;
    // Bar 1: top-left → bottom-right (two triangles forming a thick quad)
    oled.fillTriangle(xl, yt, xl + t, yt, xr, yb, SSD1306_WHITE);
    oled.fillTriangle(xl, yt, xr, yb, xr - t, yb, SSD1306_WHITE);
    // Bar 2: top-right → bottom-left
    oled.fillTriangle(xr - t, yt, xr, yt, xl + t, yb, SSD1306_WHITE);
    oled.fillTriangle(xr - t, yt, xl + t, yb, xl, yb, SSD1306_WHITE);

    oled.display();
    Serial.println("  ✓ OLED showing PropX logo");
}

// Draw text using the largest built-in font scale that still fits, centered.
static void showBigText(const char* msg) {
    if (!g_oledOk) return;
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);

    int best = 1;
    for (int s = 8; s >= 1; s--) {
        oled.setTextSize(s);
        int16_t x1, y1; uint16_t w, h;
        oled.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
        if (w <= OLED_W && h <= OLED_H) { best = s; break; }
    }

    oled.setTextSize(best);
    int16_t x1, y1; uint16_t w, h;
    oled.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (OLED_W - (int)w) / 2 - x1;
    int16_t y = (OLED_H - (int)h) / 2 - y1;
    oled.setCursor(x, y);
    oled.print(msg);
    oled.display();
    Serial.printf("  ✓ OLED showing \"%s\" at text size %d\n", msg, best);
}

// ── Button helpers ──────────────────────────────────────────────────
static void setupButtons() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        // External 10k pull-ups present, but enable internal too for safety.
        pinMode(g_buttons[i].inGpio, INPUT_PULLUP);
        g_buttons[i].lastRaw = digitalRead(g_buttons[i].inGpio);
        g_buttons[i].pressed = (g_buttons[i].lastRaw == LOW);

        if (g_buttons[i].outGpio >= 0) {
            pinMode(g_buttons[i].outGpio, OUTPUT);
            digitalWrite(g_buttons[i].outGpio, OUT_IDLE);
        }
    }
    Serial.println("Buttons armed:");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (g_buttons[i].outGpio >= 0)
            Serial.printf("  GPIO%-2d  →  drives GPIO%d\n", g_buttons[i].inGpio, g_buttons[i].outGpio);
        else if (g_buttons[i].pixel >= 0)
            Serial.printf("  GPIO%-2d  →  cycles RGB LED%d\n", g_buttons[i].inGpio, g_buttons[i].pixel);
        else
            Serial.printf("  GPIO%-2d  →  (log only)\n", g_buttons[i].inGpio);
    }
}

static void pollButtons() {
    unsigned long now = millis();
    for (int i = 0; i < NUM_BUTTONS; i++) {
        Button& b = g_buttons[i];
        int raw = digitalRead(b.inGpio);

        if (raw != b.lastRaw) {
            b.lastRaw = raw;
            b.lastChangeMs = now;          // start/restart debounce window
        }

        // Accept the reading once it's been stable past the debounce window.
        if ((now - b.lastChangeMs) >= DEBOUNCE_MS) {
            bool nowPressed = (raw == LOW);
            if (nowPressed != b.pressed) {
                b.pressed = nowPressed;

                if (nowPressed) {
                    if (b.outGpio >= 0) {
                        digitalWrite(b.outGpio, OUT_ACTIVE);
                        Serial.printf("[BUTTON] GPIO%-2d PRESSED  → GPIO%d active (LOW)\n",
                                      b.inGpio, b.outGpio);
                    } else if (b.pixel >= 0) {
                        b.colorIdx = (b.colorIdx + 1) % PALETTE_LEN;
                        uint32_t c = PALETTE[b.colorIdx];
                        pixels.setPixelColor(b.pixel, c);
                        pixels.show();
                        Serial.printf("[BUTTON] GPIO%-2d PRESSED  → LED%d color %d/%d (#%06X)\n",
                                      b.inGpio, b.pixel, b.colorIdx + 1, PALETTE_LEN, (unsigned)c);
                    } else {
                        Serial.printf("[BUTTON] GPIO%-2d PRESSED\n", b.inGpio);
                    }
                } else {
                    if (b.outGpio >= 0) {
                        digitalWrite(b.outGpio, OUT_IDLE);
                        Serial.printf("[BUTTON] GPIO%-2d released → GPIO%d idle (HIGH)\n",
                                      b.inGpio, b.outGpio);
                    } else if (b.pixel < 0) {
                        Serial.printf("[BUTTON] GPIO%-2d released\n", b.inGpio);
                    }
                    // pixel-cycle buttons: nothing to do on release
                }
                Serial.print("> ");
            }
        }
    }
}

// ── General-purpose I/O helpers ─────────────────────────────────────
static int gpIndex(int pin) {
    for (int i = 0; i < NUM_GP; i++) if (GP_PINS[i] == pin) return i;
    return -1;
}

static const char* gpModeName(int mode) {
    switch (mode) {
        case OUTPUT:         return "OUTPUT";
        case INPUT:          return "INPUT";
        case INPUT_PULLUP:   return "INPUT_PULLUP";
        case INPUT_PULLDOWN: return "INPUT_PULLDOWN";
        default:             return "?";
    }
}

static void setupGpPins() {
    for (int i = 0; i < NUM_GP; i++) {
        pinMode(GP_PINS[i], INPUT);   // high-Z so an external jumper defines the level
        g_gpMode[i] = INPUT;
    }
    Serial.println("GPIO (configurable) pins ready:");
    Serial.print("  ");
    for (int i = 0; i < NUM_GP; i++) Serial.printf("GPIO%d ", GP_PINS[i]);
    Serial.println("\n  gpio                 — list state");
    Serial.println("  gpio <pin> in|up|down|out  — set mode");
    Serial.println("  gpio <pin> hi|lo|tog|rd    — drive / read");
}

static void listGp() {
    Serial.println("  GPIO  Mode             Level");
    Serial.println("  ----  ---------------  -----");
    for (int i = 0; i < NUM_GP; i++) {
        int lvl = digitalRead(GP_PINS[i]);
        Serial.printf("  %4d  %-15s  %s\n",
                      GP_PINS[i], gpModeName(g_gpMode[i]), lvl ? "HIGH" : "LOW");
    }
}

// Handle "gpio <pin> <action>" (action empty → list).
static void handleGpio(const String& arg) {
    if (arg.isEmpty()) { listGp(); return; }

    int sp = arg.indexOf(' ');
    int pin = (sp < 0 ? arg : arg.substring(0, sp)).toInt();
    String act = (sp < 0) ? "" : arg.substring(sp + 1);
    act.trim();
    act.toLowerCase();

    int idx = gpIndex(pin);
    if (idx < 0) {
        Serial.printf("  ✗ GPIO%d is not a configurable pin (use: ", pin);
        for (int i = 0; i < NUM_GP; i++) Serial.printf("%d ", GP_PINS[i]);
        Serial.println(").");
        return;
    }

    if (act.isEmpty() || act == "rd") {
        int lvl = digitalRead(pin);
        Serial.printf("  GPIO%d = %s  (%s)\n", pin, lvl ? "HIGH" : "LOW", gpModeName(g_gpMode[idx]));
    } else if (act == "in" || act == "up" || act == "down" || act == "out") {
        int mode = (act == "out")  ? OUTPUT :
                   (act == "up")   ? INPUT_PULLUP :
                   (act == "down") ? INPUT_PULLDOWN : INPUT;
        pinMode(pin, mode);
        g_gpMode[idx] = mode;
        if (mode == OUTPUT) digitalWrite(pin, LOW);
        int lvl = digitalRead(pin);
        Serial.printf("  GPIO%d → %s  (level=%s)\n", pin, gpModeName(mode), lvl ? "HIGH" : "LOW");
    } else if (act == "hi" || act == "lo" || act == "tog") {
        if (g_gpMode[idx] != OUTPUT) {
            Serial.printf("  GPIO%d is not an OUTPUT. Run 'gpio %d out' first.\n", pin, pin);
            return;
        }
        int val = (act == "hi") ? HIGH :
                  (act == "lo") ? LOW  : !digitalRead(pin);
        digitalWrite(pin, val);
        Serial.printf("  GPIO%d = %s\n", pin, val ? "HIGH" : "LOW");
    } else {
        Serial.printf("  ✗ Unknown action '%s' (in|up|down|out|hi|lo|tog|rd)\n", act.c_str());
    }
}

// ── Audio "box" browser ─────────────────────────────────────────────
static bool endsWithMp3(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".mp3");
}

// Join a directory and a child name into a clean absolute path.
static String joinPath(const String& dir, const String& name) {
    if (dir == "/") return "/" + name;
    return dir + "/" + name;
}

// List the current directory: sub-dirs first, then numbered .mp3 files.
static void scanBox() {
    g_trackCount = 0;
    File dir = SD.open(g_cwd);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("  ✗ Cannot open %s\n", g_cwd.c_str());
        if (dir) dir.close();
        return;
    }

    Serial.printf("\n  ┌─ %s\n", g_cwd.c_str());

    // First pass: directories
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        if (e.isDirectory()) {
            // entry.name() may be a full path on some cores — show last segment
            String n = e.name();
            int slash = n.lastIndexOf('/');
            if (slash >= 0) n = n.substring(slash + 1);
            if (!n.startsWith("."))   // skip hidden / dot directories
                Serial.printf("  │  [DIR]  %s/\n", n.c_str());
        }
        e.close();
    }

    // Second pass: numbered mp3 files
    dir.rewindDirectory();
    for (File e = dir.openNextFile(); e && g_trackCount < MAX_TRACKS; e = dir.openNextFile()) {
        if (!e.isDirectory()) {
            String n = e.name();
            int slash = n.lastIndexOf('/');
            if (slash >= 0) n = n.substring(slash + 1);
            if (!n.startsWith(".") && endsWithMp3(n)) {
                g_tracks[g_trackCount] = n;
                Serial.printf("  │  %3d.   %s\n", g_trackCount + 1, n.c_str());
                g_trackCount++;
            }
        }
        e.close();
    }
    dir.close();

    if (g_trackCount == 0) Serial.println("  │  (no MP3 files here)");
    Serial.println("  └─ cd <dir> | cd .. | play <n> | stop | exit");
}

static void boxChangeDir(const String& arg) {
    if (arg == "..") {
        if (g_cwd == "/") { Serial.println("  Already at root."); return; }
        int slash = g_cwd.lastIndexOf('/');
        g_cwd = (slash <= 0) ? "/" : g_cwd.substring(0, slash);
        scanBox();
        return;
    }
    String target = joinPath(g_cwd, arg);
    File d = SD.open(target);
    if (d && d.isDirectory()) {
        g_cwd = target;
        d.close();
        scanBox();
    } else {
        if (d) d.close();
        Serial.printf("  ✗ No such directory: %s\n", arg.c_str());
    }
}

// Set volume on a 1..20 scale (20 → unity gain). Applies live.
static void setVolume(int v) {
    if (v < 1)  v = 1;
    if (v > 20) v = 20;
    g_volume = v;
    if (g_out) g_out->SetGain((float)v / 20.0f);   // 0.05 .. 1.0
    Serial.printf("  🔊 Volume %d/20\n", g_volume);
}

// Tear down the current decoder + file source (safe to call when idle).
static void audioStop() {
    if (g_mp3 && g_mp3->isRunning()) g_mp3->stop();
    if (g_file) { delete g_file; g_file = nullptr; }
}

static void boxPlay(const String& arg) {
    int n = arg.toInt();
    if (n < 1 || n > g_trackCount) {
        Serial.printf("  ✗ No track %s (have 1..%d).\n", arg.c_str(), g_trackCount);
        return;
    }
    String path = joinPath(g_cwd, g_tracks[n - 1]);
    audioStop();   // stop whatever was playing

    g_file = new AudioFileSourceSD(path.c_str());
    if (g_file && g_mp3->begin(g_file, g_out)) {
        g_playing = n - 1;
        Serial.printf("  ▶ Playing %d: %s\n", n, g_tracks[n - 1].c_str());
    } else {
        audioStop();
        g_playing = -1;
        Serial.printf("  ✗ Failed to play %s\n", path.c_str());
    }
}

static void boxStop() {
    if (g_playing < 0) { Serial.println("  Nothing playing."); return; }
    Serial.printf("  ■ Stopped %s\n", g_tracks[g_playing].c_str());
    audioStop();
    g_playing = -1;
}

// Pump the decoder; detect natural end-of-track. Call every loop().
static void audioPump() {
    if (g_mp3 && g_mp3->isRunning()) {
        if (!g_mp3->loop()) {        // returns false when the song ends
            audioStop();
            if (g_playing >= 0)
                Serial.printf("\n  ✓ Finished: %s\n> ", g_tracks[g_playing].c_str());
            g_playing = -1;
        }
    }
}

// ── Main ────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n═════════════════════════════════════════");
    Serial.println("     SD + Button/Output Harness — ESP32-S3 ");
    Serial.println("═════════════════════════════════════════\n");
    Serial.printf("SD pins: MISO=%d MOSI=%d SCK=%d CS=%d\n\n", SD_MISO, SD_MOSI, SD_SCK, SD_CS);

    g_mounted = mountCard();
    if (g_mounted) { Serial.println(); listRoot(); }

    Serial.println();
    Serial.printf("OLED pins: SDA=%d SCL=%d (addr 0x%02X)\n", OLED_SDA, OLED_SCL, OLED_ADDR);
    g_oledOk = initOled();
    if (g_oledOk) drawPropXLogo();

    Serial.println();
    Serial.printf("RGB (NeoPixel): pin=%d, %d pixels\n", RGB_PIN, NUM_PIXELS);
    pixels.begin();
    pixels.clear();
    pixels.show();

    Serial.println();
    setupButtons();

    Serial.println();
    setupGpPins();

    Serial.println();
    Serial.printf("I2S audio: DOUT=%d BCLK=%d LRC=%d\n", I2S_DOUT, I2S_BCLK, I2S_LRC);
    g_out = new AudioOutputI2S();
    g_out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);  // (bclk, lrclk, dout)
    g_mp3 = new AudioGeneratorMP3();
    setVolume(g_volume);   // apply initial volume (1..20)

    Serial.println("\nType 'ls'/'mount' for SD, or 'box' for the MP3 player.");
    Serial.print("> ");
}

void loop() {
    audioPump();       // must run continuously to stream MP3 audio
    pollButtons();

    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) { Serial.print("> "); return; }
    Serial.println(line);

    // Split into command word (case-insensitive) and original-case argument.
    int sp = line.indexOf(' ');
    String cmd = (sp < 0) ? line : line.substring(0, sp);
    String arg = (sp < 0) ? ""   : line.substring(sp + 1);
    arg.trim();
    cmd.toLowerCase();

    if (g_box) {
        // ── MP3 player mode ──
        if (cmd == "ls")        { scanBox(); }
        else if (cmd == "cd")   { if (arg.isEmpty()) Serial.println("  Usage: cd <dir> | cd .."); else boxChangeDir(arg); }
        else if (cmd == "play") { boxPlay(arg); }
        else if (cmd == "stop") { boxStop(); }
        else if (cmd == "vol")  { if (arg.isEmpty()) Serial.printf("  Volume %d/20 (usage: vol <1-20>)\n", g_volume); else setVolume(arg.toInt()); }
        else if (cmd == "exit" || cmd == "quit") { boxStop(); g_box = false; Serial.println("  Left box. Type 'box' to re-enter."); }
        else if (cmd == "h" || cmd == "help" || cmd == "?") {
            Serial.println("  box commands: ls | cd <dir> | cd .. | play <n> | stop | vol <1-20> | exit");
        }
        else { Serial.println("  box: ls | cd <dir> | cd .. | play <n> | stop | vol <1-20> | exit"); }
    } else {
        // ── Top-level mode ──
        if (cmd == "ls") {
            if (g_mounted) listRoot();
            else Serial.println("  Card not mounted. Type 'mount' first.");
        } else if (cmd == "mount") {
            if (g_mounted) { SD.end(); g_mounted = false; }
            g_mounted = mountCard();
            if (g_mounted) { Serial.println(); listRoot(); }
        } else if (cmd == "box") {
            if (!g_mounted) { Serial.println("  Card not mounted. Type 'mount' first."); }
            else { g_box = true; g_cwd = "/"; Serial.println("  Entered box (MP3 player)."); scanBox(); }
        } else if (cmd == "gpio") {
            handleGpio(arg);
        } else {
            Serial.println("  Commands: ls | mount | box | gpio [<pin> <action>]");
        }
    }
    Serial.print("> ");
}
