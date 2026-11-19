
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <DHT.h>
#include <string.h>


const byte PIN_FAN     = 0;     
const byte PIN_DHT     = 1;  
const byte PIN_SERVO   = 10;
const byte PIN_BUZZER  = 11;
const byte PIN_EXT_LED = 12;
const byte PIN_PIR     = A2;   
const byte PIN_LDR     = A0;
const byte PIN_KNOB    = A1;
const byte PIN_WINDOW  = 13;   
const byte PIN_IN_LED  = A3;

const char PASSWORD[] = "1234";    
const byte MAX_DIGITS = 8;
const byte WRONG_LIMIT = 3;

const int ANGLE_CLOSED = 0;         
const int ANGLE_OPEN = 90;
const unsigned long DOOR_OPEN_TIME = 5000UL;
const unsigned long PIR_WARMUP = 60000UL;

const int PIR_MOTION_ON_ADC  = 550; 
const int PIR_MOTION_OFF_ADC = 200; 
const unsigned long PIR_DEBOUNCE = 80UL;
const unsigned long MSG_TIME = 1500UL;
const unsigned long CREDIT_INTRO_TIME = 1200UL;
const unsigned long CREDIT_NAMES_TIME = 2500UL;
const unsigned long DHT_INTERVAL = 2000UL;
const unsigned long WINDOW_DEBOUNCE = 50UL;
const int LDR_DARK_THRESHOLD = 300;
const int LDR_LIGHT_THRESHOLD = 450;
const bool DEBUG_SHOW_LDR = false;   

const int SETPOINT_MIN = 20;       
const int SETPOINT_MAX = 40;
const int FAN_HYSTERESIS = 2;       
const bool FAN_ACTIVE_HIGH = true;   

LiquidCrystal_I2C lcd(0x27, 16, 2); 
Servo doorServo;
DHT dht(PIN_DHT, DHT11);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


enum Mode { MODE_AWAY, MODE_HOME, MODE_UNLOCKED };
enum LightMode { LIGHT_AUTO, LIGHT_FORCE_ON, LIGHT_FORCE_OFF };
Mode mode = MODE_AWAY;
LightMode lightMode = LIGHT_AUTO;

char typed[MAX_DIGITS + 1];
byte typedLen = 0;
byte wrongCount = 0;
bool windowAlarm = false;
bool intruderAlert = false;

bool doorOpen = false;
unsigned long doorOpenedAt = 0;

bool windowOpen = false;
bool windowRawOpen = false;
unsigned long windowChangeAt = 0;

bool pirReady = false;
bool pirMotion = false;
bool pirRawMotion = false;
unsigned long pirChangeAt = 0;
int pirAdc = 1023;
unsigned long bootAt = 0;

int ldrFiltered = 0;
bool lightOn = false;

int lastTemp = -99;                
unsigned long lastDhtRead = 0;

int knobFiltered = 0;
int knobAtChange = 0;
int setpoint = SETPOINT_MIN;
bool fanOn = false;

char msg1[17] = "";
char msg2[17] = "";
bool msgActive = false;
unsigned long msgStart = 0;
char shown1[17] = "";
char shown2[17] = "";
bool shownCursor = false;
byte shownCursorCol = 255;


byte creditStage = 0;
unsigned long creditStartedAt = 0;
bool creditOnUnlock = false;

const unsigned int PATTERN_BELL[] = {150, 150, 150};
const unsigned int* pattern = nullptr;
byte patternLen = 0;
byte patternStep = 0;
unsigned long stepStart = 0;

// -------------------- HELPERS --------------------
bool securityOn()      { return mode != MODE_UNLOCKED; }
bool lightingAllowed() { return mode != MODE_AWAY; }
bool climateAllowed()  { return mode != MODE_AWAY; }
bool alarmActive()     { return windowAlarm || intruderAlert; }

void setLine(char* dst, const char* src) {
  byte i = 0;
  while (i < 16 && src[i] != '\0') { dst[i] = src[i]; i++; }
  while (i < 16) { dst[i] = ' '; i++; }
  dst[16] = '\0';
}

void showMessage(const char* line1, const char* line2) {
  strncpy(msg1, line1, 16); msg1[16] = '\0';
  strncpy(msg2, line2, 16); msg2[16] = '\0';
  msgStart = millis();
  msgActive = true;
}

