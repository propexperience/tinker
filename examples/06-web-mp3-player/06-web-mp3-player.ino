/*
 * PropX Tinker — Example 06: Web MP3 Player
 * --------------------------------------------------------------------
 * Combines the SD card + the PCM5102A audio DAC + Wi-Fi into a small web app.
 * The board creates its own Wi-Fi hotspot (SoftAP); connect to it and open the
 * page in a browser to:
 *   - browse the SD card's folder structure and navigate into folders
 *   - play a chosen MP3
 *   - play a folder at RANDOM (shuffle)
 *   - play a folder INLINE (every track in order)
 *
 * Connect to Wi-Fi:  "PropX-Tinker"   (password: "tinker123")
 * Then open:         http://192.168.4.1
 *
 * Pins:
 *   SD (SPI) : MISO=40 MOSI=38 SCK=39 CS=41
 *   I2S DAC  : DIN=10  BCK=11  LRCK=12
 *
 * Libraries: ESP8266Audio (Earle F. Philhower). WiFi + WebServer are built in.
 *
 * NOTE: PCM5102A is line-level - connect powered speakers / an amp.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// ── Config ──────────────────────────────────────────────────────────
const char* AP_SSID = "PropX-Tinker";
const char* AP_PASS = "tinker123";        // >= 8 chars; "" for an open network

static const int SD_MISO = 40, SD_MOSI = 38, SD_SCK = 39, SD_CS = 41;
static const int I2S_DIN = 10, I2S_BCK = 11, I2S_LRCK = 12;

// ── Globals ─────────────────────────────────────────────────────────
WebServer server(80);
SPIClass spiSD(FSPI);

AudioGeneratorMP3* mp3  = nullptr;
AudioFileSourceSD* file = nullptr;
AudioOutputI2S*    out  = nullptr;
int  volume = 8;                          // 1..20

enum PlayMode { PM_NONE, PM_SINGLE, PM_INLINE, PM_RANDOM };
PlayMode playMode = PM_NONE;

static const int MAX_FILES = 256;
String playlist[MAX_FILES];               // full paths of mp3s in the active folder
int    plCount = 0;
int    plIndex = -1;
String nowPlaying = "";

// ── Small helpers ───────────────────────────────────────────────────
String baseName(const String& path) {
  int s = path.lastIndexOf('/');
  return s >= 0 ? path.substring(s + 1) : path;
}
bool isMp3(const String& name) {
  String l = name; l.toLowerCase();
  return l.endsWith(".mp3");
}
bool isHidden(const String& name) { return name.startsWith("."); }

String joinPath(const String& dir, const String& name) {
  if (dir == "/" || dir.length() == 0) return "/" + name;
  return dir + "/" + name;
}

// JSON-escape a string (quotes + backslashes).
String jsonEsc(const String& s) {
  String o; o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else o += c;
  }
  return o;
}

void applyVolume() {
  if (volume < 1) volume = 1;
  if (volume > 20) volume = 20;
  if (out) out->SetGain((float)volume / 20.0f);
}

// ── Audio control ───────────────────────────────────────────────────
void audioStop() {
  if (mp3 && mp3->isRunning()) mp3->stop();
  if (file) { delete file; file = nullptr; }
}

bool startFile(const String& path) {
  audioStop();
  if (!SD.exists(path)) { Serial.printf("not found: %s\n", path.c_str()); return false; }
  file = new AudioFileSourceSD(path.c_str());
  if (file && mp3->begin(file, out)) {
    nowPlaying = path;
    Serial.printf("playing %s\n", path.c_str());
    return true;
  }
  audioStop();
  nowPlaying = "";
  return false;
}

// Build the playlist of mp3s in a folder (non-recursive, skips hidden).
void buildPlaylist(const String& dir) {
  plCount = 0;
  File d = SD.open(dir);
  if (!d || !d.isDirectory()) { if (d) d.close(); return; }
  for (File e = d.openNextFile(); e && plCount < MAX_FILES; e = d.openNextFile()) {
    if (!e.isDirectory()) {
      String n = baseName(String(e.name()));
      if (!isHidden(n) && isMp3(n)) playlist[plCount++] = joinPath(dir, n);
    }
    e.close();
  }
  d.close();
}

void playInline(const String& dir) {
  buildPlaylist(dir);
  if (plCount == 0) { playMode = PM_NONE; return; }
  playMode = PM_INLINE;
  plIndex = 0;
  startFile(playlist[plIndex]);
}

void playRandom(const String& dir) {
  buildPlaylist(dir);
  if (plCount == 0) { playMode = PM_NONE; return; }
  playMode = PM_RANDOM;
  plIndex = random(plCount);
  startFile(playlist[plIndex]);
}

void playSingle(const String& path) {
  playMode = PM_SINGLE;
  plCount = 0; plIndex = -1;
  startFile(path);
}

void stopAll() {
  playMode = PM_NONE;
  audioStop();
  nowPlaying = "";
}

// Called when a track finishes on its own.
void onTrackEnd() {
  if (playMode == PM_INLINE) {
    plIndex++;
    if (plIndex < plCount) { startFile(playlist[plIndex]); return; }
  } else if (playMode == PM_RANDOM) {
    plIndex = random(plCount);
    startFile(playlist[plIndex]);
    return;
  }
  stopAll();   // SINGLE, or inline reached the end
}

// ── Web handlers ────────────────────────────────────────────────────
// Directory listing as JSON: { "path": "...", "dirs": [...], "files": [...] }
void handleList() {
  String path = server.hasArg("path") ? server.arg("path") : "/";
  if (path.length() == 0) path = "/";

  File d = SD.open(path);
  if (!d || !d.isDirectory()) {
    if (d) d.close();
    server.send(404, "application/json", "{\"error\":\"not a directory\"}");
    return;
  }

  String dirs = "", files = "";
  for (File e = d.openNextFile(); e; e = d.openNextFile()) {
    String n = baseName(String(e.name()));
    if (!isHidden(n)) {
      if (e.isDirectory()) {
        if (dirs.length()) dirs += ",";
        dirs += "\"" + jsonEsc(n) + "\"";
      } else if (isMp3(n)) {
        if (files.length()) files += ",";
        files += "\"" + jsonEsc(n) + "\"";
      }
    }
    e.close();
  }
  d.close();

  String json = "{\"path\":\"" + jsonEsc(path) + "\",\"dirs\":[" + dirs +
                "],\"files\":[" + files + "]}";
  server.send(200, "application/json", json);
}

void handlePlay() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "missing file"); return; }
  playSingle(server.arg("file"));
  server.send(200, "text/plain", "ok");
}
void handlePlayRandom() {
  playRandom(server.hasArg("dir") ? server.arg("dir") : "/");
  server.send(200, "text/plain", "ok");
}
void handlePlayInline() {
  playInline(server.hasArg("dir") ? server.arg("dir") : "/");
  server.send(200, "text/plain", "ok");
}
void handleStop() { stopAll(); server.send(200, "text/plain", "ok"); }
void handleNext() { onTrackEnd(); server.send(200, "text/plain", "ok"); }

void handleVol() {
  if (server.hasArg("v")) { volume = server.arg("v").toInt(); applyVolume(); }
  server.send(200, "text/plain", String(volume));
}

void handleStatus() {
  const char* m = playMode == PM_SINGLE ? "single" :
                  playMode == PM_INLINE ? "inline" :
                  playMode == PM_RANDOM ? "random" : "stopped";
  bool playing = (mp3 && mp3->isRunning());
  String json = "{\"playing\":" + String(playing ? "true" : "false") +
                ",\"mode\":\"" + m + "\"" +
                ",\"file\":\"" + jsonEsc(nowPlaying) + "\"" +
                ",\"track\":" + String(plIndex + 1) +
                ",\"count\":" + String(plCount) +
                ",\"vol\":" + String(volume) + "}";
  server.send(200, "application/json", json);
}

// The single-page UI. Kept dependency-free (vanilla JS).
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PropX Tinker - MP3 Player</title>
<style>
 body{font-family:system-ui,sans-serif;margin:0;background:#111;color:#eee}
 header{background:#1c1c1c;padding:12px 16px;border-bottom:2px solid #f5b301}
 h1{margin:0;font-size:18px}h1 span{color:#f5b301}
 .wrap{padding:16px;max-width:760px;margin:0 auto}
 .crumb{color:#9aa;font-size:13px;margin:8px 0;word-break:break-all}
 ul{list-style:none;padding:0;margin:0}
 li{padding:10px 12px;border-bottom:1px solid #222;cursor:pointer;display:flex;justify-content:space-between}
 li:hover{background:#1d1d1d}
 .dir{color:#7cc3ff}.file{color:#eee}
 .bar{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0}
 button{background:#f5b301;color:#111;border:0;border-radius:6px;padding:8px 12px;font-weight:600;cursor:pointer}
 button.sec{background:#333;color:#eee}
 .now{background:#1c1c1c;border:1px solid #333;border-radius:8px;padding:10px 12px;margin-top:12px;font-size:14px}
 .tag{color:#f5b301;font-weight:600}
 input[type=range]{vertical-align:middle}
</style></head><body>
<header><h1>PROP<span>X</span> Tinker &mdash; MP3 Player</h1></header>
<div class="wrap">
 <div class="crumb" id="crumb">/</div>
 <div class="bar">
   <button class="sec" id="up">&uarr; Up</button>
   <button id="rand">&#9842; Play random</button>
   <button id="inline">&#9654; Play all (inline)</button>
   <button class="sec" id="stop">&#9632; Stop</button>
 </div>
 <ul id="list"></ul>
 <div class="now" id="now">Nothing playing.</div>
 <div class="bar">
   Vol <input type="range" id="vol" min="1" max="20" value="8">
   <span id="volv">8</span>
   <button class="sec" id="next">Next &raquo;</button>
 </div>
</div>
<script>
let path="/";
function enc(s){return encodeURIComponent(s);}
async function load(){
  const r=await fetch("/api/list?path="+enc(path));
  const d=await r.json();
  path=d.path; document.getElementById("crumb").textContent=path;
  const ul=document.getElementById("list"); ul.innerHTML="";
  d.dirs.sort().forEach(n=>{
    const li=document.createElement("li");
    li.innerHTML='<span class="dir">&#128193; '+n+'</span><span>&rsaquo;</span>';
    li.onclick=()=>{path=(path==="/"?"":path)+"/"+n; load();};
    ul.appendChild(li);
  });
  d.files.sort().forEach(n=>{
    const li=document.createElement("li");
    li.innerHTML='<span class="file">&#127925; '+n+'</span><span>&#9654;</span>';
    li.onclick=()=>{fetch("/api/play?file="+enc((path==="/"?"":path)+"/"+n)).then(status);};
    ul.appendChild(li);
  });
}
function up(){ if(path!=="/"){ path=path.substring(0,path.lastIndexOf("/"))||"/"; load(); } }
async function status(){
  const r=await fetch("/api/status"); const s=await r.json();
  const el=document.getElementById("now");
  if(s.playing){
    let t=s.count>0?(" ["+s.track+"/"+s.count+"]"):"";
    el.innerHTML='<span class="tag">'+s.mode.toUpperCase()+'</span> '+
                 (s.file.split("/").pop())+t;
  } else el.textContent="Nothing playing.";
  document.getElementById("vol").value=s.vol;
  document.getElementById("volv").textContent=s.vol;
}
document.getElementById("up").onclick=up;
document.getElementById("rand").onclick=()=>fetch("/api/playrandom?dir="+enc(path)).then(status);
document.getElementById("inline").onclick=()=>fetch("/api/playinline?dir="+enc(path)).then(status);
document.getElementById("stop").onclick=()=>fetch("/api/stop").then(status);
document.getElementById("next").onclick=()=>fetch("/api/next").then(status);
const vol=document.getElementById("vol");
vol.oninput=()=>{document.getElementById("volv").textContent=vol.value;};
vol.onchange=()=>fetch("/api/vol?v="+vol.value);
load(); status(); setInterval(status,2000);
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

// ── Setup / loop ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nPropX Tinker - Example 06: Web MP3 Player");

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD, 4000000)) {
    Serial.println("X SD mount failed - the player needs the SD card.");
    return;
  }
  Serial.println("OK SD mounted.");

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCK, I2S_LRCK, I2S_DIN);
  mp3 = new AudioGeneratorMP3();
  applyVolume();
  randomSeed(micros());

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("Wi-Fi AP: \"%s\"  pass: \"%s\"\n", AP_SSID, AP_PASS);
  Serial.printf("Open http://%s\n", WiFi.softAPIP().toString().c_str());

  server.on("/", handleRoot);
  server.on("/api/list", handleList);
  server.on("/api/play", handlePlay);
  server.on("/api/playrandom", handlePlayRandom);
  server.on("/api/playinline", handlePlayInline);
  server.on("/api/stop", handleStop);
  server.on("/api/next", handleNext);
  server.on("/api/vol", handleVol);
  server.on("/api/status", handleStatus);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  // Keep audio flowing; advance the playlist when a track ends.
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) onTrackEnd();
  }
  server.handleClient();
}
