# Example 04 — I/O Control (Buttons + Opto Inputs + MOSFET Outputs)

This is the **inputs and outputs** example. It covers everything you need to
read the board's buttons and isolated inputs and to switch its power outputs —
and it lets you choose **what triggers the outputs**.

> ⚠️ The MOSFET outputs switch **9–12 V** loads. Read the
> [safety notes](#safety) before connecting power.

---

## Pins

| Role | GPIO | Notes |
|---|---|---|
| **Buttons** | 13, 14, 21, 47 | active-LOW (pressed = LOW) |
| **Opto inputs** | 15, 16 | isolated, 9–12 V field side |
| **MOSFET outputs** | 4, 5, 6, 7 | **active-LOW**, switched ground |

---

## How the OUTPUTS work (read this first)

The four MOSFET outputs are **active-LOW** and **opto-isolated**:

```
  GPIO HIGH (idle, 10k pull-up) ─► optocoupler OFF ─► MOSFET OFF ─► load OFF
  GPIO LOW  (driven by code)    ─► optocoupler ON  ─► MOSFET ON  ─► load ON
```

- Each output GPIO has a **10 kΩ pull-up**, so it sits HIGH and the output is
  **OFF** by default — even while the ESP32 is resetting or unprogrammed. Safe.
- To turn an output **ON**, the firmware drives the GPIO **LOW**.
- The output itself is a **switched ground**: wire your load between the
  connector's **12 V** pin and the **OUT** pin. When ON, the MOSFET connects
  OUT to ground and current flows.

In code this is just:

```cpp
const int OUT_OFF = HIGH;   // active-LOW: HIGH = off
const int OUT_ON  = LOW;    //             LOW  = on
digitalWrite(OUT_PINS[i], on ? OUT_ON : OUT_OFF);
```

```
  +12V ──●──────────────┐
         │            [ LOAD ]
         │              │
  OUT ───┴── MOSFET ────┘   (switched ground; ON pulls OUT to GND)
```

## How the INPUTS work

- **Buttons** are **active-LOW**: an internal pull-up holds the pin HIGH, and
  pressing the button pulls it LOW. The sketch reads `LOW` as "pressed".
- **Opto inputs** are **isolated**: a signal on the 9–12 V field side turns the
  optocoupler on, which pulls the GPIO. The sketch treats the pin as active-LOW
  too (`LOW` = input active). If your wiring reads inverted, see
  [Tuning](#tuning-the-input-polarity).
- Both are **debounced** (25 ms) so you get one clean event per change.

---

## Choosing the trigger source

Edit one line near the top of the sketch:

```cpp
#define TRIGGER_SOURCE  TRIG_SERIAL   // TRIG_SERIAL | TRIG_BUTTONS | TRIG_OPTO
```

| Mode | What drives the outputs |
|---|---|
| `TRIG_SERIAL` | You type commands in the Serial Monitor |
| `TRIG_BUTTONS` | `13→OUT1  14→OUT2  21→OUT3  47→OUT4` (on while held) |
| `TRIG_OPTO` | `IN15→OUT1  IN16→OUT2` (on while the input is active) |

In **button** mode you can switch from momentary to latching:

```cpp
#define BUTTON_TOGGLE  true   // press once = on, press again = off
```

No matter which mode you pick, the live state of every input is still printed,
and the serial commands below always work.

## Serial commands

| Command | Action |
|---|---|
| `on <1-4>` | turn an output ON |
| `off <1-4>` | turn an output OFF |
| `toggle <1-4>` | flip an output |
| `all off` | turn everything off |
| `status` | print all input/output states |

---

## Try it

1. Flash the sketch, open the Serial Monitor at **115200**.
2. **Outputs only (safe, no 12 V):** in `TRIG_SERIAL` mode, type `on 1`. The
   corresponding MOSFET gate is driven; you can verify with a meter on the OUT
   pin or by listening for a relay/click if one is wired.
3. **Buttons:** set `TRIGGER_SOURCE TRIG_BUTTONS`, reflash, and press a button —
   its output follows.
4. **Opto inputs:** set `TRIG_OPTO`, reflash, and apply a signal to input 15/16.

> 💡 You can test outputs and inputs **without 12 V connected** — the GPIO logic
> still toggles. Connect 12 V and a load only when you're ready.

## Tuning the input polarity

The opto inputs are read as **active-LOW** (`pinMode(..., INPUT_PULLUP)` and
`LOW` = active). Depending on how your field-side signal is wired, "active" may
appear inverted. If so, either swap your field wiring or change the `active`
test in `pollInput()` from `(raw == LOW)` to `(raw == HIGH)`.

---

## Safety

- Outputs switch **9–12 V from an external supply** — never the 3.3 V logic.
- Respect the MOSFET's per-channel current rating (see
  [hardware reference](../../docs/HARDWARE.md)).
- Add a **flyback diode** across inductive loads (motors, solenoids, relays).
- Double-check polarity before powering a load.

## Libraries

None — built-in GPIO only.
