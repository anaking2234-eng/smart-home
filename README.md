# Smart Home Project

**MCT 331s — Mechatronic System Integration | Fall 2026**

An Arduino Uno smart home prototype with a password-operated door, window alarm, motion-activated exterior light, automatic indoor lighting, adjustable temperature-controlled fan, and 16×2 LCD. The system uses a custom two-layer PCB in a laser-cut wooden house with a hinged entrance door, a double-casement window, and a wooden air-conditioning-style fan casing.

## Features

- **Door access:** A correct keypad password opens the servo-operated door and schedules closure after 5 seconds. In Unlocked mode, `#` reopens the door without a password. Three wrong passwords trigger an intruder alarm.
- **Window security:** Opening the monitored window in Away or Home mode activates the buzzer. A correct password acknowledges the window alarm without unlocking the house.
- **Exterior lighting:** An HC-SR501 PIR is read through analog input A2. After a 60-second startup warm-up, sustained ADC readings of 550 or above indicate motion; readings of 200 or below indicate idle. An 80 ms state debounce and 5-second software hold are used in addition to the PIR module's own output delay. The light is disabled in Unlocked mode.
- **Indoor lighting:** The LDR controls the indoor LED using separate dark and light thresholds. Key `C` cycles through manual ON, manual OFF, and automatic operation (Home and Unlocked only).
- **Climate control:** A DHT11 supplies temperature readings. A 10 kΩ potentiometer sets the threshold from 20°C to 40°C. The fan is driven by a MOSFET on D0, with 2°C hysteresis, and is disabled in Away mode.
- **LCD:** Shows door/security/temperature information, alarm and event messages, a startup welcome, and team credits after successful unlocking.

## Team

Mostafa · Eslam · Ashraf · Zeyad

## Project files

```text
Smart_Home_Project/
├── Smart_Home_Final/
│   └── Smart_Home_Final.ino
└── README.md
```

The Arduino IDE expects the `.ino` sketch to be inside a folder of the same name.

## Hardware and supplies

Arduino Uno; 4×4 matrix keypad; 16×2 I2C LCD; servo motor; HC-SR501 PIR; MC-38 magnetic reed switch; 3-pin DHT11 module; LDR and resistor divider; 10 kΩ potentiometer; indoor and exterior LEDs; active buzzer; 5V fan; N-channel MOSFET fan driver, gate resistor, pull-down resistor and flyback diode; custom two-sided PCB; 5V 2A regulated DC adapter; decoupling capacitors; headers and jumpers.

The physical fan MOSFET used during development was reported as **IRFZ44N**. Unlike the originally intended IRLZ44N, it is not guaranteed to switch fully with a 5V gate drive. The team should verify fan-terminal voltage under load before final submission. An EAGLE library label may not match the installed component.

## Final Arduino connections

| Arduino pin | Device / signal |
|---|---|
| D0 | Fan MOSFET gate through removable **JP1** and 100 Ω gate resistor |
| D1 | DHT11 DATA (3-pin module, with wiring matched to physical pin labels) |
| D2–D5 | Keypad columns C4–C1 |
| D6–D9 | Keypad rows R4–R1 |
| D10 | Servo signal |
| D11 | Buzzer through 100 Ω resistor |
| D12 | Exterior LED through 220 Ω resistor |
| D13 | Reed switch to GND, using `INPUT_PULLUP` |
| A0 | LDR voltage divider |
| A1 | Potentiometer wiper |
| A2 | PIR OUT **(analog input)** |
| A3 | Indoor LED through 220 Ω resistor |
| A4 | LCD SDA |
| A5 | LCD SCL |

**Important PCB adaptation:** The originally manufactured PCB was labeled for PIR on D13 and reed switch on A2. On the assembled project, their **signal leads are cross-connected externally**: PIR OUT to the A2 net and reed-switch signal to the D13 net. Do not swap their power or ground leads. Verify the actual signal against common ground before reconnecting. The analog PIR input must not be driven below Arduino GND.

**Power:** A regulated 5V adapter supplies the PCB power rail and Arduino 5V pin; Vin and the Arduino barrel jack are not used. This bypasses Arduino's onboard input regulator and protections. Do not simultaneously connect the external adapter and USB in this arrangement. Ensure external loads cannot draw their operating current through the computer's USB port.

## Software

Use Arduino IDE with Arduino Uno / Arduino AVR Boards and these libraries:

- `Wire` (built into Arduino AVR support)
- `Servo`
- `Keypad`
- `LiquidCrystal_I2C` (a version supporting `lcd.init()` and `lcd.backlight()`)
- Adafruit `DHT sensor library`
- Adafruit `Adafruit Unified Sensor` dependency

The main loop runs non-blocking subsystem updates using `millis()` rather than `delay()`. The sketch does not use `Serial` because D0/D1 are occupied by external devices.

## Upload and startup