void startCredits(bool afterUnlock) {
  creditStage = 1;
  creditStartedAt = millis();
  creditOnUnlock = afterUnlock;
}

void updateCredits() {
  if (creditStage == 0) return;
  unsigned long elapsed = millis() - creditStartedAt;
  if (creditStage == 1 && elapsed >= CREDIT_INTRO_TIME) {
    creditStage = 2;
    creditStartedAt = millis();
  } else if (creditStage == 2 && elapsed >= CREDIT_NAMES_TIME) {
    creditStage = 0;
  }
}

void setFan(bool on) {
  fanOn = on;
  digitalWrite(PIN_FAN, (fanOn == FAN_ACTIVE_HIGH) ? HIGH : LOW);
}

void startBeep(const unsigned int* p, byte len) {
  pattern = p;
  patternLen = len;
  patternStep = 0;
  stepStart = millis();
  digitalWrite(PIN_BUZZER, HIGH);
}

void openDoor() {
  doorServo.write(ANGLE_OPEN);
  doorOpen = true;
  doorOpenedAt = millis();
}

void closeDoor() {
  doorServo.write(ANGLE_CLOSED);
  doorOpen = false;
}

void goAway() {
  mode = MODE_AWAY;
  closeDoor();
  setFan(false);                  
  lightMode = LIGHT_AUTO;
  typedLen = 0;
  msgActive = false;
  creditStage = 0;
}

void goHome() {
  mode = MODE_HOME;
  closeDoor();
  typedLen = 0;
  msgActive = false;
  creditStage = 0;
}

void unlock() {
  mode = MODE_UNLOCKED;
  typedLen = 0;
  msgActive = false;
  startCredits(true);
  openDoor();
}

void checkPassword() {
  if (typedLen == 0) return;
  typed[typedLen] = '\0';
  bool correct = (strcmp(typed, PASSWORD) == 0);
  typedLen = 0;

  if (correct) {
    wrongCount = 0;
    if (windowAlarm) {            
      windowAlarm = false;
      intruderAlert = false;
    } else {
      intruderAlert = false;
      unlock();
    }
  } else {
    if (alarmActive()) return;   
    wrongCount++;
    if (wrongCount >= WRONG_LIMIT) intruderAlert = true;
    else showMessage("Wrong code", " ");
  }
}

void cycleLightOverride() {
  if (lightMode == LIGHT_AUTO) {
    lightMode = LIGHT_FORCE_ON;
    showMessage("LDR: OFF", "Light: ON");
  } else if (lightMode == LIGHT_FORCE_ON) {
    lightMode = LIGHT_FORCE_OFF;
    showMessage("LDR: OFF", "Light: OFF");
  } else {
    lightMode = LIGHT_AUTO;
    showMessage("LDR: ON", "Light: AUTO");
  }
}

void handleKey(char k) {
 
  creditStage = 0;
  bool alarm = alarmActive();
  if (k >= '0' && k <= '9') {
    if (mode != MODE_UNLOCKED && typedLen < MAX_DIGITS) typed[typedLen++] = k;
  } else if (k == '*') {
    typedLen = 0;
  } else if (k == '#') {
    if (mode == MODE_UNLOCKED && !alarm) openDoor();
    else checkPassword();
  } else if (k == 'A') {
    if (!alarm && (mode == MODE_HOME || mode == MODE_UNLOCKED)) goAway();
  } else if (k == 'B') {
    if (!alarm && mode == MODE_UNLOCKED) goHome();
  } else if (k == 'C') {
    if (!alarm && lightingAllowed()) cycleLightOverride();
  } else if (k == 'D') {
    if (!alarm) startBeep(PATTERN_BELL, 3);
  }
}

// -------------------- SENSOR AND OUTPUT UPDATES --------------------
void updateWindow() {
  bool raw = digitalRead(PIN_WINDOW) == HIGH; 
  if (raw != windowRawOpen) {
    windowRawOpen = raw;
    windowChangeAt = millis();
  }
  if ((millis() - windowChangeAt >= WINDOW_DEBOUNCE) &&
      (windowRawOpen != windowOpen)) {
    windowOpen = windowRawOpen;
    if (windowOpen && securityOn()) windowAlarm = true;
  }
}

