/*
 * PropX Tinker — Example 07: Full Hardware Test (Web)
 * --------------------------------------------------------------------
 * One web app that exercises the whole board over its own Wi-Fi hotspot.
 * Four tabs:
 *   1) I/O      - live HIGH/LOW status of every input; manually toggle outputs
 *   2) Media    - the SD-card MP3 player (browse / play / random / inline)
 *   3) NeoPixel - a color picker + on/off for each RGB pixel
 *   4) Screen   - type a message to show on the OLED
 *
 * The OLED shows the PropX logo at boot, the current track while audio
 * plays, and otherwise whatever message you typed on the Screen tab.
 *
 * Connect to Wi-Fi:  "PropX-Tinker"   (password: "tinker123")
 * Then open:         http://192.168.4.1
 *
 * Pins:
 *   SD (SPI) : MISO=40 MOSI=38 SCK=39 CS=41
 *   I2S DAC  : DIN=10  BCK=11  LRCK=12
 *   OLED I2C : SDA=18  SCL=8            (SSD1306, addr 0x3C)
 *   NeoPixel : data=48
 *   Buttons  : 13, 14, 21, 47        (active-LOW)
 *   Opto in  : 15, 16                (isolated)
 *   MOSFET   : 4, 5, 6, 7            (active-LOW outputs)
 *
 * Libraries: ESP8266Audio, Adafruit NeoPixel, Adafruit GFX + SSD1306.
 * WiFi + WebServer are built in.
 *
 * Architecture: the MP3 decoder runs in its own FreeRTOS task on core 1
 * (above loop() priority) so web requests never starve the audio, and the
 * WiFi stack keeps core 0. The web handlers and the audio task share the
 * SD card and player state, so all of that is guarded by audioMutex.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSerifBoldItalic12pt7b.h>
#include "AudioFileSourceSD.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// ── Config ──────────────────────────────────────────────────────────
const char* AP_SSID = "PropX-Tinker";
const char* AP_PASS = "tinker123";

static const int SD_MISO = 40, SD_MOSI = 38, SD_SCK = 39, SD_CS = 41;
static const int I2S_DIN = 10, I2S_BCK = 11, I2S_LRCK = 12;
static const int RGB_PIN = 48;
#define NUM_PIXELS 2        // 2 LEDs in the chain on GPIO48

// OLED (SSD1306, I2C) — same wiring as Example 02
static const int     OLED_SDA = 18, OLED_SCL = 8, OLED_W = 128, OLED_H = 64;
static const uint8_t OLED_ADDR = 0x3C;   // some panels are 0x3D

// Inputs (active-LOW) and outputs (active-LOW MOSFETs)
const int   BTN_PINS[]  = {13, 14, 21, 47};
const char* BTN_NAMES[] = {"Button 1", "Button 2", "Button 3", "Button 4"};
const int   OPT_PINS[]  = {15, 16};
const char* OPT_NAMES[] = {"Opto A", "Opto B"};
const int   OUT_PINS[]  = {4, 5, 6, 7};
const int   OUT_OFF = HIGH, OUT_ON = LOW;

// ── Globals ─────────────────────────────────────────────────────────
WebServer server(80);
SPIClass spiSD(FSPI);
Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306  oled(OLED_W, OLED_H, &Wire, -1);
bool oledOk = false;

// "PropX / Experience" is drawn once into this off-screen canvas at a big,
// smooth font, then scale-blitted (down only, so it stays crisp) both for
// the boot shrink and the resting splash. Built once in setup.
GFXcanvas1* gLock = nullptr;
int   gLockW=0, gLockH=0;
float gLockSmallScale=0.3f;     // scale of the settled small lockup
int   gLockSmallCY=12;          // its centre Y at the top of the panel
int   gAreaTop=26;              // first free row under the small lockup
int   gTinkCY=45;               // centre Y for "Tinker" below it

bool     outState[4] = {false, false, false, false};
bool     pxOn[NUM_PIXELS];
uint32_t pxColor[NUM_PIXELS];
uint8_t  brightness = 128;      // global NeoPixel brightness, 0-255

AudioGeneratorMP3*     mp3  = nullptr;
AudioFileSourceSD*     file = nullptr;
AudioFileSourceBuffer* buff = nullptr;   // RAM buffer between SD and decoder
AudioOutputI2S*        out  = nullptr;
int  volume = 8;
bool sdOk = false;

// Everything that touches the SD card or the player state (mp3/file/buff,
// playlist, playMode, nowPlaying, volume) runs under this mutex — the audio
// task and the web handlers race otherwise. Lock at the outermost level
// only (handlers and the task); the helpers below never lock themselves.
SemaphoreHandle_t audioMutex = nullptr;
void lockAudio()  { xSemaphoreTake(audioMutex, portMAX_DELAY); }
void unlockAudio(){ xSemaphoreGive(audioMutex); }

static const int AUDIO_BUF_BYTES = 16384;   // absorbs SD stalls from web handlers

enum PlayMode { PM_NONE, PM_SINGLE, PM_INLINE, PM_RANDOM };
PlayMode playMode = PM_NONE;
static const int MAX_FILES = 256;
String playlist[MAX_FILES];
int    plCount = 0, plIndex = -1;
String nowPlaying = "";

// OLED content. serviceScreen() in loop() does the actual (slow) I2C draw;
// it decides what to show — a playing track wins, else the typed message,
// else the boot logo. screenMsg is written by the Screen-tab handler and
// read by serviceScreen(), so it lives under audioMutex like the rest.
String screenMsg = "";
String screenSig = "";      // signature of what is currently on the panel

// ── Helpers ─────────────────────────────────────────────────────────
String baseName(const String& p){ int s=p.lastIndexOf('/'); return s>=0?p.substring(s+1):p; }
bool   isMp3(const String& n){ String l=n; l.toLowerCase(); return l.endsWith(".mp3"); }
bool   isHidden(const String& n){ return n.startsWith("."); }
String joinPath(const String& d,const String& n){ return (d=="/"||!d.length())?("/"+n):(d+"/"+n); }
String jsonEsc(const String& s){ String o; for(size_t i=0;i<s.length();i++){char c=s[i]; if(c=='"'||c=='\\'){o+='\\';o+=c;} else if(c=='\n')o+="\\n"; else o+=c;} return o; }
void   applyVolume(){ if(volume<1)volume=1; if(volume>20)volume=20; if(out)out->SetGain((float)volume/20.0f); }

// ── NeoPixel ────────────────────────────────────────────────────────
void renderPixels(){
  // setBrightness before setPixelColor: the library scales each color at
  // store time, so re-setting from the true pxColor[] each pass keeps
  // brightness changes lossless (no cumulative rounding).
  pixels.setBrightness(brightness);
  for(int i=0;i<NUM_PIXELS;i++) pixels.setPixelColor(i, pxOn[i]?pxColor[i]:0);
  pixels.show();
}

// ── OLED / Screen ───────────────────────────────────────────────────
// The splash animates by scale-blitting pre-rendered canvases (blitScaled):
// the "PropX / Experience" lockup shrinks to the top, then italic "Tinker"
// grows in below it. The lockup is rendered once at a big smooth font
// (FreeSansBold18pt) and only ever downscaled, so it never looks blocky.

// Nearest-neighbor scale-blit of a 1-bit canvas onto the OLED buffer,
// centered at (cx,cy). setTextSize only scales in integer steps; blitting
// from a high-res canvas lets the scale be a smooth float, keeps the italic
// face on "Tinker" at every size, and keeps the lockup crisp when small.
void blitScaled(GFXcanvas1& c, float scale, int cx, int cy){
  int sw=c.width(), sh=c.height();
  int dw=(int)(sw*scale+0.5f), dh=(int)(sh*scale+0.5f);
  if(dw<1||dh<1) return;
  int x0=cx-dw/2, y0=cy-dh/2;
  for(int dy=0; dy<dh; dy++){
    int py=y0+dy; if(py<0||py>=OLED_H) continue;
    int sy=(int)(dy/scale); if(sy>=sh) sy=sh-1;
    for(int dx=0; dx<dw; dx++){
      int px=x0+dx; if(px<0||px>=OLED_W) continue;
      int sx=(int)(dx/scale); if(sx>=sw) sx=sw-1;
      if(c.getPixel(sx,sy)) oled.drawPixel(px,py,SSD1306_WHITE);
    }
  }
}

// Render "PropX" over "Experience" (same font/size) once into gLock, and
// work out the settled small scale + the layout rows below it. Both words
// are centered; the canvas is sized to the wider one ("Experience").
void buildLockup(){
  const GFXfont* LF = &FreeSansBold18pt7b;
  oled.setTextWrap(false);   // else getTextBounds wraps long words at 128px
  oled.setFont(LF);
  int16_t px,py; uint16_t pw,ph; oled.getTextBounds("PropX",0,0,&px,&py,&pw,&ph);
  int16_t ex,ey; uint16_t ew,eh; oled.getTextBounds("Experience",0,0,&ex,&ey,&ew,&eh);
  oled.setFont(nullptr);

  const int gap=6;
  int cw=(pw>ew?pw:ew)+2, ch=(int)ph+gap+(int)eh+2;
  gLock=new GFXcanvas1(cw,ch);
  gLock->setTextWrap(false); gLock->setFont(LF); gLock->setTextColor(1);
  gLock->setCursor((cw-(int)pw)/2 - px, 1 - py);              gLock->print("PropX");
  gLock->setCursor((cw-(int)ew)/2 - ex, 1 + (int)ph + gap - ey); gLock->print("Experience");
  gLockW=cw; gLockH=ch;

  gLockSmallScale = 24.0f / ch;                 // settle the block ~24px tall
  int blkH=(int)(ch*gLockSmallScale);
  gLockSmallCY = blkH/2 + 2;                     // 2px margin at the very top
  gAreaTop     = blkH + 4;
  gTinkCY      = gAreaTop + (OLED_H-gAreaTop)/2;
}

// The resting/idle image: the small lockup up top, italic "Tinker" below.
void drawSplash(){
  oled.clearDisplay();
  oled.setTextWrap(false);
  if(gLock) blitScaled(*gLock, gLockSmallScale, OLED_W/2, gLockSmallCY);
  oled.setFont(&FreeSerifBoldItalic12pt7b);
  oled.setTextColor(SSD1306_WHITE);
  int16_t bx,by; uint16_t bw,bh;
  oled.getTextBounds("Tinker",0,0,&bx,&by,&bw,&bh);
  oled.setCursor((OLED_W-(int)bw)/2 - bx, gTinkCY - (by + (int)bh/2));
  oled.print("Tinker");
  oled.setFont(nullptr);
  oled.display();
}

// Boot animation (blocking — runs once from setup): the big lockup shrinks
// and rises to the top, then italic "Tinker" grows from a dot to full size.
// Ends on the static drawSplash() image.
void playSplashIntro(){
  if(!gLock){ drawSplash(); return; }

  // Tinker drawn once in its italic face at final size (blit scale 1.0).
  int16_t bx,by; uint16_t bw,bh;
  oled.setFont(&FreeSerifBoldItalic12pt7b);
  oled.getTextBounds("Tinker",0,0,&bx,&by,&bw,&bh);
  oled.setFont(nullptr);
  GFXcanvas1 cTink(bw+2, bh+2);
  cTink.setTextWrap(false); cTink.setFont(&FreeSerifBoldItalic12pt7b);
  cTink.setTextColor(1); cTink.setCursor(1-bx, 1-by); cTink.print("Tinker");

  const int   STEPS=16;
  const float bigScale=(float)(OLED_W-4)/gLockW;   // fill the width when big
  const int   bigCY=OLED_H/2;

  // hold the big lockup a beat
  oled.clearDisplay(); blitScaled(*gLock, bigScale, OLED_W/2, bigCY); oled.display();
  delay(700);

  // lockup: shrink + rise to the top
  for(int f=1; f<=STEPS; f++){
    float t=(float)f/STEPS;
    float sc=bigScale + (gLockSmallScale-bigScale)*t;
    int   cy=bigCY + (int)((gLockSmallCY-bigCY)*t);
    oled.clearDisplay(); blitScaled(*gLock, sc, OLED_W/2, cy); oled.display();
    delay(25);
  }

  // Tinker: grow from a dot (ease-in so it blooms), lockup held small on top
  for(int f=1; f<=STEPS; f++){
    float t=(float)f/STEPS;
    float sc=0.05f + 0.95f*t*t;
    oled.clearDisplay();
    blitScaled(*gLock, gLockSmallScale, OLED_W/2, gLockSmallCY);
    blitScaled(cTink, sc, OLED_W/2, gTinkCY);
    oled.display();
    delay(25);
  }
  drawSplash();                       // settle onto the canonical idle image
}

// Greedy word-wrap `text` into out[] at `cols` chars/line. Returns the line
// count; sets `truncated` if it ran out of lines (overlong words are split).
int wrapText(const String& text, int cols, String* out, int maxLines, bool& truncated){
  truncated=false; int nl=0; String cur="";
  auto push=[&](const String& s)->bool{ if(nl<maxLines){ out[nl++]=s; return true; } truncated=true; return false; };
  int i=0, n=text.length();
  while(i<n){
    while(i<n && text[i]==' ') i++;                 // skip leading spaces
    if(i>=n) break;
    int j=i; while(j<n && text[j]!=' ') j++;
    String w=text.substring(i,j); i=j;
    while((int)w.length()>cols){                    // hard-split an overlong word
      if(cur.length()){ if(!push(cur)) return nl; cur=""; }
      if(!push(w.substring(0,cols))) return nl;
      w=w.substring(cols);
    }
    if(cur.length()==0) cur=w;
    else if((int)(cur.length()+1+w.length())<=cols) cur+=" "+w;
    else { if(!push(cur)) return nl; cur=w; }
  }
  if(cur.length()) push(cur);
  return nl;
}

// Center wrapped text on the panel, largest of size 2/1 that fits, with an
// optional small header row. Whatever doesn't fit at size 1 is clipped.
void drawTextScreen(const String& text, const char* header){
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);
  oled.setFont(nullptr);
  int top=0;
  if(header && header[0]){
    oled.setTextSize(1); oled.setCursor(0,0); oled.print(header);
    oled.drawFastHLine(0,10,OLED_W,SSD1306_WHITE);
    top=14;
  }
  String lines[8]; int nl=0, size=1; bool tr=true;
  for(size=2; size>=1; size--){
    int cols=OLED_W/(6*size);
    int maxRows=(OLED_H-top)/(8*size); if(maxRows>8) maxRows=8;
    nl=wrapText(text, cols, lines, maxRows, tr);
    if(!tr) break;                                  // fits whole at this size
  }
  int charW=6*size, charH=8*size, blockH=nl*charH;
  int y=top+((OLED_H-top)-blockH)/2; if(y<top) y=top;
  oled.setTextSize(size);
  for(int k=0;k<nl;k++){
    int x=(OLED_W-(int)lines[k].length()*charW)/2; if(x<0) x=0;
    oled.setCursor(x,y); oled.print(lines[k]); y+=charH;
  }
  oled.display();
}

// Called from loop(): recompute what the panel should show and redraw only
// when it changes. Throttled to keep the ~20ms I2C blit off the hot path.
void serviceScreen(){
  if(!oledOk) return;
  static uint32_t last=0;
  uint32_t t=millis();
  if(t-last < 200) return;
  last=t;

  lockAudio();
  bool   playing = (mp3 && mp3->isRunning());
  String track   = playing ? baseName(nowPlaying) : String("");
  String msg     = screenMsg;
  unlockAudio();

  String sig = playing ? ("P|"+track) : (msg.length() ? ("M|"+msg) : String("LOGO"));
  if(sig==screenSig) return;
  screenSig=sig;

  if(playing)           drawTextScreen(track, "NOW PLAYING");
  else if(msg.length()) drawTextScreen(msg, nullptr);
  else                  drawSplash();
}

// ── Audio engine (same approach as Example 06) ──────────────────────
void audioStop(){
  if(mp3&&mp3->isRunning())mp3->stop();
  if(buff){delete buff;buff=nullptr;}
  if(file){delete file;file=nullptr;}
}
bool startFile(const String& path){
  audioStop();
  if(!SD.exists(path)) return false;
  file = new AudioFileSourceSD(path.c_str());
  buff = new AudioFileSourceBuffer(file, AUDIO_BUF_BYTES);
  if(buff && mp3->begin(buff,out)){ nowPlaying=path; return true; }
  audioStop(); nowPlaying=""; return false;
}
void buildPlaylist(const String& dir){
  plCount=0; File d=SD.open(dir);
  if(!d||!d.isDirectory()){ if(d)d.close(); return; }
  for(File e=d.openNextFile(); e&&plCount<MAX_FILES; e=d.openNextFile()){
    if(!e.isDirectory()){ String n=baseName(String(e.name())); if(!isHidden(n)&&isMp3(n)) playlist[plCount++]=joinPath(dir,n); }
    e.close();
  }
  d.close();
}
void playInline(const String& dir){ buildPlaylist(dir); if(!plCount){playMode=PM_NONE;return;} playMode=PM_INLINE; plIndex=0; startFile(playlist[0]); }
void playRandom(const String& dir){ buildPlaylist(dir); if(!plCount){playMode=PM_NONE;return;} playMode=PM_RANDOM; plIndex=random(plCount); startFile(playlist[plIndex]); }
void playSingle(const String& path){ playMode=PM_SINGLE; plCount=0; plIndex=-1; startFile(path); }
void stopAll(){ playMode=PM_NONE; audioStop(); nowPlaying=""; }
void onTrackEnd(){
  if(playMode==PM_INLINE){ plIndex++; if(plIndex<plCount){ startFile(playlist[plIndex]); return; } }
  else if(playMode==PM_RANDOM){ plIndex=random(plCount); startFile(playlist[plIndex]); return; }
  stopAll();
}

// ── Audio task ──────────────────────────────────────────────────────
// Pumps the decoder from its own FreeRTOS task (core 1, above loop()
// priority) so HTTP handling never starves playback and vice versa.
// mp3->loop() decodes at most one frame and returns immediately when the
// I2S DMA buffer is full, so the lock is only held briefly per pass.
void audioTask(void*){
  for(;;){
    bool active=false;
    lockAudio();
    if(mp3 && mp3->isRunning()){
      if(!mp3->loop()) onTrackEnd();
      active=true;
    }
    unlockAudio();
    vTaskDelay(active ? 1 : pdMS_TO_TICKS(20));
  }
}

// ── Web handlers: I/O ───────────────────────────────────────────────
void handleIoStatus(){
  String ins="";
  for(int i=0;i<4;i++){ bool a=digitalRead(BTN_PINS[i])==LOW;
    if(ins.length())ins+=","; ins+="{\"name\":\""+String(BTN_NAMES[i])+"\",\"pin\":"+BTN_PINS[i]+",\"active\":"+(a?"true":"false")+"}"; }
  for(int i=0;i<2;i++){ bool a=digitalRead(OPT_PINS[i])==LOW;
    ins+=",{\"name\":\""+String(OPT_NAMES[i])+"\",\"pin\":"+OPT_PINS[i]+",\"active\":"+(a?"true":"false")+"}"; }
  String outs="";
  for(int i=0;i<4;i++){ if(outs.length())outs+=","; outs+="{\"n\":"+String(i+1)+",\"pin\":"+OUT_PINS[i]+",\"on\":"+(outState[i]?"true":"false")+"}"; }
  server.send(200,"application/json","{\"inputs\":["+ins+"],\"outputs\":["+outs+"]}");
}
void handleIoSet(){
  int n=server.arg("out").toInt();
  if(n<1||n>4){ server.send(400,"text/plain","out 1-4"); return; }
  bool on = server.arg("on").toInt()!=0;
  outState[n-1]=on;
  digitalWrite(OUT_PINS[n-1], on?OUT_ON:OUT_OFF);
  server.send(200,"text/plain","ok");
}

// ── Web handlers: NeoPixel ──────────────────────────────────────────
void handleRgbStatus(){
  String px="";
  for(int i=0;i<NUM_PIXELS;i++){
    char hex[7]; sprintf(hex,"%06X",(unsigned)(pxColor[i]&0xFFFFFF));
    if(px.length())px+=","; px+="{\"on\":"+String(pxOn[i]?"true":"false")+",\"color\":\""+String(hex)+"\"}";
  }
  server.send(200,"application/json","{\"count\":"+String(NUM_PIXELS)+",\"bright\":"+String(brightness)+",\"pixels\":["+px+"]}");
}
void handleRgbSet(){
  int i=server.arg("i").toInt();
  if(i<0||i>=NUM_PIXELS){ server.send(400,"text/plain","bad index"); return; }
  if(server.hasArg("on"))    pxOn[i]=server.arg("on").toInt()!=0;
  if(server.hasArg("color")) pxColor[i]=(uint32_t)strtol(server.arg("color").c_str(),nullptr,16);
  renderPixels();
  server.send(200,"text/plain","ok");
}
void handleRgbBright(){
  if(server.hasArg("v")){ int v=server.arg("v").toInt(); brightness=(uint8_t)constrain(v,0,255); renderPixels(); }
  server.send(200,"text/plain",String(brightness));
}

// ── Web handlers: Screen ────────────────────────────────────────────
void handleScreenGet(){
  lockAudio(); String m=screenMsg; unlockAudio();
  server.send(200,"application/json","{\"msg\":\""+jsonEsc(m)+"\"}");
}
void handleScreenSet(){
  // The panel repaints from serviceScreen(); we just store the text here.
  lockAudio(); screenMsg = server.hasArg("text") ? server.arg("text") : String(""); unlockAudio();
  server.send(200,"text/plain","ok");
}

// ── Web handlers: Media (same API as Example 06) ────────────────────
void handleList(){
  String path=server.hasArg("path")?server.arg("path"):"/"; if(!path.length())path="/";
  lockAudio();   // SD bus is shared with the audio task
  File d=SD.open(path);
  if(!d||!d.isDirectory()){ if(d)d.close(); unlockAudio(); server.send(404,"application/json","{\"error\":\"not a dir\"}"); return; }
  String dirs="",files="";
  for(File e=d.openNextFile(); e; e=d.openNextFile()){
    String n=baseName(String(e.name()));
    if(!isHidden(n)){
      if(e.isDirectory()){ if(dirs.length())dirs+=","; dirs+="\""+jsonEsc(n)+"\""; }
      else if(isMp3(n)){ if(files.length())files+=","; files+="\""+jsonEsc(n)+"\""; }
    }
    e.close();
  }
  d.close();
  unlockAudio();
  server.send(200,"application/json","{\"path\":\""+jsonEsc(path)+"\",\"dirs\":["+dirs+"],\"files\":["+files+"]}");
}
void handlePlay(){ if(!server.hasArg("file")){server.send(400,"text/plain","missing");return;} lockAudio(); playSingle(server.arg("file")); unlockAudio(); server.send(200,"text/plain","ok"); }
void handlePlayRandom(){ lockAudio(); playRandom(server.hasArg("dir")?server.arg("dir"):"/"); unlockAudio(); server.send(200,"text/plain","ok"); }
void handlePlayInline(){ lockAudio(); playInline(server.hasArg("dir")?server.arg("dir"):"/"); unlockAudio(); server.send(200,"text/plain","ok"); }
void handleStop(){ lockAudio(); stopAll(); unlockAudio(); server.send(200,"text/plain","ok"); }
void handleNext(){ lockAudio(); onTrackEnd(); unlockAudio(); server.send(200,"text/plain","ok"); }
void handleVol(){ if(server.hasArg("v")){ lockAudio(); volume=server.arg("v").toInt(); applyVolume(); unlockAudio(); } server.send(200,"text/plain",String(volume)); }
void handleStatus(){
  lockAudio();
  const char* m = playMode==PM_SINGLE?"single":playMode==PM_INLINE?"inline":playMode==PM_RANDOM?"random":"stopped";
  bool playing=(mp3&&mp3->isRunning());
  String json =
    "{\"playing\":"+String(playing?"true":"false")+",\"mode\":\""+m+"\",\"file\":\""+jsonEsc(nowPlaying)+
    "\",\"track\":"+String(plIndex+1)+",\"count\":"+String(plCount)+",\"vol\":"+String(volume)+",\"sd\":"+String(sdOk?"true":"false")+"}";
  unlockAudio();
  server.send(200,"application/json",json);
}

// ── The single-page UI ──────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PropX Tinker - Hardware Test</title>
<style>
 body{font-family:system-ui,sans-serif;margin:0;background:#111;color:#eee}
 header{background:#1c1c1c;padding:12px 16px;border-bottom:2px solid #f5b301}
 h1{margin:0;font-size:18px}h1 span{color:#f5b301}
 .tabs{display:flex;background:#1c1c1c}
 .tabs button{flex:1;background:#1c1c1c;color:#aaa;border:0;border-bottom:3px solid transparent;padding:12px;font-size:15px;cursor:pointer}
 .tabs button.on{color:#f5b301;border-bottom-color:#f5b301}
 .wrap{padding:16px;max-width:760px;margin:0 auto}
 .panel{display:none}.panel.on{display:block}
 .row{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;border-bottom:1px solid #222}
 .pill{padding:3px 10px;border-radius:12px;font-size:13px;font-weight:600}
 .hi{background:#1f7a1f;color:#fff}.lo{background:#333;color:#999}
 button.act{background:#f5b301;color:#111;border:0;border-radius:6px;padding:8px 12px;font-weight:600;cursor:pointer}
 button.sec{background:#333;color:#eee;border:0;border-radius:6px;padding:8px 12px;cursor:pointer}
 .out{display:flex;gap:8px;align-items:center}
 .sw{width:54px;text-align:center}
 ul{list-style:none;padding:0;margin:0}
 li{padding:10px 12px;border-bottom:1px solid #222;cursor:pointer;display:flex;justify-content:space-between}
 li:hover{background:#1d1d1d}.dir{color:#7cc3ff}
 .crumb{color:#9aa;font-size:13px;margin:8px 0;word-break:break-all}
 .bar{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0;align-items:center}
 .now{background:#1c1c1c;border:1px solid #333;border-radius:8px;padding:10px 12px;margin-top:12px}
 .tag{color:#f5b301;font-weight:600}
 .px{display:flex;justify-content:space-between;align-items:center;padding:12px;border-bottom:1px solid #222}
 input[type=color]{width:48px;height:36px;border:0;background:none}
 h3{margin:16px 0 4px;color:#f5b301;font-size:14px}
</style></head><body>
<header><h1>PROP<span>X</span> Tinker &mdash; Hardware Test</h1></header>
<div class="tabs">
 <button id="t0" class="on" onclick="tab(0)">I/O</button>
 <button id="t1" onclick="tab(1)">Media</button>
 <button id="t2" onclick="tab(2)">NeoPixel</button>
 <button id="t3" onclick="tab(3)">Screen</button>
</div>
<div class="wrap">

 <!-- I/O -->
 <div class="panel on" id="p0">
   <h3>Inputs (live)</h3>
   <div id="inputs"></div>
   <h3>Outputs (MOSFET, active-LOW)</h3>
   <div id="outputs"></div>
 </div>

 <!-- MEDIA -->
 <div class="panel" id="p1">
   <div class="crumb" id="crumb">/</div>
   <div class="bar">
     <button class="sec" onclick="up()">&uarr; Up</button>
     <button class="act" onclick="api('/api/playrandom?dir='+enc(path))">&#9842; Random</button>
     <button class="act" onclick="api('/api/playinline?dir='+enc(path))">&#9654; Play all</button>
     <button class="sec" onclick="api('/api/stop')">&#9632; Stop</button>
   </div>
   <ul id="list"></ul>
   <div class="now" id="now">Nothing playing.</div>
   <div class="bar">Vol <input type="range" id="vol" min="1" max="20" value="8" onchange="api('/api/vol?v='+this.value)">
     <span id="volv">8</span>
     <button class="sec" onclick="api('/api/next')">Next &raquo;</button></div>
 </div>

 <!-- NEOPIXEL -->
 <div class="panel" id="p2">
   <h3>Brightness</h3>
   <div class="bar">
     <input type="range" id="bright" min="0" max="255" value="128" style="flex:1"
       oninput="rgbBright(this.value)">
     <span id="brightv" class="sw">128</span>
   </div>
   <h3>RGB pixels</h3>
   <div id="pixels"></div>
 </div>

 <!-- SCREEN -->
 <div class="panel" id="p3">
   <h3>OLED message</h3>
   <textarea id="msg" rows="3" maxlength="120" placeholder="Type a message for the screen&hellip;"
     style="width:100%;box-sizing:border-box;background:#1c1c1c;color:#eee;border:1px solid #333;border-radius:6px;padding:8px;font-size:15px;font-family:inherit"></textarea>
   <div class="bar">
     <button class="act" onclick="sendMsg()">Show on screen</button>
     <button class="sec" onclick="clearMsg()">Clear</button>
   </div>
   <p class="crumb">While a track is playing the screen shows the song. Your message returns when playback stops; clearing it brings back the logo.</p>
 </div>

</div>
<script>
let cur=0, path="/";
function enc(s){return encodeURIComponent(s);}
function api(u){return fetch(u);}
function tab(i){
  cur=i;
  for(let k=0;k<4;k++){
    document.getElementById("t"+k).className=(k==i?"on":"");
    document.getElementById("p"+k).className="panel"+(k==i?" on":"");
  }
  if(i==0)ioStatus(); if(i==1){loadDir();mediaStatus();} if(i==2)rgbStatus(); if(i==3)loadMsg();
}

/* ---- I/O ---- */
async function ioStatus(){
  const d=await (await fetch("/api/io/status")).json();
  let h="";
  d.inputs.forEach(x=>{ h+='<div class="row"><span>'+x.name+' <small>(GPIO'+x.pin+')</small></span>'+
    '<span class="pill '+(x.active?'hi':'lo')+'">'+(x.active?'ACTIVE / LOW':'idle / HIGH')+'</span></div>'; });
  document.getElementById("inputs").innerHTML=h;
  let o="";
  d.outputs.forEach(x=>{ o+='<div class="row"><span>OUT'+x.n+' <small>(GPIO'+x.pin+')</small></span>'+
    '<span class="out"><span class="sw '+(x.on?'':'')+'">'+(x.on?'ON':'off')+'</span>'+
    '<button class="'+(x.on?'sec':'act')+'" onclick="ioSet('+x.n+','+(x.on?0:1)+')">'+(x.on?'Turn off':'Turn on')+'</button></span></div>'; });
  document.getElementById("outputs").innerHTML=o;
}
async function ioSet(n,on){ await fetch("/api/io/set?out="+n+"&on="+on); ioStatus(); }