1. Turn off and disconnect the external 5V adapter.
2. Open **JP1** to disconnect the D0 fan gate signal; disconnect D1 from DHT11 DATA before uploading via USB.
3. Prevent the external PCB loads from drawing current through the Arduino USB supply during upload.
4. Open `Smart_Home_Final/Smart_Home_Final.ino` in Arduino IDE, select Arduino Uno and correct port, then verify and upload.
5. Disconnect USB. With power OFF, reconnect JP1 and D1; power the system using the regulated 5V adapter.
6. Wait 60 seconds for the PIR warm-up before evaluating motion-activated lighting.

## Keypad controls

| Key | Action |
|---|---|
| `0`–`9` | Password entry (up to eight digits) |
| `*` | Clear entered digits |
| `#` | Confirm password in Away/Home; open the door in Unlocked |
| `A` | Switch to Away from Home or Unlocked |
| `B` | Switch to Home from Unlocked |
| `C` | Cycle indoor light override (Home or Unlocked) |
| `D` | Two-beep doorbell unless an alarm is active |

The demonstration password in the sketch is `1234`; change it if needed.

## Operating modes

| Function | Away | Home | Unlocked |
|---|---|---|---|
| Door access | Password | Password | `#` directly |
| Window security | Enabled | Enabled | Disabled |
| Motion exterior light | Enabled | Enabled | Disabled |
| Indoor light | Disabled | Enabled | Enabled |
| Temperature fan | Disabled | Enabled | Enabled |
| Doorbell | Enabled | Enabled | Enabled |

The Arduino starts in Away mode on boot. A correct password normally opens the door and changes to Unlocked mode. A correct password while a window alarm is sounding acknowledges that alarm without changing modes.

## Important constants

| Setting | Current value |
|---|---|
| `DOOR_OPEN_TIME` | 5000 ms |
| `PIR_WARMUP` | 60000 ms |
| `PIR_MOTION_ON_ADC` | 550 |
| `PIR_MOTION_OFF_ADC` | 200 |
| `PIR_DEBOUNCE` | 80 ms |
| `EXT_LED_HOLD` | 5000 ms after last confirmed motion |
| `WINDOW_DEBOUNCE` | 50 ms |
| `DHT_INTERVAL` | 2000 ms |
| `SETPOINT_MIN` / `SETPOINT_MAX` | 20°C / 40°C |
| `FAN_HYSTERESIS` | 2°C |
| `LDR_DARK_THRESHOLD` / `LDR_LIGHT_THRESHOLD` | 300 / 450 (calibrate for environment) |
| `ANGLE_CLOSED` / `ANGLE_OPEN` | 0° / 90° (calibrate for door) |
| `CREDIT_INTRO_TIME` / `CREDIT_NAMES_TIME` | 800 / 1500 ms |
| `MSG_TIME` | 1500 ms |
| `WRONG_LIMIT` | 3 |

Fan starts only when the last valid DHT11 temperature exceeds the selected setpoint; when running, it switches off below setpoint minus 2°C. A failed sensor read retains the last valid temperature.

## LCD welcome and team credits

On startup, the LCD briefly shows `Welcome` / `Smart Home`, followed by `Mostafa + Eslam` / `Ashraf + Zeyad`. After a normal successful password unlock, it shows `ACCESS GRANTED!` / `Meet the team`, then the four names. Door timing remains independent of the display sequence; alarms override credits.

## Main functions

- `handleKey()`, `checkPassword()` — keypad, modes, password and alarms
- `updateWindow()` — reed switch on D13 and debounce
- `updateSecurityLight()` — PIR analog thresholds, debounce, exterior LED hold
- `updateIndoorLight()` — LDR AUTO and C-key manual override
- `updateTemperature()`, `updateKnob()`, `updateFan()` — fan control and hysteresis
- `updateDoor()` — automatic servo-door closing
- `updateBuzzer()` — alarm priority and doorbell
- `startCredits()`, `updateCredits()`, `render()` — LCD welcome, names and status messages

## Final hardware checks

- [ ] Arduino IDE **Verify** succeeds using the chosen library versions.
- [ ] No external 5V adapter is present during USB uploading; D0/D1 can be disconnected.
- [ ] PIR A2 reads near 0 at rest and around 643 during motion, as previously observed; no negative voltage relative to Arduino GND.
- [ ] Exterior LED activates after PIR warm-up and remains on for five additional seconds after the PIR output returns to idle.
- [ ] Window alarm works through D13; a correct password acknowledges it without unlocking.
- [ ] Door opens correctly and closes approximately five seconds after the open command; credits do not delay door action.
- [ ] LDR automatic behavior and all three C-key override states work.
- [ ] DHT11 and potentiometer operate fan, and the **actual voltage across the fan under load** is sufficient.
- [ ] LCD welcome, team names and alarm priority work.
- [ ] All five required subsystems work together without unexpected resets.

The project report contains the bill of materials, mechanical CAD drawings, schematic and PCB layout, interconnection diagram, control flowcharts and code documentation. Circuit simulation is not claimed; physical verification results should be included only when observed.
