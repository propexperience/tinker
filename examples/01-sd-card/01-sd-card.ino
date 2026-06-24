/*
 * PropX Tinker — Example 01: microSD Card (SPI)
 * ------------------------------------------------
 * Mounts the microSD card, prints card info, lists the root directory,
 * and (if present) reads a file called /hello.txt.
 *
 * Pins (SPI): MISO=40  MOSI=38  SCK=39  CS=41
 *
 * Serial Monitor @ 115200. Type 'ls' to re-list, 'mount' to retry.
 *
 * Libraries: none beyond the built-in ESP32 SD library.
 */

#include <SPI.h>
#include <SD.h>

static const int SD_MISO = 40;
static const int SD_MOSI = 38;
static const int SD_SCK  = 39;
static const int SD_CS   = 41;

// A dedicated SPI bus so we don't clash with anything else.
SPIClass spiSD(FSPI);
bool mounted = false;

const char* cardTypeName(uint8_t t) {
  switch (t) {
    case CARD_MMC:  return "MMC";
    case CARD_SD:   return "SDSC";
    case CARD_SDHC: return "SDHC";
    default:        return "UNKNOWN";
  }
}

bool mountCard() {
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  // 4 MHz is a safe mounting speed for jumper wires; raise once it's reliable.
  if (!SD.begin(SD_CS, spiSD, 4000000)) {
    Serial.println("  X SD.begin() failed - check wiring, CS, and the card.");
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    Serial.println("  X No card detected.");
    return false;
  }
  Serial.printf("  OK Mounted %s card, %llu MB\n",
                cardTypeName(SD.cardType()),
                SD.cardSize() / (1024ULL * 1024ULL));
  return true;
}

void listRoot() {
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("  X Could not open root.");
    return;
  }
  Serial.println("  TYPE  SIZE        NAME");
  for (File e = root.openNextFile(); e; e = root.openNextFile()) {
    if (e.isDirectory()) Serial.printf("  DIR   %10s  %s/\n", "-", e.name());
    else                 Serial.printf("  FILE  %10u  %s\n", (unsigned)e.size(), e.name());
    e.close();
  }
  root.close();
}

void readHello() {
  if (!SD.exists("/hello.txt")) {
    Serial.println("  (no /hello.txt to read - create one to test file reads)");
    return;
  }
  File f = SD.open("/hello.txt");
  Serial.println("  --- /hello.txt ---");
  while (f.available()) Serial.write(f.read());
  Serial.println("\n  ------------------");
  f.close();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nPropX Tinker - Example 01: microSD Card");
  Serial.printf("Pins: MISO=%d MOSI=%d SCK=%d CS=%d\n\n", SD_MISO, SD_MOSI, SD_SCK, SD_CS);

  mounted = mountCard();
  if (mounted) { listRoot(); readHello(); }
  Serial.print("\n> ");
}

void loop() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();
  Serial.println(cmd);

  if (cmd == "ls") {
    if (mounted) listRoot(); else Serial.println("  Not mounted.");
  } else if (cmd == "mount") {
    if (mounted) { SD.end(); mounted = false; }
    mounted = mountCard();
    if (mounted) listRoot();
  } else if (cmd.length()) {
    Serial.println("  Commands: ls | mount");
  }
  Serial.print("> ");
}
