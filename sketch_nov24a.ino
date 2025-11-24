#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include <Adafruit_Sensor.h>

// ---------------------- KC868-A6 PINOUT ASSUMPTIONS ----------------------
static const int I2C_SDA_PIN = 4;   // I2C bus used by onboard display
static const int I2C_SCL_PIN = 15;

static const int PIN_VOLTAGE_ADC = 36;  // A1 -> ZMPT101B 0-5V input
static const int PIN_CURRENT_ADC = 39;  // A2 -> ACS758 (fed with 3.3V)

// PCF8574 for 6 relays (P0..P5)
static const uint8_t PCF8574_ADDR = 0x24;  // adjust if needed (0x20–0x27)

// ---------------------- ADC & SENSOR CALIBRATION -------------------------
static const float ADC_REF_VOLT      = 3.3f;
static const int   ADC_MAX_VALUE     = 4095;

// Tune this so Serial shows ~110V in your current setup
static const float ZMPT_MAX_VOLTAGE  = 120.0f;  

// ACS758 parameters (demo-tuned, not physically exact)
static const float ACS_SUPPLY_VOLT          = 3.3f;
static const float ACS_ZERO_CURRENT_V       = ACS_SUPPLY_VOLT / 2.0f; // ~1.65V
// Bigger sensitivity → smaller displayed amps (for demo)
static const float ACS_SENSITIVITY_V_PER_A  = 0.264f;  // ~10x smaller reading vs before

// ---------------------- CONTROL THRESHOLDS (DEMO-FRIENDLY) ---------------
static const float SETPOINT_TEMP_C          = 23.0f;   // °C
static const float SETPOINT_HUM_PCT         = 60.0f;   // %RH
static const float MAX_ALLOWED_VOLTAGE      = 130.0f;  // VAC cutoff
static const float MAX_ALLOWED_CURRENT_A    = 5.0f;    // A cutoff

// ---------------------- SAMPLING / AVERAGING -----------------------------
// 20 samples per second -> 40 samples = 2 seconds window
static const unsigned long SAMPLE_PERIOD_MS = 50;      // 20 Hz
static const int WINDOW_SIZE                = 40;      // 2 s window

// ---------------------- PID PARAMETERS -----------------------------------
static const float Kp = 0.4f;
static const float Ki = 0.05f;
static const float Kd = 0.0f;

// ---------------------- GLOBAL OBJECTS -----------------------------------
Adafruit_SHT4x sht4;

// relay shadow register (last value written to PCF8574)
// Assume active-LOW relays: bit=0 -> relay ON, bit=1 -> relay OFF
uint8_t relayShadow = 0xFF; // all OFF

// Buffers for moving average (2 s window)
float tempBuf[WINDOW_SIZE];
float humBuf[WINDOW_SIZE];
float voltBuf[WINDOW_SIZE];
float currBuf[WINDOW_SIZE];

int  sampleIndex   = 0;
bool bufferFilled  = false;

float avgTemp = 0.0f;
float avgHum  = 0.0f;
float avgVolt = 0.0f;
float avgCurr = 0.0f;

unsigned long lastSampleMs = 0;
unsigned long lastPrintMs  = 0;

// PID state
float pidIntegral  = 0.0f;
float pidPrevError = 0.0f;

// ---------------------- FORWARD DECLARATIONS -----------------------------
void setAllRelays(bool on);
void writePCF8574(uint8_t value);

void sampleSensors();
void updateAverages();
void runControl();

float convertVoltageFromADC(int raw);
float convertCurrentFromADC(int raw);

// ========================================================================
// SETUP
// ========================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // I2C shared for SHT45 and PCF8574
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);   // 400 kHz I2C

  // SHT4x (SHT45) init
  if (!sht4.begin()) {
    Serial.println("ERROR: SHT4x (SHT45) not found on I2C. Check wiring.");
    while (1) {
      delay(1000);
    }
  }
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  // Initialize relays: all OFF
  relayShadow = 0xFF;  // all bits HIGH (assuming active-low)
  writePCF8574(relayShadow);

  // Init buffers
  for (int i = 0; i < WINDOW_SIZE; i++) {
    tempBuf[i] = 0.0f;
    humBuf[i]  = 0.0f;
    voltBuf[i] = 0.0f;
    currBuf[i] = 0.0f;
  }

  lastSampleMs = millis();
  lastPrintMs  = millis();

  Serial.println("KC868-A6 env+power controller (2s media, demo thresholds).");
}

// ========================================================================
// LOOP
// ========================================================================
void loop() {
  unsigned long now = millis();
  if (now - lastSampleMs >= SAMPLE_PERIOD_MS) {
    lastSampleMs += SAMPLE_PERIOD_MS;

    sampleSensors();    // read SHT45 + analogs + push into buffers
    updateAverages();   // compute avgTemp, avgHum, avgVolt, avgCurr
    runControl();       // PID + thresholds -> relays

    // Print only once every 2 seconds (the media window)
    if (now - lastPrintMs >= 2000) {
      lastPrintMs = now;

      Serial.print("2s AVG  T=");
      Serial.print(avgTemp, 2);
      Serial.print("C  RH=");
      Serial.print(avgHum, 1);
      Serial.print("%  V=");
      Serial.print(avgVolt, 1);
      Serial.print("V  I=");
      Serial.print(avgCurr, 2);
      Serial.print("A  | RelaysShadow=0b");
      for (int i = 7; i >= 0; i--) {
        Serial.print((relayShadow >> i) & 0x01);
      }
      Serial.println();
    }
  }
}

