#pragma once

#include <Arduino.h>

#include "sensors/LightSensorPair.h"
#include "drivers/MotorDriver.h"
#include "track/TrackerController.h"
#include "track/TravelGuard.h"
#include "sensors/Dht11Sensor.h"
#include "display/DisplayManager.h"
#include "sensors/TouchButton.h"

namespace ProjectConfig {

//! ----- Tracking H axis (Horizontal) -----
// LDR pins (analog inputs)
static const int LDR_H_PIN_A = 33;
static const int LDR_H_PIN_B = 35;

// Motor driver pins (H-bridge inputs)
static const int MOTOR_H_IN1_PIN = 16;
static const int MOTOR_H_IN2_PIN = 17;

// Timing for light tracking (ms)
static const unsigned long READ_INTERVAL_MS = 3;
static const unsigned long ACTION_INTERVAL_MS = 120;
static const unsigned long MOTOR_UPDATE_INTERVAL_MS = 30;
static const unsigned long AUTO_BLOCK_DEADBAND_HOLD_MS = 1500;
static const unsigned long AUTO_BLOCK_DURATION_MS = 10000;

// Diff thresholds (percent). Deadband stops the motor target (0 PWM).
static const float DIFF_DEADBAND_H = 1.0f;
static const float DIFF_PWM_THRESHOLD_H = 15.0f;

// Low-light adaptive deadband (percent, based on max(avg_a, avg_b) ADC counts):
// <500 => 5%, <200 => 20%, <100 => 100%.
static const uint32_t LOW_LIGHT_LEVEL_1 = 500;
static const float LOW_LIGHT_DEADBAND_1_PERCENT = 5.0f;
static const uint32_t LOW_LIGHT_LEVEL_2 = 200;
static const float LOW_LIGHT_DEADBAND_2_PERCENT = 20.0f;
static const uint32_t LOW_LIGHT_LEVEL_3 = 100;
static const float LOW_LIGHT_DEADBAND_3_PERCENT = 100.0f;

// PWM config (normalized min/max, 0..1)
static const int MOTOR_PWM_FREQ_H = 20000;
static const int MOTOR_PWM_RES_BITS_H = 8;
static const int MOTOR_PWM_CH_IN1_H = 0;
static const int MOTOR_PWM_CH_IN2_H = 1;
static const float MOTOR_PWM_MIN_NORM_H = 0.8f; // 0..1
static const float MOTOR_PWM_MAX_NORM_H = 0.99f; // 0..1
static const float MOTOR_PWM_SMOOTH_H = 0.8f;   // 0..1 (0 = instant, 1 = very smooth)
static const float MOTOR_PWM_KICK_NORM_H = 0.8f; // 0..1
static const unsigned long MOTOR_PWM_KICK_MS_H = 200;

// Logging toggle for H tracking
static const bool LOG_H_ENABLED = true;

// Light sensor pair configuration (H)
static const LightSensorPair::Config SENSOR_CFG_H = {
    LDR_H_PIN_A,
    LDR_H_PIN_B,
    READ_INTERVAL_MS,
    ACTION_INTERVAL_MS
};

// Tracking controller configuration (H)
static const TrackerController::Config TRACKER_CFG_H = {
    DIFF_DEADBAND_H,
    DIFF_PWM_THRESHOLD_H,
    MOTOR_PWM_MIN_NORM_H,
    MOTOR_PWM_MAX_NORM_H,
    LOW_LIGHT_LEVEL_1,
    LOW_LIGHT_DEADBAND_1_PERCENT,
    LOW_LIGHT_LEVEL_2,
    LOW_LIGHT_DEADBAND_2_PERCENT,
    LOW_LIGHT_LEVEL_3,
    LOW_LIGHT_DEADBAND_3_PERCENT
};

// Motor driver configuration (H)
static const MotorDriver::Config MOTOR_CFG_H = {
    MOTOR_H_IN1_PIN,
    MOTOR_H_IN2_PIN,
    MOTOR_PWM_FREQ_H,
    MOTOR_PWM_RES_BITS_H,
    MOTOR_PWM_CH_IN1_H,
    MOTOR_PWM_CH_IN2_H,
    MOTOR_PWM_SMOOTH_H,
    MOTOR_UPDATE_INTERVAL_MS,
    MOTOR_PWM_KICK_NORM_H,
    MOTOR_PWM_KICK_MS_H
};

//! ----- Tracking V axis (Vertical) -----
// LDR pins (analog inputs) - set to H if you want to mirror for testing
static const int LDR_V_PIN_A = 32;
static const int LDR_V_PIN_B = 34;

// Motor driver pins (H-bridge inputs)
static const int MOTOR_V_IN1_PIN = -1;
static const int MOTOR_V_IN2_PIN = -1;

// Timing for light tracking uses global values above.

// Diff thresholds (percent). Deadband stops the motor target (0 PWM).
static const float DIFF_DEADBAND_V = 1.0f;
static const float DIFF_PWM_THRESHOLD_V = 10.0f;

// PWM config (normalized min/max, 0..1)
static const int MOTOR_PWM_FREQ_V = 20000;
static const int MOTOR_PWM_RES_BITS_V = 8;
static const int MOTOR_PWM_CH_IN1_V = 2;
static const int MOTOR_PWM_CH_IN2_V = 3;
static const float MOTOR_PWM_MIN_NORM_V = 0.8f; // 0..1
static const float MOTOR_PWM_MAX_NORM_V = 0.99f; // 0..1
static const float MOTOR_PWM_SMOOTH_V = 0.8f;   // 0..1 (0 = instant, 1 = very smooth)
static const float MOTOR_PWM_KICK_NORM_V = 0.8f; // 0..1
static const unsigned long MOTOR_PWM_KICK_MS_V = 200;

// Logging toggle for V tracking
static const bool LOG_V_ENABLED = true;

// Light sensor pair configuration (V)
static const LightSensorPair::Config SENSOR_CFG_V = {
    LDR_V_PIN_A,
    LDR_V_PIN_B,
    READ_INTERVAL_MS,
    ACTION_INTERVAL_MS
};

// Tracking controller configuration (V)
static const TrackerController::Config TRACKER_CFG_V = {
    DIFF_DEADBAND_V,
    DIFF_PWM_THRESHOLD_V,
    MOTOR_PWM_MIN_NORM_V,
    MOTOR_PWM_MAX_NORM_V,
    LOW_LIGHT_LEVEL_1,
    LOW_LIGHT_DEADBAND_1_PERCENT,
    LOW_LIGHT_LEVEL_2,
    LOW_LIGHT_DEADBAND_2_PERCENT,
    LOW_LIGHT_LEVEL_3,
    LOW_LIGHT_DEADBAND_3_PERCENT
};

// Motor driver configuration (V)
static const MotorDriver::Config MOTOR_CFG_V = {
    MOTOR_V_IN1_PIN,
    MOTOR_V_IN2_PIN,
    MOTOR_PWM_FREQ_V,
    MOTOR_PWM_RES_BITS_V,
    MOTOR_PWM_CH_IN1_V,
    MOTOR_PWM_CH_IN2_V,
    MOTOR_PWM_SMOOTH_V,
    MOTOR_UPDATE_INTERVAL_MS,
    MOTOR_PWM_KICK_NORM_V,
    MOTOR_PWM_KICK_MS_V
};

//! ----- Deep sleep config -----
static const unsigned long SLEEP_INTERVAL_SEC = 30;

//! ----- TFT ST7789 config -----
static const int TFT_PIN_SCK = 18;
static const int TFT_PIN_MOSI = 23;
static const int TFT_PIN_DC = 2;
static const int TFT_PIN_RST = 4;
static const int TFT_PIN_BLK = 27;
static const bool TFT_BLK_ACTIVE_HIGH = true;
static const int TFT_PIN_CS = -1; 
static const unsigned long TFT_REFRESH_INTERVAL_MS = 30;
static const float DISPLAY_DEADBAND_PERCENT =
    (DIFF_DEADBAND_H > DIFF_DEADBAND_V) ? DIFF_DEADBAND_H : DIFF_DEADBAND_V;
static const float DISPLAY_PWM_THRESHOLD_PERCENT =
    (DIFF_PWM_THRESHOLD_H > DIFF_PWM_THRESHOLD_V) ? DIFF_PWM_THRESHOLD_H : DIFF_PWM_THRESHOLD_V;

// Display configuration
static const DisplayManager::Config DISPLAY_CFG = {
    TFT_PIN_DC,
    TFT_PIN_RST,
    TFT_PIN_BLK,
    TFT_BLK_ACTIVE_HIGH,
    TFT_REFRESH_INTERVAL_MS
};

// Battery (mock for now)
static const float BATTERY_PERCENT_MOCK = 80.0f;
static const float SOLAR_PERCENT_MOCK = 45.0f;
static const bool SOLAR_CHARGING_MOCK = true;

//! ----- DHT11 config -----
static const int DHT11_PIN = 21;
static const unsigned long DHT11_REPORT_INTERVAL_MS = 20000;
static const unsigned int DHT11_SAMPLES_PER_REPORT = 5;
static const int DHT11_TYPE = DHT11;

// Logging toggle for DHT
static const bool DHT_LOG_ENABLED = true;

// DHT11 sensor configuration
static const Dht11Sensor::Config DHT_CFG = {
    DHT11_PIN,
    DHT11_REPORT_INTERVAL_MS,
    DHT11_SAMPLES_PER_REPORT,
    DHT11_TYPE
};

//! ----- Touch button config -----
static const int TOUCH_BUTTON_PIN = 15;
static const bool TOUCH_BUTTON_ACTIVE_HIGH = true;
static const unsigned long TOUCH_BUTTON_DEBOUNCE_MS = 40;
static const unsigned long TOUCH_BUTTON_LONG_PRESS_MS = 2000;
static const bool TOUCH_BUTTON_LOG_ENABLED = true;

static const TouchButton::Config TOUCH_BUTTON_CFG = {
    TOUCH_BUTTON_PIN,
    TOUCH_BUTTON_ACTIVE_HIGH,
    TOUCH_BUTTON_DEBOUNCE_MS,
    TOUCH_BUTTON_LONG_PRESS_MS
};

//! ----- Travel guard (NC endstops) -----
// NC switches with pull-up: pressed/open circuit -> HIGH on input.
static const int TRAVEL_GUARD_PIN_1 = 5;
static const int TRAVEL_GUARD_PIN_2 = 22;
static const bool TRAVEL_GUARD_ACTIVE_HIGH = true;
static const bool TRAVEL_GUARD_USE_PULLUP = true;
static const unsigned long TRAVEL_GUARD_DEBOUNCE_MS = 25;
static const float TRAVEL_GUARD_SWEEP_NORM = MOTOR_PWM_MAX_NORM_V;
static const int TRAVEL_GUARD_DIR_FROM_PIN_1 = +1; // When pin 1 is hit, move towards pin 2
static const int TRAVEL_GUARD_DIR_FROM_PIN_2 = -1; // When pin 2 is hit, move towards pin 1

static const TravelGuard::Config TRAVEL_GUARD_CFG = {
    TRAVEL_GUARD_PIN_1,
    TRAVEL_GUARD_PIN_2,
    TRAVEL_GUARD_ACTIVE_HIGH,
    TRAVEL_GUARD_USE_PULLUP,
    TRAVEL_GUARD_DEBOUNCE_MS,
    TRAVEL_GUARD_SWEEP_NORM,
    TRAVEL_GUARD_DIR_FROM_PIN_1,
    TRAVEL_GUARD_DIR_FROM_PIN_2
};

} // namespace ProjectConfig