void updateSecurityLight() {
  unsigned long now = millis();
  if (!pirReady && now - bootAt >= PIR_WARMUP) {
    pirReady = true;
    pirChangeAt = now;
  }

  pirAdc = analogRead(PIN_PIR);  
  if (!pirReady) {
    pirMotion = false;
    pirRawMotion = false;
    digitalWrite(PIN_EXT_LED, LOW);
    return;
  }

  bool candidate = pirRawMotion;
  if (pirAdc >= PIR_MOTION_ON_ADC) candidate = true;
  else if (pirAdc <= PIR_MOTION_OFF_ADC) candidate = false;

  if (candidate != pirRawMotion) {
    pirRawMotion = candidate;
    pirChangeAt = now;
  }
  if (pirMotion != pirRawMotion && now - pirChangeAt >= PIR_DEBOUNCE) {
    pirMotion = pirRawMotion;
  }


  digitalWrite(PIN_EXT_LED, (securityOn() && pirMotion) ? HIGH : LOW);
}

void updateIndoorLight() {
  int raw = analogRead(PIN_LDR);
  ldrFiltered = (ldrFiltered * 7 + raw) / 8;
  if (!lightingAllowed()) lightOn = false;
  else if (lightMode == LIGHT_FORCE_ON) lightOn = true;
  else if (lightMode == LIGHT_FORCE_OFF) lightOn = false;
  else {
    if (ldrFiltered < LDR_DARK_THRESHOLD) lightOn = true;
    else if (ldrFiltered > LDR_LIGHT_THRESHOLD) lightOn = false;
  }
  digitalWrite(PIN_IN_LED, lightOn ? HIGH : LOW);
}

void updateTemperature() {
  if (millis() - lastDhtRead < DHT_INTERVAL) return;
  lastDhtRead = millis();
  float t = dht.readTemperature();
  if (!isnan(t)) lastTemp = (int)(t + 0.5f);
 
}

void updateKnob() {
  int raw = analogRead(PIN_KNOB);
  knobFiltered = (knobFiltered * 7 + raw) / 8;
  int candidate = map(knobFiltered, 0, 1023, SETPOINT_MIN, SETPOINT_MAX);
  if (candidate != setpoint && abs(knobFiltered - knobAtChange) > 10) {
    setpoint = candidate;
    knobAtChange = knobFiltered;
    if (climateAllowed() && !alarmActive()) {
      char buf[17];
      snprintf(buf, sizeof(buf), "Fan set: %dC", setpoint);
      showMessage(buf, "");
    }
  }
}

void updateFan() {
  
  if (!climateAllowed() || lastTemp == -99) {
    setFan(false);
    return;
  }

  bool wanted = fanOn;
  if (!fanOn && lastTemp > setpoint) wanted = true;
  else if (fanOn && lastTemp < (setpoint - FAN_HYSTERESIS)) wanted = false;

  if (wanted != fanOn) {
    setFan(wanted);
    if (!alarmActive()) showMessage(fanOn ? "Fan: ON" : "Fan: OFF", "");
  }
}

void updateDoor() {
  if (doorOpen && millis() - doorOpenedAt >= DOOR_OPEN_TIME) closeDoor();
}

void updateMessage() {
  if (msgActive && millis() - msgStart >= MSG_TIME) msgActive = false;
}

void updateBuzzer() {
  if (alarmActive()) {
    pattern = nullptr;           
    digitalWrite(PIN_BUZZER, HIGH);
    return;
  }
  if (pattern == nullptr) {
    digitalWrite(PIN_BUZZER, LOW);
    return;
  }
  if (millis() - stepStart >= pattern[patternStep]) {
    patternStep++;
    stepStart = millis();
    if (patternStep >= patternLen) {
      digitalWrite(PIN_BUZZER, LOW);
      pattern = nullptr;
    } else {
      digitalWrite(PIN_BUZZER, (patternStep % 2 == 0) ? HIGH : LOW);
    }
  }
}

