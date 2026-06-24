/*
 * PropX Tinker — Example 07: Full Hardware Test (Web)
 * --------------------------------------------------------------------
 * One web app that exercises the whole board over its own Wi-Fi hotspot.
 * Three tabs:
 *   1) I/O      - live HIGH/LOW status of every input; manually toggle outputs
 *   2) Media    - the SD-card MP3 player (browse / play / random / inline)
 *   3) NeoPixel - a color picker + on/off for each RGB pixel
 *
 * Connect to Wi-Fi:  "PropX-Tinker"   (password: "tinker123")
 * Then open:         http://192.168.4.1
 *
 * Pins:
 *   SD (SPI) : MISO=40 MOSI=38 SCK=39 CS=41
 *   I2S DAC  : DIN=10  BCK=11  LRCK=12
 *   NeoPixel : data=48
 *   Buttons  : 13, 14, 21, 47        (active-LOW)
 *   Opto in  : 15, 16                (isolated)
 *   MOSFET   : 4, 5, 6, 7            (active-LOW outputs)
 *
 * Libraries: ESP8266Audio, Adafruit NeoPixel. WiFi + WebServer are built in.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// ── Config ──────────────────────────────────────────────────────────
const char* AP_SSID = "PropX-Tinker";
const char* AP_PASS = "tinker123";

static const int SD_MISO = 40, SD_MOSI = 38, SD_SCK = 39, SD_CS = 41;
static const int I2S_DIN = 10, I2S_BCK = 11, I2S_LRCK = 12;
static const int RGB_PIN = 48;
#define NUM_PIXELS 2        // 2 LEDs in the chain on GPIO48

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

bool     outState[4] = {false, false, false, false};
bool     pxOn[NUM_PIXELS];
uint32_t pxColor[NUM_PIXELS];

AudioGeneratorMP3* mp3  = nullptr;
AudioFileSourceSD* file = nullptr;
AudioOutputI2S*    out  = nullptr;
int  volume = 8;
bool sdOk = false;

enum PlayMode { PM_NONE, PM_SINGLE, PM_INLINE, PM_RANDOM };
PlayMode playMode = PM_NONE;
static const int MAX_FILES = 256;
String playlist[MAX_FILES];
int    plCount = 0, plIndex = -1;
String nowPlaying = "";

// ── Helpers ─────────────────────────────────────────────────────────
String baseName(const String& p){ int s=p.lastIndexOf('/'); return s>=0?p.substring(s+1):p; }
bool   isMp3(const String& n){ String l=n; l.toLowerCase(); return l.endsWith(".mp3"); }
bool   isHidden(const String& n){ return n.startsWith("."); }
String joinPath(const String& d,const String& n){ return (d=="/"||!d.length())?("/"+n):(d+"/"+n); }
String jsonEsc(const String& s){ String o; for(size_t i=0;i<s.length();i++){char c=s[i]; if(c=='"'||c=='\\'){o+='\\';o+=c;} else if(c=='\n')o+="\\n"; else o+=c;} return o; }
void   applyVolume(){ if(volume<1)volume=1; if(volume>20)volume=20; if(out)out->SetGain((float)volume/20.0f); }

// ── NeoPixel ────────────────────────────────────────────────────────
void renderPixels(){
  for(int i=0;i<NUM_PIXELS;i++) pixels.setPixelColor(i, pxOn[i]?pxColor[i]:0);
  pixels.show();
}

// ── Audio engine (same approach as Example 06) ──────────────────────
void audioStop(){ if(mp3&&mp3->isRunning())mp3->stop(); if(file){delete file;file=nullptr;} }
bool startFile(const String& path){
  audioStop();
  if(!SD.exists(path)) return false;
  file = new AudioFileSourceSD(path.c_str());
  if(file && mp3->begin(file,out)){ nowPlaying=path; return true; }
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
  server.send(200,"application/json","{\"count\":"+String(NUM_PIXELS)+",\"pixels\":["+px+"]}");
}
void handleRgbSet(){
  int i=server.arg("i").toInt();
  if(i<0||i>=NUM_PIXELS){ server.send(400,"text/plain","bad index"); return; }
  if(server.hasArg("on"))    pxOn[i]=server.arg("on").toInt()!=0;
  if(server.hasArg("color")) pxColor[i]=(uint32_t)strtol(server.arg("color").c_str(),nullptr,16);
  renderPixels();
  server.send(200,"text/plain","ok");
}

// ── Web handlers: Media (same API as Example 06) ────────────────────
void handleList(){
  String path=server.hasArg("path")?server.arg("path"):"/"; if(!path.length())path="/";
  File d=SD.open(path);
  if(!d||!d.isDirectory()){ if(d)d.close(); server.send(404,"application/json","{\"error\":\"not a dir\"}"); return; }
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
  server.send(200,"application/json","{\"path\":\""+jsonEsc(path)+"\",\"dirs\":["+dirs+"],\"files\":["+files+"]}");
}
void handlePlay(){ if(!server.hasArg("file")){server.send(400,"text/plain","missing");return;} playSingle(server.arg("file")); server.send(200,"text/plain","ok"); }
void handlePlayRandom(){ playRandom(server.hasArg("dir")?server.arg("dir"):"/"); server.send(200,"text/plain","ok"); }
void handlePlayInline(){ playInline(server.hasArg("dir")?server.arg("dir"):"/"); server.send(200,"text/plain","ok"); }
void handleStop(){ stopAll(); server.send(200,"text/plain","ok"); }
void handleNext(){ onTrackEnd(); server.send(200,"text/plain","ok"); }
void handleVol(){ if(server.hasArg("v")){ volume=server.arg("v").toInt(); applyVolume(); } server.send(200,"text/plain",String(volume)); }
void handleStatus(){
  const char* m = playMode==PM_SINGLE?"single":playMode==PM_INLINE?"inline":playMode==PM_RANDOM?"random":"stopped";
  bool playing=(mp3&&mp3->isRunning());
  server.send(200,"application/json",
    "{\"playing\":"+String(playing?"true":"false")+",\"mode\":\""+m+"\",\"file\":\""+jsonEsc(nowPlaying)+
    "\",\"track\":"+String(plIndex+1)+",\"count\":"+String(plCount)+",\"vol\":"+String(volume)+",\"sd\":"+String(sdOk?"true":"false")+"}");
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
   <h3>RGB pixels</h3>
   <div id="pixels"></div>
 </div>

</div>
<script>
let cur=0, path="/";
function enc(s){return encodeURIComponent(s);}
function api(u){return fetch(u);}
function tab(i){
  cur=i;
  for(let k=0;k<3;k++){
    document.getElementById("t"+k).className=(k==i?"on":"");
    document.getElementById("p"+k).className="panel"+(k==i?" on":"");
  }
  if(i==0)ioStatus(); if(i==1){loadDir();mediaStatus();} if(i==2)rgbStatus();
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
}
/* Color changes fire live (oninput) and do NOT rebuild the DOM, so the
   native color wheel stays open until you close it or switch tabs. */
function rgbColor(i,hex){ fetch("/api/rgb/set?i="+i+"&color="+hex); }
async function rgbToggle(i){
  const on=!pxState[i];
  await fetch("/api/rgb/set?i="+i+"&on="+(on?1:0));
  pxState[i]=on;
  const b=document.getElementById("pxb"+i);
  b.textContent=on?"Off":"On"; b.className=on?"sec":"act";
}

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

  // SD + audio
  spiSD.begin(SD_SCK,SD_MISO,SD_MOSI,SD_CS);
  sdOk = SD.begin(SD_CS,spiSD,4000000);
  Serial.println(sdOk?"OK SD mounted.":"X SD mount failed (Media tab will be empty).");
  out = new AudioOutputI2S();
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
}

void loop(){
  if(mp3 && mp3->isRunning()){ if(!mp3->loop()) onTrackEnd(); }
  server.handleClient();
}
