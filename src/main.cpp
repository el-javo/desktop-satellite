#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#include "track/TrackingUnit.h"
#include "track/TrackingCoordinator.h"
#include "track/TravelGuard.h"
#include "sensors/Dht11Sensor.h"
#include "sensors/TouchButton.h"
#include "display/DisplayManager.h"
#include "config/ProjectConfig.h"

enum class SystemMode {
    Active,
    ActiveBlocked,
    DeepSleep
};

static const char* systemModeName(SystemMode mode) {
    switch (mode) {
    case SystemMode::Active:
        return "ACTIVE";
    case SystemMode::ActiveBlocked:
        return "ACTIVE_BLOCKED";
    case SystemMode::DeepSleep:
        return "DEEPSLEEP";
    default:
        return "?";
    }
}

TrackingUnit tracking_unit_h(
    ProjectConfig::SENSOR_CFG_H,
    ProjectConfig::TRACKER_CFG_H,
    ProjectConfig::MOTOR_CFG_H);
TrackingUnit tracking_unit_v(
    ProjectConfig::SENSOR_CFG_V,
    ProjectConfig::TRACKER_CFG_V,
    ProjectConfig::MOTOR_CFG_V);
TrackingCoordinator tracking_coordinator(
    {
        ProjectConfig::AUTO_BLOCK_DEADBAND_HOLD_MS,
        ProjectConfig::AUTO_BLOCK_DURATION_MS
    },
    tracking_unit_h,
    tracking_unit_v);
TravelGuard travel_guard(ProjectConfig::TRAVEL_GUARD_CFG);

Dht11Sensor dht11(ProjectConfig::DHT_CFG);
TouchButton touch_button(ProjectConfig::TOUCH_BUTTON_CFG);
DisplayManager display(ProjectConfig::DISPLAY_CFG);
SystemMode system_mode = SystemMode::Active;

