#include <Arduino.h>
#include <WiFi.h>

#include "track/TrackingUnit.h"

// LDR pins
const int LDR_PIN_A = 15;
const int LDR_PIN_B = 4;

// Motor driver pins
const int MOTOR_IN1_PIN = 16;
const int MOTOR_IN2_PIN = 17;

// Timing
const unsigned long READ_INTERVAL_MS = 30;
const unsigned long ACTION_INTERVAL_MS = 300;
const unsigned long MOTOR_UPDATE_INTERVAL_MS = 30;

// Diff thresholds (percent)
const float DIFF_DEADBAND_POS = 1.0f;
const float DIFF_DEADBAND_NEG = -1.0f;
const float DIFF_PWM_THRESHOLD = 10.0f;

// PWM config
const int MOTOR_PWM_FREQ = 20000;
const int MOTOR_PWM_RES_BITS = 8;
const int MOTOR_PWM_CH_IN1 = 0;
const int MOTOR_PWM_CH_IN2 = 1;
const float MOTOR_PWM_MIN_NORM = 0.3f; // 0..1
const float MOTOR_PWM_MAX_NORM = 0.9f; // 0..1
const float MOTOR_PWM_SMOOTH = 0.3f;   // 0..1

const bool LOG_ENABLED = true;

const LightSensorPair::Config SENSOR_CFG = {
    LDR_PIN_A,
    LDR_PIN_B,
    READ_INTERVAL_MS,
    ACTION_INTERVAL_MS
};

const TrackerController::Config TRACKER_CFG = {
    DIFF_DEADBAND_POS,
    DIFF_DEADBAND_NEG,
    DIFF_PWM_THRESHOLD,
    MOTOR_PWM_MIN_NORM,
    MOTOR_PWM_MAX_NORM
};

const MotorDriver::Config MOTOR_CFG = {
    MOTOR_IN1_PIN,
    MOTOR_IN2_PIN,
    MOTOR_PWM_FREQ,
    MOTOR_PWM_RES_BITS,
    MOTOR_PWM_CH_IN1,
    MOTOR_PWM_CH_IN2,
    MOTOR_PWM_SMOOTH,
    MOTOR_UPDATE_INTERVAL_MS
};

TrackingUnit tracker_unit(SENSOR_CFG, TRACKER_CFG, MOTOR_CFG);

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);

    // ESP32 ADC configuration
    analogReadResolution(12);       // Range: 0-4095
    analogSetAttenuation(ADC_11db); // Up to ~3.3 V

    tracker_unit.begin();

    Serial.println("System initialized");
    Serial.println("Format: LDR_A | LDR_B | DIFF_% | PWM");
}

void loop() {
    const unsigned long now_ms = millis();
    tracker_unit.tick(now_ms);

    if (LOG_ENABLED) {
        TrackingUnit::LogSample log;
        if (tracker_unit.consumeLog(log)) {
            Serial.print("LDR_A=");
            Serial.print(log.avg_a);
            Serial.print(" | LDR_B=");
            Serial.print(log.avg_b);
            Serial.print(" | DIFF_AVG=");
            Serial.print(log.diff_percent, 2);
            Serial.print(" %");
            Serial.print(" | PWM=");
            Serial.println(log.applied_raw);
        }
    }
}
