/*
 * PropX Tinker — Example 02: SSD1306 OLED (I2C)
 * ------------------------------------------------
 * Initialises a 128x64 SSD1306 OLED and shows the PropX logo, then cycles
 * through a few demo screens.
 *
 * Pins (I2C): SDA=18  SCL=8   (display header or Connector B)
 * I2C address: 0x3C (some panels are 0x3D)
 *
 * Libraries: Adafruit GFX, Adafruit SSD1306
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>

static const int OLED_SDA = 18;
static const int OLED_SCL = 8;
static const int OLED_W   = 128;
static const int OLED_H   = 64;   // set to 32 for a 128x32 panel
static const uint8_t OLED_ADDR = 0x3C;

Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
bool ok = false;

// The PropX lockup: bold "PROP" + a big stylized "X", centered as one unit.
void drawLogo() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);

  const int Xw = 46, t = 12, yt = 6, yb = 58, gap = 3;
  oled.setFont(&FreeSansBold9pt7b);
  oled.setTextSize(1);
  int16_t bx, by; uint16_t bw, bh;
  oled.getTextBounds("PROP", 0, 0, &bx, &by, &bw, &bh);

  int total = (int)bw + gap + Xw;
  int startX = (OLED_W - total) / 2;
  if (startX < 0) startX = 0;

  oled.setCursor(startX - bx, (OLED_H - (int)bh) / 2 - by);
  oled.print("PROP");
  oled.setFont(nullptr);

  int xl = startX + (int)bw + gap, xr = xl + Xw;
  oled.fillTriangle(xl, yt, xl + t, yt, xr, yb, SSD1306_WHITE);
  oled.fillTriangle(xl, yt, xr, yb, xr - t, yb, SSD1306_WHITE);
  oled.fillTriangle(xr - t, yt, xr, yt, xl + t, yb, SSD1306_WHITE);
  oled.fillTriangle(xr - t, yt, xl + t, yb, xl, yb, SSD1306_WHITE);
  oled.display();
}

// Largest built-in font scale that fits, centered.
void bigText(const char* msg) {
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
  oled.setCursor((OLED_W - (int)w) / 2 - x1, (OLED_H - (int)h) / 2 - y1);
  oled.print(msg);
  oled.display();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nPropX Tinker - Example 02: OLED");

  Wire.begin(OLED_SDA, OLED_SCL);
  ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!ok) {
    Serial.println("X SSD1306 init failed - check SDA/SCL and the address (0x3C / 0x3D).");
    return;
  }
  Serial.println("OK - showing logo.");
  drawLogo();
}

void loop() {
  if (!ok) return;
  static int screen = 0;
  delay(2500);
  switch (screen % 3) {
    case 0: drawLogo();         break;
    case 1: bigText("HELLO");   break;
    case 2: bigText("TINKER");  break;
  }
  screen++;
}