static void applySystemMode(SystemMode mode) {
    if (mode == SystemMode::Active) {
        tracking_coordinator.setEnabled(true);
        tracking_coordinator.resetState();
        tracking_unit_h.setMotorOverride(true);
        tracking_unit_v.setMotorOverride(true);
    } else if (mode == SystemMode::ActiveBlocked) {
        tracking_coordinator.setEnabled(false);
        tracking_coordinator.resetState();
        tracking_unit_h.setMotorOverride(false);
        tracking_unit_v.setMotorOverride(false);
    } else {
        tracking_coordinator.setEnabled(false);
        tracking_coordinator.resetState();
        tracking_unit_h.clearMotorOverride();
        tracking_unit_v.clearMotorOverride();
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
    tracking_unit_v.clearTargetOverride();
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
    delay(100);
    Serial.println("[DBG] Boot");
    Serial.print("[DBG] TravelGuard pins: ");
    Serial.print(ProjectConfig::TRAVEL_GUARD_PIN_1);
    Serial.print(", ");
    Serial.print(ProjectConfig::TRAVEL_GUARD_PIN_2);
    Serial.print(" | active_high=");
    Serial.print(ProjectConfig::TRAVEL_GUARD_ACTIVE_HIGH ? 1 : 0);
    Serial.print(" | pullup=");
    Serial.println(ProjectConfig::TRAVEL_GUARD_USE_PULLUP ? 1 : 0);
    Serial.print("[DBG] Motor V pins: ");
    Serial.print(ProjectConfig::MOTOR_V_IN1_PIN);
    Serial.print(", ");
    Serial.print(ProjectConfig::MOTOR_V_IN2_PIN);
    Serial.print(" | smooth=");
    Serial.println(ProjectConfig::MOTOR_PWM_SMOOTH_V, 3);
    if (ProjectConfig::MOTOR_V_IN1_PIN < 0 || ProjectConfig::MOTOR_V_IN2_PIN < 0) {
        Serial.println("[DBG][WARN] Motor V disabled in config (pin < 0)");
    }
    if (ProjectConfig::MOTOR_PWM_SMOOTH_V >= 0.999f) {
        Serial.println("[DBG][WARN] Motor V smooth ~1.0 => filtered PWM may stay near 0");
    }

    WiFi.mode(WIFI_OFF);

    releaseBacklightHold();

    // ESP32 ADC configuration
    analogReadResolution(12);       // Range: 0-4095
    analogSetAttenuation(ADC_11db); // Up to ~3.3 V

    tracking_unit_h.begin();
    tracking_unit_v.begin();
    travel_guard.begin();
    dht11.begin();
    touch_button.begin();
    display.begin();
    display.setMode(DisplayManager::Mode::Tracking);
    display.setDeadbandPercent(ProjectConfig::DISPLAY_DEADBAND_PERCENT);
    display.setPwmThresholdPercent(ProjectConfig::DISPLAY_PWM_THRESHOLD_PERCENT);
    display.setMotorPwmRanges(
        ProjectConfig::MOTOR_PWM_MIN_NORM_H,
        ProjectConfig::MOTOR_PWM_MAX_NORM_H,
        ProjectConfig::MOTOR_PWM_MIN_NORM_V,
        ProjectConfig::MOTOR_PWM_MAX_NORM_V);
    display.setBatteryPercent(ProjectConfig::BATTERY_PERCENT_MOCK);
    display.setSolarChargePercent(ProjectConfig::SOLAR_PERCENT_MOCK);
    display.setSolarCharging(ProjectConfig::SOLAR_CHARGING_MOCK);
    esp_sleep_wakeup_cause_t wake = esp_sleep_get_wakeup_cause();
    if (wake == ESP_SLEEP_WAKEUP_TIMER) {
        system_mode = SystemMode::DeepSleep;
    } else if (wake == ESP_SLEEP_WAKEUP_EXT0 ||
               wake == ESP_SLEEP_WAKEUP_EXT1) {
        system_mode = SystemMode::Active;
    }
    applySystemMode(system_mode);
    display.setActiveIndicator(system_mode == SystemMode::Active);
}

void loop() {
    const unsigned long now_ms = millis();
    touch_button.tick(now_ms);
    travel_guard.tick(now_ms);
    const bool travel_sweep_active = travel_guard.isSweepActive();
    const float travel_target_norm = travel_sweep_active
        ? travel_guard.sweepTargetNorm()
        : 0.0f;
    if (travel_sweep_active) {
        tracking_unit_v.setTargetOverride(travel_target_norm);
    } else {
        tracking_unit_v.clearTargetOverride();
    }
    static SystemMode last_mode = system_mode;
    static unsigned long deep_sleep_deadband_ms = 0;
    static bool have_diff_h = false;
    static bool have_diff_v = false;
    static float last_pwm_norm_h = 0.0f;
    static float last_pwm_norm_v = 0.0f;
    if (touch_button.consumeLongPress()) {
        if (system_mode != SystemMode::DeepSleep) {
            system_mode = SystemMode::DeepSleep;
            applySystemMode(system_mode);
            prepareForSleep(now_ms);
            enterDeepSleep();
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
    }
    if (last_mode != system_mode) {
        applySystemMode(system_mode);
        display.setActiveIndicator(system_mode == SystemMode::Active);
        Serial.print("[DBG] Mode -> ");
        Serial.println(systemModeName(system_mode));
        last_mode = system_mode;
    }

    if (system_mode == SystemMode::Active) {
        tracking_coordinator.tick(now_ms);
    }
    if (travel_sweep_active) {
        tracking_unit_v.setMotorOverride(true);
    } else if (system_mode == SystemMode::ActiveBlocked) {
        tracking_unit_v.setMotorOverride(false);
    } else if (system_mode == SystemMode::DeepSleep) {
        tracking_unit_v.clearMotorOverride();
    }
    tracking_unit_h.tick(now_ms);
    tracking_unit_v.tick(now_ms);
    dht11.tick(now_ms);
    {
        static float last_display_deadband = -1.0f;
        if (tracking_unit_h.hasDiffSample() && tracking_unit_v.hasDiffSample()) {
            const float display_deadband = max(
                tracking_unit_h.lastEffectiveDeadband(),
                tracking_unit_v.lastEffectiveDeadband());
            if (display_deadband >= 0.0f &&
                fabsf(display_deadband - last_display_deadband) > 0.0005f) {
                display.setDeadbandPercent(display_deadband);
                last_display_deadband = display_deadband;
            }
        }
    }
    display.setBlocked(
        !(tracking_unit_h.isMotorEnabled() && tracking_unit_v.isMotorEnabled()));

    TrackingUnit::LogSample log_h;
    static float last_diff_percent_h = 0.0f;
    static float last_diff_percent_v = 0.0f;
    if (tracking_unit_h.consumeLog(log_h)) {
        last_diff_percent_h = log_h.diff_percent;
        last_pwm_norm_h = log_h.applied_norm;
        have_diff_h = true;
        display.setTrackingRawH(log_h.avg_a, log_h.avg_b);
        display.setTrackingInfoHV(last_diff_percent_h, last_diff_percent_v);
        display.setMotorPwmHV(last_pwm_norm_h, last_pwm_norm_v);
    }

    TrackingUnit::LogSample log_v;
    if (tracking_unit_v.consumeLog(log_v)) {
        last_diff_percent_v = log_v.diff_percent;
        last_pwm_norm_v = log_v.applied_norm;
        have_diff_v = true;
        display.setTrackingRawV(log_v.avg_a, log_v.avg_b);
        display.setTrackingInfoHV(last_diff_percent_h, last_diff_percent_v);
        display.setMotorPwmHV(last_pwm_norm_h, last_pwm_norm_v);
    }

    {
        static unsigned long last_dbg_ms = 0;
        static bool last_sweep = false;
        static bool last_limit_1 = false;
        static bool last_limit_2 = false;
        static int last_raw_1 = -1;
        static int last_raw_2 = -1;

        const int raw_1 = digitalRead(ProjectConfig::TRAVEL_GUARD_PIN_1);
        const int raw_2 = digitalRead(ProjectConfig::TRAVEL_GUARD_PIN_2);
        const bool limit_1 = travel_guard.isLimit1Pressed();
        const bool limit_2 = travel_guard.isLimit2Pressed();
        const bool changed =
            (last_sweep != travel_sweep_active) ||
            (last_limit_1 != limit_1) ||
            (last_limit_2 != limit_2) ||
            (last_raw_1 != raw_1) ||
            (last_raw_2 != raw_2);

        if (changed || (now_ms - last_dbg_ms) >= 250) {
            Serial.print("[DBG] TG raw=");
            Serial.print(raw_1);
            Serial.print(",");
            Serial.print(raw_2);
            Serial.print(" press=");
            Serial.print(limit_1 ? 1 : 0);
            Serial.print(",");
            Serial.print(limit_2 ? 1 : 0);
            Serial.print(" sweep=");
            Serial.print(travel_sweep_active ? 1 : 0);
            Serial.print(" tgt=");
            Serial.print(travel_target_norm, 3);
            Serial.print(" mode=");
            Serial.print(systemModeName(system_mode));
            Serial.print(" v_en=");
            Serial.print(tracking_unit_v.isMotorEnabled() ? 1 : 0);
            Serial.print(" pwmV=");
            Serial.println(last_pwm_norm_v, 3);

            last_dbg_ms = now_ms;
            last_sweep = travel_sweep_active;
            last_limit_1 = limit_1;
            last_limit_2 = limit_2;
            last_raw_1 = raw_1;
            last_raw_2 = raw_2;
        }
    }

    Dht11Sensor::Sample dht_log;
    if (dht11.consumeSample(dht_log)) {
        display.setEnvironment(dht_log.temperature_c, dht_log.humidity_pct);
    }

    if (system_mode == SystemMode::DeepSleep) {
        if (!(have_diff_h && have_diff_v)) {
            deep_sleep_deadband_ms = 0;
        } else {
            const float db_h = fabsf(tracking_unit_h.lastEffectiveDeadband());
            const float db_v = fabsf(tracking_unit_v.lastEffectiveDeadband());
            const bool in_deadband_h =
                fabsf(last_diff_percent_h) <= db_h;
            const bool in_deadband_v =
                fabsf(last_diff_percent_v) <= db_v;
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

