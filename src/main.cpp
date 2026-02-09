#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

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

static void applySystemMode(SystemMode mode) {
    if (mode == SystemMode::Active) {
        tracking_unit_h.clearMotorOverride();
        tracking_unit_v.clearMotorOverride();
        tracking_unit_h.setAutoBlockEnabled(true);
        tracking_unit_v.setAutoBlockEnabled(true);
    } else if (mode == SystemMode::ActiveBlocked) {
        tracking_unit_h.setAutoBlockEnabled(false);
        tracking_unit_v.setAutoBlockEnabled(false);
        tracking_unit_h.setMotorOverride(false);
        tracking_unit_v.setMotorOverride(false);
    } else {
        tracking_unit_h.clearMotorOverride();
        tracking_unit_v.clearMotorOverride();
        tracking_unit_h.setAutoBlockEnabled(false);
        tracking_unit_v.setAutoBlockEnabled(false);
    }
}

static bool isRtcGpio(int pin) {
    return rtc_gpio_is_valid_gpio((gpio_num_t)pin);
}

static void releaseBacklightHold() {
    if (ProjectConfig::TFT_PIN_BLK >= 0 &&
        isRtcGpio(ProjectConfig::TFT_PIN_BLK)) {
        rtc_gpio_hold_dis((gpio_num_t)ProjectConfig::TFT_PIN_BLK);
        gpio_deep_sleep_hold_dis();
    }
}

static void holdBacklightForSleep() {
    if (ProjectConfig::TFT_PIN_BLK >= 0 &&
        isRtcGpio(ProjectConfig::TFT_PIN_BLK)) {
        rtc_gpio_hold_en((gpio_num_t)ProjectConfig::TFT_PIN_BLK);
        gpio_deep_sleep_hold_en();
    }
}

static void prepareForSleep(unsigned long now_ms) {
    tracking_unit_h.setMotorOverride(false);
    tracking_unit_v.setMotorOverride(false);
    tracking_unit_h.tick(now_ms);
    tracking_unit_v.tick(now_ms);
    display.setMode(DisplayManager::Mode::Off);
    display.setBacklight(false);
    holdBacklightForSleep();
}

static bool isButtonPressedRaw() {
    const bool raw = (digitalRead(ProjectConfig::TOUCH_BUTTON_PIN) != 0);
    return ProjectConfig::TOUCH_BUTTON_ACTIVE_HIGH ? raw : !raw;
}

static void waitForButtonRelease() {
    while (isButtonPressedRaw()) {
        delay(10);
    }
}

static void enterDeepSleep() {
    const uint64_t interval_us =
        (uint64_t)ProjectConfig::SLEEP_INTERVAL_SEC * 1000000ULL;
    waitForButtonRelease();
    esp_sleep_enable_timer_wakeup(interval_us);
    esp_sleep_enable_ext0_wakeup(
        (gpio_num_t)ProjectConfig::TOUCH_BUTTON_PIN,
        ProjectConfig::TOUCH_BUTTON_ACTIVE_HIGH ? 1 : 0);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);

    releaseBacklightHold();

    // ESP32 ADC configuration
    analogReadResolution(12);       // Range: 0-4095
    analogSetAttenuation(ADC_11db); // Up to ~3.3 V

    tracking_unit_h.begin();
    tracking_unit_v.begin();
    tracking_unit_h.setAutoBlockConfig(
        ProjectConfig::AUTO_BLOCK_DEADBAND_HOLD_MS,
        ProjectConfig::AUTO_BLOCK_DURATION_MS);
    tracking_unit_v.setAutoBlockConfig(
        ProjectConfig::AUTO_BLOCK_DEADBAND_HOLD_MS,
        ProjectConfig::AUTO_BLOCK_DURATION_MS);
    dht11.begin();
    touch_button.begin();
    display.begin();
    display.setMode(DisplayManager::Mode::Tracking);
    display.setDeadbandPercent(ProjectConfig::DISPLAY_DEADBAND_PERCENT);
    display.setPwmThresholdPercent(ProjectConfig::DISPLAY_PWM_THRESHOLD_PERCENT);
    esp_sleep_wakeup_cause_t wake = esp_sleep_get_wakeup_cause();
    if (wake == ESP_SLEEP_WAKEUP_TIMER) {
        system_mode = SystemMode::DeepSleep;
    } else if (wake == ESP_SLEEP_WAKEUP_EXT0 ||
               wake == ESP_SLEEP_WAKEUP_EXT1) {
        system_mode = SystemMode::Active;
    }
    applySystemMode(system_mode);
    display.setActiveIndicator(system_mode == SystemMode::Active);

    Serial.println("System initialized");
    Serial.println("Format: LDR_A | LDR_B | DIFF_% | PWM");
    Serial.print("System mode: ");
    Serial.println(systemModeLabel(system_mode));
}