// -------------------- LCD --------------------
void buildStatus(char* line1, char* line2) {
  setLine(line1, doorOpen ? "Door:OPEN" : "Door:CLOSED");
  const char* sec = (mode == MODE_UNLOCKED) ? "OFF" : "HOME";
  char buf[17];
  if (DEBUG_SHOW_LDR) snprintf(buf, sizeof(buf), "Sec:%s L:%d", sec, ldrFiltered);
  else if (lastTemp == -99) snprintf(buf, sizeof(buf), "Sec:%s T:--C", sec);
  else snprintf(buf, sizeof(buf), "Sec:%s T:%dC", sec, lastTemp);
  setLine(line2, buf);
}

void render() {
  char line1[17], line2[17], buf[17];
  bool wantCursor = false;
  byte cursorCol = 0;

  if (alarmActive()) {
    setLine(line1, windowAlarm ? "ALARM! WINDOW" : "INTRUDER ALERT");
    if (typedLen > 0) {
      for (byte i = 0; i < typedLen; i++) buf[i] = '*';
      buf[typedLen] = '\0';
      setLine(line2, buf);
      wantCursor = true;
      cursorCol = typedLen;
    } else setLine(line2, "Enter password");
  } else if (typedLen > 0) {
    setLine(line1, "Enter password");
    for (byte i = 0; i < typedLen; i++) buf[i] = '*';
    buf[typedLen] = '\0';
    setLine(line2, buf);
    wantCursor = true;
    cursorCol = typedLen;
  } else if (creditStage != 0) {
    
    if (creditStage == 1) {
      setLine(line1, creditOnUnlock ? "ACCESS GRANTED!" : "SMART HOME CREW");
      setLine(line2, creditOnUnlock ? "Meet the legends" : "Fueled by coffee");
    } else {
      setLine(line1, "Mostafa + Eslam");
      setLine(line2, "Ashraf + Zeyad");
    }
  } else if (msgActive) {
    setLine(line1, msg1);
    if (msg2[0] != '\0') setLine(line2, msg2);
    else if (mode == MODE_AWAY) setLine(line2, " ");
    else { char unused[17]; buildStatus(unused, line2); }
  } else if (mode == MODE_AWAY) {
    setLine(line1, "Enter password");
    setLine(line2, " ");
    wantCursor = true;
    cursorCol = 0;
  } else buildStatus(line1, line2);

  bool rewritten = false;
  if (strcmp(line1, shown1) != 0) {
    lcd.setCursor(0, 0);
    lcd.print(line1);
    strcpy(shown1, line1);
    rewritten = true;
  }
  if (strcmp(line2, shown2) != 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
    strcpy(shown2, line2);
    rewritten = true;
  }
  if (rewritten || wantCursor != shownCursor || cursorCol != shownCursorCol) {
    if (wantCursor) {
      lcd.setCursor(cursorCol, 1);
      lcd.blink();
    } else lcd.noBlink();
    shownCursor = wantCursor;
    shownCursorCol = cursorCol;
  }
}

// -------------------- SETUP / LOOP --------------------
void setup() {

  digitalWrite(PIN_FAN, FAN_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PIN_FAN, OUTPUT);
  setFan(false);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_EXT_LED, OUTPUT);
  digitalWrite(PIN_EXT_LED, LOW);
  pinMode(PIN_IN_LED, OUTPUT);
  digitalWrite(PIN_IN_LED, LOW);
  pinMode(PIN_PIR, INPUT); 
  pinMode(PIN_WINDOW, INPUT_PULLUP);  t

  doorServo.attach(PIN_SERVO);
  doorServo.write(ANGLE_CLOSED);
  dht.begin();

  windowRawOpen = digitalRead(PIN_WINDOW) == HIGH;
  windowOpen = windowRawOpen;     
  analogRead(PIN_LDR);
  ldrFiltered = analogRead(PIN_LDR);
  analogRead(PIN_KNOB);
  knobFiltered = analogRead(PIN_KNOB);
  knobAtChange = knobFiltered;
  setpoint = map(knobFiltered, 0, 1023, SETPOINT_MIN, SETPOINT_MAX);

  bootAt = millis();
  lastDhtRead = millis();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  startCredits(false);
}

void loop() {
  char key = keypad.getKey();
  if (key) handleKey(key);

  updateWindow();
  updateSecurityLight();
  updateIndoorLight();
  updateTemperature();
  updateKnob();
  updateFan();
  updateDoor();
  updateMessage();
  updateCredits();
  updateBuzzer();
  render();
}
