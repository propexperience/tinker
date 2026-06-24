/*
 * PropX Tinker — Example 03: NeoPixel RGB
 * ------------------------------------------------
 * Cycles the on-board RGB LED through a color palette and a smooth rainbow.
 *
 * Pin: data = GPIO48   (one WS2812-style pixel)
 *
 * Library: Adafruit NeoPixel
 */

#include <Adafruit_NeoPixel.h>

static const int RGB_PIN    = 48;
static const int NUM_PIXELS = 1;
static const uint8_t BRIGHTNESS = 60;   // 0..255 (keep modest; these are bright)

Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// A few named colors to step through.
const uint32_t PALETTE[] = {
  0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00,
  0x00FFFF, 0x0000FF, 0x8B00FF, 0xFFFFFF,
};
const int PALETTE_LEN = sizeof(PALETTE) / sizeof(PALETTE[0]);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nPropX Tinker - Example 03: NeoPixel RGB");
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.clear();
  pixels.show();
}

void loop() {
  // 1) Step through the named palette.
  for (int i = 0; i < PALETTE_LEN; i++) {
    pixels.setPixelColor(0, PALETTE[i]);
    pixels.show();
    delay(400);
  }

  // 2) Smooth rainbow sweep using the library's hue helper.
  for (uint16_t hue = 0; hue < 65535; hue += 256) {
    pixels.setPixelColor(0, pixels.gamma32(pixels.ColorHSV(hue)));
    pixels.show();
    delay(5);
  }
}
