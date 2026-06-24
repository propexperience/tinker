/*
 * PropX Tinker — Example 05: Audio DAC (PCM5102A over I2S)
 * --------------------------------------------------------------------
 * Plays an MP3 from the SD card through the PCM5102A I2S DAC.
 *
 * Pins:
 *   SD (SPI) : MISO=40 MOSI=38 SCK=39 CS=41
 *   I2S DAC  : DIN=10  BCK=11  LRCK=12
 *
 * Put an MP3 named  /test.mp3  on the SD card, or use the serial commands.
 * Serial Monitor @ 115200:
 *   play          - play /test.mp3
 *   play <path>   - play a specific file (e.g. play /music/song.mp3)
 *   stop          - stop
 *   vol <1-20>    - set volume
 *
 * Library: ESP8266Audio (by Earle F. Philhower) - works on ESP32 too.
 *
 * NOTE: PCM5102A is a DAC (line-level), not an amplifier. Connect powered
 *       speakers, headphones, or an amp to its output.
 */

#include <SPI.h>
#include <SD.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

static const int SD_MISO = 40, SD_MOSI = 38, SD_SCK = 39, SD_CS = 41;
static const int I2S_DIN = 10, I2S_BCK = 11, I2S_LRCK = 12;

SPIClass spiSD(FSPI);
AudioGeneratorMP3* mp3  = nullptr;
AudioFileSourceSD* file = nullptr;
AudioOutputI2S*    out  = nullptr;
int volume = 6;   // 1..20

void applyVolume() {
  if (volume < 1) volume = 1;
  if (volume > 20) volume = 20;
  if (out) out->SetGain((float)volume / 20.0f);   // 0.05 .. 1.0
  Serial.printf("  volume %d/20\n", volume);
}

void stopPlay() {
  if (mp3 && mp3->isRunning()) mp3->stop();
  if (file) { delete file; file = nullptr; }
}

void startPlay(const char* path) {
  stopPlay();
  if (!SD.exists(path)) { Serial.printf("  X not found: %s\n", path); return; }
  file = new AudioFileSourceSD(path);
  if (file && mp3->begin(file, out)) Serial.printf("  > playing %s\n", path);
  else { stopPlay(); Serial.printf("  X failed to play %s\n", path); }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nPropX Tinker - Example 05: Audio DAC");

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD, 4000000)) {
    Serial.println("X SD mount failed - audio needs the SD card.");
    return;
  }
  Serial.println("OK SD mounted.");

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCK, I2S_LRCK, I2S_DIN);   // (bclk, lrclk, dout)
  mp3 = new AudioGeneratorMP3();
  applyVolume();

  Serial.println("Commands: play [path] | stop | vol <1-20>");
  Serial.print("> ");
}

void loop() {
  // Pump the decoder; detect end of song.
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) { stopPlay(); Serial.println("\n  done\n> "); }
  }

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (!line.length()) { return; }
    Serial.println(line);
    int sp = line.indexOf(' ');
    String cmd = sp < 0 ? line : line.substring(0, sp);
    String arg = sp < 0 ? ""   : line.substring(sp + 1);
    cmd.toLowerCase(); arg.trim();

    if (cmd == "play")      startPlay(arg.length() ? arg.c_str() : "/test.mp3");
    else if (cmd == "stop") { stopPlay(); Serial.println("  stopped"); }
    else if (cmd == "vol")  { volume = arg.toInt(); applyVolume(); }
    else                    Serial.println("  Commands: play [path] | stop | vol <1-20>");
    Serial.print("> ");
  }
}