// ========================================================================
// SENSOR SAMPLING (20 Hz)
// ========================================================================
void sampleSensors() {
  // --- SHT45: temperature & humidity ---
  sensors_event_t humidityEvent, tempEvent;
  sht4.getEvent(&humidityEvent, &tempEvent);  // fresh data

  float t = tempEvent.temperature;           // °C
  float h = humidityEvent.relative_humidity; // %RH

  // --- Analog: voltage (ZMPT101B) & current (ACS758) ---
  int rawVolt = analogRead(PIN_VOLTAGE_ADC);
  int rawCurr = analogRead(PIN_CURRENT_ADC);

  float v = convertVoltageFromADC(rawVolt);  // ~VAC (scaled)
  float c = convertCurrentFromADC(rawCurr);  // A   (scaled)

  // Store into circular buffers
  tempBuf[sampleIndex] = t;
  humBuf[sampleIndex]  = h;
  voltBuf[sampleIndex] = v;
  currBuf[sampleIndex] = c;

  sampleIndex++;
  if (sampleIndex >= WINDOW_SIZE) {
    sampleIndex  = 0;
    bufferFilled = true;
  }
}

// ========================================================================
// MOVING AVERAGE OVER LAST WINDOW_SIZE SAMPLES (2 s)
// ========================================================================
void updateAverages() {
  int count;
  if (bufferFilled) {
    count = WINDOW_SIZE;
  } else {
    if (sampleIndex == 0) {
      // nothing yet
      return;
    }
    count = sampleIndex;
  }

  float sumT = 0.0f;
  float sumH = 0.0f;
  float sumV = 0.0f;
  float sumC = 0.0f;

  for (int i = 0; i < count; i++) {
    sumT += tempBuf[i];
    sumH += humBuf[i];
    sumV += voltBuf[i];
    sumC += currBuf[i];
  }

  avgTemp = sumT / count;
  avgHum  = sumH / count;
  avgVolt = sumV / count;
  avgCurr = sumC / count;
}

// ========================================================================
// CONTROL LOGIC (PID + HARD LIMITS)
// ========================================================================
void runControl() {
  // Wait until we have a reasonable chunk of samples
  if (!bufferFilled && sampleIndex < WINDOW_SIZE / 2) {
    setAllRelays(false);
    return;
  }

  // Safety override on voltage/current
  float absCurrent = (avgCurr >= 0.0f) ? avgCurr : -avgCurr;

  if (avgVolt > MAX_ALLOWED_VOLTAGE || absCurrent > MAX_ALLOWED_CURRENT_A) {
    setAllRelays(false);
    pidIntegral  = 0.0f;
    pidPrevError = 0.0f;
    return;
  }

  // If below temp or humidity thresholds, keep OFF and reset PID
  if (avgTemp <= SETPOINT_TEMP_C || avgHum < SETPOINT_HUM_PCT) {
    setAllRelays(false);
    pidIntegral  = 0.0f;
    pidPrevError = 0.0f;
    return;
  }

  // PID-ish: combine temp + humidity excess
  float errorT = avgTemp - SETPOINT_TEMP_C;      // >0 when hotter than setpoint
  float errorH = avgHum  - SETPOINT_HUM_PCT;     // >0 when more humid than setpoint
  if (errorH < 0.0f) errorH = 0.0f;

  float error = errorT + 0.25f * errorH;

  float dt = SAMPLE_PERIOD_MS / 1000.0f;  // 0.05 s

  pidIntegral += error * dt;
  float derivative = (error - pidPrevError) / dt;
  pidPrevError = error;

  float output = Kp * error + Ki * pidIntegral + Kd * derivative;

  if (output < 0.0f) output = 0.0f;
  if (output > 1.0f) output = 1.0f;

  // Threshold the PID output -> ON/OFF (you can tune 0.2)
  bool wantOn = (output > 0.2f);

  setAllRelays(wantOn);
}

// ========================================================================
// RELAY CONTROL VIA PCF8574 (ACTIVE-LOW ASSUMPTION)
// ========================================================================
void setAllRelays(bool on) {
  // P0..P5 are used for relays (6 bits -> mask 0b0011 1111 = 0x3F)
  const uint8_t relayMask = 0x3F;

  if (on) {
    // active-LOW: 0 -> ON; so clear bits 0..5
    relayShadow &= ~relayMask;
  } else {
    // OFF -> bits 0..5 HIGH
    relayShadow |= relayMask;
  }

  writePCF8574(relayShadow);
}

void writePCF8574(uint8_t value) {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(value);
  Wire.endTransmission();
}

// ========================================================================
// CONVERSIONS (DEMO CALIBRATION)
// ========================================================================
float convertVoltageFromADC(int raw) {
  // Simple linear scale. Tune ZMPT_MAX_VOLTAGE until this prints ~110 V.
  float fraction = (float)raw / (float)ADC_MAX_VALUE;
  float voltage  = fraction * ZMPT_MAX_VOLTAGE;
  return voltage;
}

float convertCurrentFromADC(int raw) {
  // Map ADC -> ACS758 current (bidirectional, ratiometric, then abs for demo)
  float vOut  = ((float)raw / (float)ADC_MAX_VALUE) * ACS_SUPPLY_VOLT;
  float vDiff = vOut - ACS_ZERO_CURRENT_V;  // +/- around zero-current

  float current = 0.0f;
  if (ACS_SENSITIVITY_V_PER_A > 1e-6f) {
    current = vDiff / ACS_SENSITIVITY_V_PER_A;
  }

  // For demo: force positive "low" amp reading
  if (current < 0.0f) current = -current;

  return current;
}
