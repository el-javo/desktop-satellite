#include <Arduino.h>
#include <WiFi.h>

#include "track/TrackingUnit.h"
#include "sensors/Dht11Sensor.h"
#include "sensors/TouchButton.h"
#include "display/DisplayManager.h"
#include "config/ProjectConfig.h"

enum class SystemMode {
    Active,
    ActiveBlocked,
    DeepSleep
};

TrackingUnit tracking_unit_h(
    ProjectConfig::SENSOR_CFG_H,
    ProjectConfig::TRACKER_CFG_H,
    ProjectConfig::MOTOR_CFG_H);
TrackingUnit tracking_unit_v(
    ProjectConfig::SENSOR_CFG_V,
    ProjectConfig::TRACKER_CFG_V,
    ProjectConfig::MOTOR_CFG_V);

Dht11Sensor dht11(ProjectConfig::DHT_CFG);
TouchButton touch_button(ProjectConfig::TOUCH_BUTTON_CFG);
DisplayManager display(ProjectConfig::DISPLAY_CFG);
SystemMode system_mode = SystemMode::Active;

static const char* systemModeLabel(SystemMode mode) {
    switch (mode) {
    case SystemMode::Active:
        return "ACTIVE";
    case SystemMode::ActiveBlocked:
        return "ACTIVE_BLOCKED";
    case SystemMode::DeepSleep:
        return "DEEPSLEEP";
    default:
        return "UNKNOWN";
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);

    // ESP32 ADC configuration
    analogReadResolution(12);       // Range: 0-4095
    analogSetAttenuation(ADC_11db); // Up to ~3.3 V

    tracking_unit_h.begin();
    tracking_unit_v.begin();
    dht11.begin();
    touch_button.begin();
    display.begin();
    display.setMode(DisplayManager::Mode::Tracking);
    display.setDeadbandPercent(ProjectConfig::DISPLAY_DEADBAND_PERCENT);
    display.setPwmThresholdPercent(ProjectConfig::DISPLAY_PWM_THRESHOLD_PERCENT);

    Serial.println("System initialized");
    Serial.println("Format: LDR_A | LDR_B | DIFF_% | PWM");
    Serial.print("System mode: ");
    Serial.println(systemModeLabel(system_mode));
}

void loop() {
    const unsigned long now_ms = millis();
    touch_button.tick(now_ms);
    if (touch_button.consumeLongPress()) {
        if (system_mode != SystemMode::DeepSleep) {
            system_mode = SystemMode::DeepSleep;
            Serial.print("System mode: ");
            Serial.println(systemModeLabel(system_mode));
        }
    }
    if (touch_button.consumeShortPress()) {
        if (system_mode == SystemMode::DeepSleep) {
            system_mode = SystemMode::Active;
        } else if (system_mode == SystemMode::Active) {
            system_mode = SystemMode::ActiveBlocked;
        } else if (system_mode == SystemMode::ActiveBlocked) {
            system_mode = SystemMode::Active;
        }
        Serial.print("System mode: ");
        Serial.println(systemModeLabel(system_mode));
    }

    tracking_unit_h.tick(now_ms);
    tracking_unit_v.tick(now_ms);
    dht11.tick(now_ms);

    TrackingUnit::LogSample log_h;
    static float last_diff_percent_h = 0.0f;
    static float last_diff_percent_v = 0.0f;
    if (tracking_unit_h.consumeLog(log_h)) {
        last_diff_percent_h = log_h.diff_percent;
        display.setTrackingInfoHV(last_diff_percent_h, last_diff_percent_v);
        if (ProjectConfig::LOG_H_ENABLED) {
            Serial.print("LDR_H_A=");
            Serial.print(log_h.avg_a);
            Serial.print(" | LDR_H_B=");
            Serial.print(log_h.avg_b);
            Serial.print(" | DIFF_H=");
            Serial.print(log_h.diff_percent, 2);
            Serial.print(" %");
            Serial.print(" | PWM_H=");
            Serial.println(log_h.applied_raw);
        }
    }

    TrackingUnit::LogSample log_v;
    if (tracking_unit_v.consumeLog(log_v)) {
        last_diff_percent_v = log_v.diff_percent;
        display.setTrackingInfoHV(last_diff_percent_h, last_diff_percent_v);
        if (ProjectConfig::LOG_V_ENABLED) {
            Serial.print("LDR_V_A=");
            Serial.print(log_v.avg_a);
            Serial.print(" | LDR_V_B=");
            Serial.print(log_v.avg_b);
            Serial.print(" | DIFF_V=");
            Serial.print(log_v.diff_percent, 2);
            Serial.print(" %");
            Serial.print(" | PWM_V=");
            Serial.println(log_v.applied_raw);
        }
    }

    Dht11Sensor::Sample dht_log;
    if (dht11.consumeSample(dht_log)) {
        display.setEnvironment(dht_log.temperature_c, dht_log.humidity_pct);
        if (ProjectConfig::DHT_LOG_ENABLED) {
            Serial.print("DHT11 T=");
            Serial.print(dht_log.temperature_c, 1);
            Serial.print(" C");
            Serial.print(" | H=");
            Serial.print(dht_log.humidity_pct, 1);
            Serial.println(" %");
        }
    }

    display.tick(now_ms);
}
