/*
 * Automated PCB Test Station - firmware
 * Board:     ESP32 DevKit (Arduino framework)
 * Libraries: Adafruit INA219 (install from Library Manager)
 *
 * Test sequence (state machine):
 *   IDLE -> PRECHECK -> POWER_UP -> RAILS -> CURRENT -> FUNCTIONAL -> DONE
 *
 * Host protocol (115200 baud, newline terminated):
 *   PC  -> ESP32 :  START,<serial_number>
 *   ESP32 -> PC  :  RESULT,<serial>,<PASS|FAIL>,<v5>,<v3v3>,<current_mA>,<fail_code>
 *
 * NOTE: pins, divider ratios and limits below are starting values.
 *       Adjust them to match your own wiring and calibrate against a multimeter.
 */

#include <Wire.h>
#include <Adafruit_INA219.h>

// ---------------- Pin map (change to match your wiring) ----------------
const uint8_t MUX_S0 = 25, MUX_S1 = 26, MUX_S2 = 27, MUX_S3 = 14;  // 16-ch analog mux select lines
const uint8_t MUX_SIG = 34;     // mux output -> ESP32 ADC1 pin (input only)
const uint8_t RELAY_PIN = 23;   // HIGH = power the DUT
const uint8_t LED_GREEN = 18, LED_RED = 19, BUZZER = 5;
const uint8_t DUT_RX = 16, DUT_TX = 17;  // UART link to the DUT (functional test)

// Mux channel numbers wired to the DUT test points
const uint8_t CH_RAIL_5V = 0;
const uint8_t CH_RAIL_3V3 = 1;

// Voltage divider ratio in front of the mux: Vadc = Vrail * DIV
const float DIV_5V = 0.5f;
const float DIV_3V3 = 0.5f;

// ---------------- Pass/fail limits ----------------
const float RAIL_5V_NOM = 5.0f;
const float RAIL_3V3_NOM = 3.3f;
const float RAIL_TOL = 0.05f;        // +/- 5 %
const float LEAKAGE_MAX_V = 0.30f;   // rails must read ~0 V before power is applied
const float I_MAX_MA = 300.0f;       // above this -> short / overcurrent
const float I_MIN_MA = 5.0f;         // below this -> board not drawing current (open / not populated)
const unsigned long SETTLE_MS = 300; // wait after power-up before measuring
const unsigned long DUT_TIMEOUT_MS = 1000;

// ---------------- Fail codes ----------------
const char* E_NONE = "E00";
const char* E_LEAKAGE = "E01";      // voltage present on a rail before power-up
const char* E_OVERCURRENT = "E02";  // short circuit / overcurrent
const char* E_RAIL_5V = "E03";
const char* E_RAIL_3V3 = "E04";
const char* E_NO_CURRENT = "E05";
const char* E_FUNCTIONAL = "E06";

// ---------------- State machine ----------------
enum State { IDLE, PRECHECK, POWER_UP, RAILS, CURRENT, FUNCTIONAL, DONE };
State state = IDLE;

Adafruit_INA219 ina;
String serialNo = "";
const char* failCode = E_NONE;
float v5 = 0, v33 = 0, iMa = 0;

// ---------------- Helpers ----------------
void selectChannel(uint8_t ch) {
  digitalWrite(MUX_S0, ch & 1);
  digitalWrite(MUX_S1, (ch >> 1) & 1);
  digitalWrite(MUX_S2, (ch >> 2) & 1);
  digitalWrite(MUX_S3, (ch >> 3) & 1);
  delay(2);  // let the mux output settle
}

// Average 16 samples, return rail voltage in volts (after undoing the divider)
float readRail(uint8_t ch, float div) {
  selectChannel(ch);
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(MUX_SIG);
    delay(1);
  }
  float adcV = (sum / 16.0f) / 1000.0f;
  return adcV / div;
}

bool inTolerance(float v, float nom) {
  return fabs(v - nom) <= nom * RAIL_TOL;
}

void powerOff() { digitalWrite(RELAY_PIN, LOW); }
void powerOn() { digitalWrite(RELAY_PIN, HIGH); }

void indicate(bool pass) {
  digitalWrite(LED_GREEN, pass ? HIGH : LOW);
  digitalWrite(LED_RED, pass ? LOW : HIGH);
  // Active buzzer: short beep = pass, long beep = fail
  digitalWrite(BUZZER, HIGH);
  delay(pass ? 150 : 600);
  digitalWrite(BUZZER, LOW);
}

void fail(const char* code) {
  failCode = code;
  powerOff();
  state = DONE;
}

// Functional test hook: adapt this to whatever your DUT can do.
// Default: send "PING" over UART and expect "PONG" back.
bool functionalTest() {
  while (Serial2.available()) Serial2.read();
  Serial2.println("PING");
  unsigned long t0 = millis();
  String reply = "";
  while (millis() - t0 < DUT_TIMEOUT_MS) {
    if (Serial2.available()) {
      reply += (char)Serial2.read();
      if (reply.indexOf("PONG") >= 0) return true;
    }
  }
  return false;
}

// Wait for "START,<serial>" from the PC
void handleStart() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.startsWith("START,")) {
    serialNo = line.substring(6);
    failCode = E_NONE;
    v5 = v33 = iMa = 0;
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
    state = PRECHECK;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, DUT_RX, DUT_TX);

  pinMode(MUX_S0, OUTPUT); pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT); pinMode(MUX_S3, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT); pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  powerOff();

  analogSetPinAttenuation(MUX_SIG, ADC_11db);  // ~0-3.1 V input range

  if (!ina.begin()) {
    Serial.println("ERROR,INA219 not found");
  }
  Serial.println("READY");
}

void loop() {
  switch (state) {
    case IDLE:
      handleStart();
      break;

    case PRECHECK:  // unpowered board: no rail should carry voltage
      if (readRail(CH_RAIL_5V, DIV_5V) > LEAKAGE_MAX_V ||
          readRail(CH_RAIL_3V3, DIV_3V3) > LEAKAGE_MAX_V) {
        fail(E_LEAKAGE);
      } else {
        state = POWER_UP;
      }
      break;

    case POWER_UP:  // apply power and watch current immediately for shorts
      powerOn();
      delay(50);
      if (ina.getCurrent_mA() > I_MAX_MA) {
        fail(E_OVERCURRENT);
      } else {
        delay(SETTLE_MS);
        state = RAILS;
      }
      break;

    case RAILS:
      v5 = readRail(CH_RAIL_5V, DIV_5V);
      v33 = readRail(CH_RAIL_3V3, DIV_3V3);
      if (!inTolerance(v5, RAIL_5V_NOM)) fail(E_RAIL_5V);
      else if (!inTolerance(v33, RAIL_3V3_NOM)) fail(E_RAIL_3V3);
      else state = CURRENT;
      break;

    case CURRENT:
      iMa = ina.getCurrent_mA();
      if (iMa > I_MAX_MA) fail(E_OVERCURRENT);
      else if (iMa < I_MIN_MA) fail(E_NO_CURRENT);
      else state = FUNCTIONAL;
      break;

    case FUNCTIONAL:
      if (!functionalTest()) fail(E_FUNCTIONAL);
      else state = DONE;
      break;

    case DONE: {
      powerOff();
      bool pass = (strcmp(failCode, E_NONE) == 0);
      indicate(pass);
      Serial.printf("RESULT,%s,%s,%.2f,%.2f,%.1f,%s\n", serialNo.c_str(),
                    pass ? "PASS" : "FAIL", v5, v33, iMa, failCode);
      state = IDLE;
      break;
    }
  }
}