/* ---- Media ---- */
async function loadDir(){
  const d=await (await fetch("/api/list?path="+enc(path))).json();
  path=d.path; document.getElementById("crumb").textContent=path;
  const ul=document.getElementById("list"); ul.innerHTML="";
  (d.dirs||[]).sort().forEach(n=>{ const li=document.createElement("li");
    li.innerHTML='<span class="dir">&#128193; '+n+'</span><span>&rsaquo;</span>';
    li.onclick=()=>{path=(path==="/"?"":path)+"/"+n; loadDir();}; ul.appendChild(li); });
  (d.files||[]).sort().forEach(n=>{ const li=document.createElement("li");
    li.innerHTML='<span>&#127925; '+n+'</span><span>&#9654;</span>';
    li.onclick=()=>api("/api/play?file="+enc((path==="/"?"":path)+"/"+n)).then(mediaStatus);
    ul.appendChild(li); });
}
function up(){ if(path!=="/"){ path=path.substring(0,path.lastIndexOf("/"))||"/"; loadDir(); } }
async function mediaStatus(){
  const s=await (await fetch("/api/status")).json();
  const el=document.getElementById("now");
  if(s.playing){ let t=s.count>0?(" ["+s.track+"/"+s.count+"]"):"";
    el.innerHTML='<span class="tag">'+s.mode.toUpperCase()+'</span> '+(s.file.split("/").pop())+t; }
  else el.textContent="Nothing playing.";
  document.getElementById("vol").value=s.vol; document.getElementById("volv").textContent=s.vol;
}

