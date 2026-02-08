#include <Arduino.h>
#include <WiFi.h>

#include "track/TrackingUnit.h"
#include "sensors/Dht11Sensor.h"
#include "display/DisplayManager.h"
#include "config/ProjectConfig.h"

TrackingUnit tracker_unit(
    ProjectConfig::SENSOR_CFG_H,
    ProjectConfig::TRACKER_CFG_H,
    ProjectConfig::MOTOR_CFG_H);

Dht11Sensor dht11(ProjectConfig::DHT_CFG);
DisplayManager display(ProjectConfig::DISPLAY_CFG);

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);

    // ESP32 ADC configuration
    analogReadResolution(12);       // Range: 0-4095
    analogSetAttenuation(ADC_11db); // Up to ~3.3 V

    tracker_unit.begin();
    dht11.begin();
    display.begin();
    display.setMode(DisplayManager::Mode::Tracking);
    display.setDeadzonePercent(ProjectConfig::DIFF_DEADBAND_POS_H);
    display.setPwmThresholdPercent(ProjectConfig::DIFF_PWM_THRESHOLD_H);

    Serial.println("System initialized");
    Serial.println("Format: LDR_A | LDR_B | DIFF_% | PWM");
}

void loop() {
    const unsigned long now_ms = millis();
    tracker_unit.tick(now_ms);
    dht11.tick(now_ms);

    TrackingUnit::LogSample log;
    if (tracker_unit.consumeLog(log)) {
        display.setTrackingInfo(log.diff_percent);
        if (ProjectConfig::LOG_H_ENABLED) {
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
