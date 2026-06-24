/*
 * PropX Tinker — Example 04: I/O Control (buttons + opto inputs + MOSFET outputs)
 * --------------------------------------------------------------------------------
 * One sketch that exercises all of the board's digital I/O:
 *   - 4 push BUTTONS        : GPIO 13, 14, 21, 47   (active-LOW)
 *   - 2 OPTO INPUTS         : GPIO 15, 16           (isolated 9-12V field side)
 *   - 4 MOSFET OUTPUTS      : GPIO 4, 5, 6, 7        (active-LOW, switched ground)
 *
 * The MOSFET outputs are ACTIVE-LOW: a 10k pull-up holds the GPIO HIGH (output
 * OFF). Driving the GPIO LOW switches an optocoupler that turns the MOSFET on,
 * completing the load's path to ground.
 *
 *   GPIO HIGH (idle) -> opto OFF -> MOSFET OFF -> load OFF
 *   GPIO LOW  (drive)-> opto ON  -> MOSFET ON  -> load ON
 *
 * ───────────────────────────────────────────────────────────────────────────
 *  CHOOSE WHAT TRIGGERS THE OUTPUTS  — edit TRIGGER_SOURCE below:
 *
 *    TRIG_SERIAL  : type commands in the Serial Monitor (on/off/toggle <out>)
 *    TRIG_BUTTONS : each button drives one output while held
 *                   13->OUT4  14->OUT5  21->OUT6  47->OUT7
 *    TRIG_OPTO    : each opto input drives one output while active
 *                   IN15->OUT4  IN16->OUT5
 *
 *  In every mode the live state of all inputs is still printed, so you can see
 *  what's happening even when they aren't driving an output.
 * ───────────────────────────────────────────────────────────────────────────
 *
 * Libraries: none (built-in GPIO only).
 */

#define TRIG_SERIAL   0
#define TRIG_BUTTONS  1
#define TRIG_OPTO     2

#define TRIGGER_SOURCE  TRIG_SERIAL   // <-- change me

// If true, button mode TOGGLES the output on each press instead of momentary.
#define BUTTON_TOGGLE   false

// ── Pin definitions ─────────────────────────────────────────────────
const int OUT_PINS[4]    = {4, 5, 6, 7};
const int BUTTON_PINS[4] = {13, 14, 21, 47};
const int OPTO_PINS[2]   = {15, 16};

// Active-LOW outputs: HIGH = off, LOW = on.
const int OUT_OFF = HIGH;
const int OUT_ON  = LOW;

// ── Input debounce state ────────────────────────────────────────────
struct DebIn {
  int  pin;
  bool active;          // logical "pressed/active" (active-LOW pins)
  int  lastRaw;
  unsigned long tMs;
};
DebIn buttons[4];
DebIn optos[2];
const unsigned long DEBOUNCE_MS = 25;

bool outState[4] = {false, false, false, false};   // logical ON/OFF per output

// ── Output helpers ──────────────────────────────────────────────────
void setOutput(int idx, bool on) {
  if (idx < 0 || idx > 3) return;
  outState[idx] = on;
  digitalWrite(OUT_PINS[idx], on ? OUT_ON : OUT_OFF);
  Serial.printf("  OUT%d (GPIO%d) -> %s\n", idx + 1, OUT_PINS[idx], on ? "ON" : "OFF");
}