/* ---- NeoPixel ---- */
let pxState=[];
async function rgbStatus(){
  const d=await (await fetch("/api/rgb/status")).json();
  pxState=d.pixels.map(p=>p.on);
  let h="";
  d.pixels.forEach((p,i)=>{ h+='<div class="px"><span>Pixel '+i+'</span><span class="out">'+
    '<input type="color" value="#'+p.color+'" oninput="rgbColor('+i+',this.value.substring(1))">'+
    '<button id="pxb'+i+'" class="'+(p.on?'sec':'act')+'" onclick="rgbToggle('+i+')">'+(p.on?'Off':'On')+'</button>'+
    '</span></div>'; });
  document.getElementById("pixels").innerHTML=h;
  document.getElementById("bright").value=d.bright;
  document.getElementById("brightv").textContent=d.bright;
}
/* oninput fires continuously while you drag, which floods the board, so we
   debounce: the toggle button flips on immediately (optimistic), but the
   actual set is sent only 500ms after you stop moving. The DOM is not
   rebuilt, so the native color wheel stays open while you scrub. */
let colorTimers={};
function rgbColor(i,hex){
  pxState[i]=true;
  const b=document.getElementById("pxb"+i);
  if(b){ b.textContent="Off"; b.className="sec"; }
  clearTimeout(colorTimers[i]);
  colorTimers[i]=setTimeout(()=>{ fetch("/api/rgb/set?i="+i+"&color="+hex+"&on=1"); },500);
}
function rgbBright(v){ document.getElementById("brightv").textContent=v; fetch("/api/rgb/bright?v="+v); }
async function rgbToggle(i){
  const on=!pxState[i];
  await fetch("/api/rgb/set?i="+i+"&on="+(on?1:0));
  pxState[i]=on;
  const b=document.getElementById("pxb"+i);
  b.textContent=on?"Off":"On"; b.className=on?"sec":"act";
}