void loop() {
    const unsigned long now_ms = millis();
    touch_button.tick(now_ms);
    static SystemMode last_mode = system_mode;
    static unsigned long deep_sleep_deadband_ms = 0;
    static bool have_diff_h = false;
    static bool have_diff_v = false;
    if (touch_button.consumeLongPress()) {
        if (ProjectConfig::TOUCH_BUTTON_LOG_ENABLED) {
            Serial.print("Button: LONG_PRESS at ");
            Serial.println(now_ms);
        }
        if (system_mode != SystemMode::DeepSleep) {
            system_mode = SystemMode::DeepSleep;
            Serial.print("System mode: ");
            Serial.println(systemModeLabel(system_mode));
            applySystemMode(system_mode);
            prepareForSleep(now_ms);
            enterDeepSleep();
        }
    }
    if (touch_button.consumeShortPress()) {
        if (ProjectConfig::TOUCH_BUTTON_LOG_ENABLED) {
            Serial.print("Button: SHORT_PRESS at ");
            Serial.println(now_ms);
        }
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
    if (last_mode != system_mode) {
        applySystemMode(system_mode);
        display.setActiveIndicator(system_mode == SystemMode::Active);
        last_mode = system_mode;
    }

    tracking_unit_h.tick(now_ms);
    tracking_unit_v.tick(now_ms);
    dht11.tick(now_ms);
    display.setBlocked(
        !(tracking_unit_h.isMotorEnabled() && tracking_unit_v.isMotorEnabled()));

    TrackingUnit::LogSample log_h;
    static float last_diff_percent_h = 0.0f;
    static float last_diff_percent_v = 0.0f;
    if (tracking_unit_h.consumeLog(log_h)) {
        last_diff_percent_h = log_h.diff_percent;
        have_diff_h = true;
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
        have_diff_v = true;
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

    if (system_mode == SystemMode::DeepSleep) {
        if (!(have_diff_h && have_diff_v)) {
            deep_sleep_deadband_ms = 0;
        } else {
            const bool in_deadband_h =
                fabsf(last_diff_percent_h) <= ProjectConfig::DIFF_DEADBAND_H;
            const bool in_deadband_v =
                fabsf(last_diff_percent_v) <= ProjectConfig::DIFF_DEADBAND_V;
            const bool in_deadband = in_deadband_h && in_deadband_v;

            if (in_deadband) {
                if (deep_sleep_deadband_ms == 0) {
                    deep_sleep_deadband_ms = now_ms;
                } else if (now_ms - deep_sleep_deadband_ms >=
                           ProjectConfig::AUTO_BLOCK_DEADBAND_HOLD_MS) {
                    prepareForSleep(now_ms);
                    enterDeepSleep();
                }
            } else {
                deep_sleep_deadband_ms = 0;
            }
        }
    } else {
        deep_sleep_deadband_ms = 0;
        have_diff_h = false;
        have_diff_v = false;
    }

    display.tick(now_ms);
}