// ── Debounced read; returns true on a fresh transition, *edge by reference ──
bool pollInput(DebIn& in, bool& nowActive) {
  int raw = digitalRead(in.pin);
  if (raw != in.lastRaw) { in.lastRaw = raw; in.tMs = millis(); }
  if (millis() - in.tMs >= DEBOUNCE_MS) {
    bool a = (raw == LOW);     // active-LOW
    if (a != in.active) { in.active = a; nowActive = a; return true; }
  }
  nowActive = in.active;
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nPropX Tinker - Example 04: I/O Control");

  for (int i = 0; i < 4; i++) {
    pinMode(OUT_PINS[i], OUTPUT);
    digitalWrite(OUT_PINS[i], OUT_OFF);     // start safely OFF
  }
  for (int i = 0; i < 4; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    buttons[i] = {BUTTON_PINS[i], false, HIGH, 0};
  }
  for (int i = 0; i < 2; i++) {
    pinMode(OPTO_PINS[i], INPUT_PULLUP);
    optos[i] = {OPTO_PINS[i], false, HIGH, 0};
  }

#if   TRIGGER_SOURCE == TRIG_SERIAL
  Serial.println("Mode: SERIAL. Commands:");
  Serial.println("  on <1-4> | off <1-4> | toggle <1-4> | all off | status");
#elif TRIGGER_SOURCE == TRIG_BUTTONS
  Serial.println("Mode: BUTTONS. 13->OUT1 14->OUT2 21->OUT3 47->OUT4");
  Serial.println(BUTTON_TOGGLE ? "  (toggle on each press)" : "  (on while held)");
#elif TRIGGER_SOURCE == TRIG_OPTO
  Serial.println("Mode: OPTO. IN15->OUT1  IN16->OUT2 (on while input active)");
#endif
  Serial.print("\n> ");
}

void printStatus() {
  Serial.print("  Buttons: ");
  for (int i = 0; i < 4; i++) Serial.printf("%d=%s ", BUTTON_PINS[i], buttons[i].active ? "DN" : "up");
  Serial.print("\n  Optos:   ");
  for (int i = 0; i < 2; i++) Serial.printf("%d=%s ", OPTO_PINS[i], optos[i].active ? "ACT" : "---");
  Serial.print("\n  Outputs: ");
  for (int i = 0; i < 4; i++) Serial.printf("OUT%d=%s ", i + 1, outState[i] ? "ON" : "off");
  Serial.println();
}

void loop() {
  // Always poll inputs so their state is visible and edges are reported.
  for (int i = 0; i < 4; i++) {
    bool a;
    if (pollInput(buttons[i], a))
      Serial.printf("[BTN] GPIO%d %s\n", BUTTON_PINS[i], a ? "pressed" : "released");
  }
  for (int i = 0; i < 2; i++) {
    bool a;
    if (pollInput(optos[i], a))
      Serial.printf("[OPTO] GPIO%d %s\n", OPTO_PINS[i], a ? "ACTIVE" : "inactive");
  }

#if TRIGGER_SOURCE == TRIG_BUTTONS
  for (int i = 0; i < 4; i++) {
  #if BUTTON_TOGGLE
    static bool prev[4] = {false, false, false, false};
    if (buttons[i].active && !prev[i]) setOutput(i, !outState[i]);  // rising edge
    prev[i] = buttons[i].active;
  #else
    if (outState[i] != buttons[i].active) setOutput(i, buttons[i].active);  // hold
  #endif
  }
#elif TRIGGER_SOURCE == TRIG_OPTO
  for (int i = 0; i < 2; i++)
    if (outState[i] != optos[i].active) setOutput(i, optos[i].active);
#endif

  // Serial commands (available in all modes; the primary control in SERIAL mode).
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    line.toLowerCase();
    if (line.length()) {
      Serial.println(line);
      int sp = line.indexOf(' ');
      String cmd = sp < 0 ? line : line.substring(0, sp);
      String arg = sp < 0 ? ""   : line.substring(sp + 1);
      arg.trim();

      if (cmd == "status") {
        printStatus();
      } else if (cmd == "all" && arg == "off") {
        for (int i = 0; i < 4; i++) setOutput(i, false);
      } else if (cmd == "on" || cmd == "off" || cmd == "toggle") {
        int n = arg.toInt();
        if (n < 1 || n > 4) Serial.println("  output must be 1-4");
        else if (cmd == "on")     setOutput(n - 1, true);
        else if (cmd == "off")    setOutput(n - 1, false);
        else                      setOutput(n - 1, !outState[n - 1]);
      } else {
        Serial.println("  Commands: on/off/toggle <1-4> | all off | status");
      }
      Serial.print("> ");
    }
  }
}