/* ---- Screen ---- */
async function loadMsg(){
  const d=await (await fetch("/api/screen")).json();
  document.getElementById("msg").value=d.msg||"";
}
function sendMsg(){ api("/api/screen/set?text="+enc(document.getElementById("msg").value)); }
function clearMsg(){ document.getElementById("msg").value=""; api("/api/screen/set?text="); }

/* ---- polling: refresh the active tab ---- */
setInterval(()=>{ if(cur==0)ioStatus(); if(cur==1)mediaStatus(); },1500);
tab(0);
</script></body></html>
)HTML";

void handleRoot(){ server.send_P(200,"text/html",INDEX_HTML); }

// ── Setup / loop ────────────────────────────────────────────────────
void setup(){
  Serial.begin(115200);
  delay(400);
  Serial.println("\nPropX Tinker - Example 07: Full Hardware Test");

  // I/O
  for(int i=0;i<4;i++){ pinMode(OUT_PINS[i],OUTPUT); digitalWrite(OUT_PINS[i],OUT_OFF); }
  for(int i=0;i<4;i++) pinMode(BTN_PINS[i],INPUT_PULLUP);
  for(int i=0;i<2;i++) pinMode(OPT_PINS[i],INPUT_PULLUP);

  // NeoPixel
  pixels.begin();
  for(int i=0;i<NUM_PIXELS;i++){ pxOn[i]=false; pxColor[i]=0xF5B301; }  // default amber, off
  renderPixels();

  // OLED — show the logo right away; serviceScreen() takes over after boot.
  Wire.begin(OLED_SDA, OLED_SCL);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if(oledOk){ buildLockup(); playSplashIntro(); screenSig="LOGO"; Serial.println("OK OLED ready (showing logo)."); }
  else Serial.println("X OLED init failed (check SDA/SCL and address 0x3C/0x3D).");

  // SD + audio
  // 20 MHz SPI — 4 MHz is bring-up speed; MP3 streaming while the web UI
  // scans directories needs the headroom. Drop back if a card misbehaves.
  spiSD.begin(SD_SCK,SD_MISO,SD_MOSI,SD_CS);
  sdOk = SD.begin(SD_CS,spiSD,20000000);
  if(!sdOk) sdOk = SD.begin(SD_CS,spiSD,4000000);   // slow-card fallback
  Serial.println(sdOk?"OK SD mounted.":"X SD mount failed (Media tab will be empty).");
  audioMutex = xSemaphoreCreateMutex();
  // 32 DMA buffers (vs. the default 8) deepen the I2S queue to ~93ms at
  // 44.1kHz so playback rides through the moments a web handler holds
  // audioMutex and decode pauses. This library fixes each buffer at 128
  // samples and only exposes the count via the constructor.
  out = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, 32);
  out->SetPinout(I2S_BCK,I2S_LRCK,I2S_DIN);
  mp3 = new AudioGeneratorMP3();
  applyVolume();
  randomSeed(micros());

  // Wi-Fi SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID,AP_PASS);
  Serial.printf("Wi-Fi AP \"%s\" (pass \"%s\")\n",AP_SSID,AP_PASS);
  Serial.printf("Open http://%s\n",WiFi.softAPIP().toString().c_str());

  // Routes
  server.on("/",handleRoot);
  server.on("/api/io/status",handleIoStatus);
  server.on("/api/io/set",handleIoSet);
  server.on("/api/rgb/status",handleRgbStatus);
  server.on("/api/rgb/set",handleRgbSet);
  server.on("/api/rgb/bright",handleRgbBright);
  server.on("/api/screen",handleScreenGet);
  server.on("/api/screen/set",handleScreenSet);
  server.on("/api/list",handleList);
  server.on("/api/play",handlePlay);
  server.on("/api/playrandom",handlePlayRandom);
  server.on("/api/playinline",handlePlayInline);
  server.on("/api/stop",handleStop);
  server.on("/api/next",handleNext);
  server.on("/api/vol",handleVol);
  server.on("/api/status",handleStatus);
  server.begin();
  Serial.println("Web server started.");

  // Audio pump runs in its own task on core 1, above loop()'s priority 1.
  // WiFi/TCP already live on core 0, so web traffic never blocks decode.
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 3, nullptr, 1);
  Serial.println("Audio task started (core 1).");
}

void loop(){
  server.handleClient();   // audio runs in its own task — see audioTask()
  serviceScreen();         // throttled + change-gated OLED refresh
  delay(1);
}
