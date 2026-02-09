#pragma once

#include <Arduino.h>

#include "sensors/LightSensorPair.h"
#include "drivers/MotorDriver.h"
#include "track/TrackerController.h"

class TrackingUnit {
public:
    struct LogSample {
        uint32_t avg_a = 0;
        uint32_t avg_b = 0;
        float diff_percent = 0.0f;
        float target_norm = 0.0f;
        float applied_norm = 0.0f;
        uint32_t applied_raw = 0;
    };

    TrackingUnit(const LightSensorPair::Config& s_cfg,
                 const TrackerController::Config& t_cfg,
                 const MotorDriver::Config& m_cfg)
        : sensors_(s_cfg), motor_(m_cfg), tracker_(t_cfg, sensors_, motor_) {}

    void begin() { motor_.begin(); }

    void tick(unsigned long now_ms) {
        sensors_.tick(now_ms);
        tracker_.tick();
        if (tracker_.hasNewSample()) {
            last_diff_percent_ = tracker_.lastSample().diff_percent;
            has_diff_ = true;
        }

        bool motor_enabled = true;
        if (motor_override_active_) {
            motor_enabled = motor_override_enabled_;
        } else if (auto_block_enabled_) {
            motor_enabled = computeAutoBlock(now_ms);
        }
        motor_enabled_last_ = motor_enabled;
        motor_.setEnabled(motor_enabled);
        motor_.tick(now_ms);
    }

    void setAutoBlockConfig(unsigned long deadband_hold_ms, unsigned long block_ms) {
        deadband_hold_ms_ = deadband_hold_ms;
        block_duration_ms_ = block_ms;
    }

    void setAutoBlockEnabled(bool enabled) {
        auto_block_enabled_ = enabled;
        if (!auto_block_enabled_) {
            deadband_enter_ms_ = 0;
            block_until_ms_ = 0;
        }
    }

    void setMotorOverride(bool enabled) {
        motor_override_active_ = true;
        motor_override_enabled_ = enabled;
    }

    void clearMotorOverride() { motor_override_active_ = false; }

    bool isMotorEnabled() const { return motor_enabled_last_; }

    bool consumeLog(LogSample& out) {
        if (!tracker_.hasNewSample()) {
            return false;
        }
        const LightSensorPair::Sample s = tracker_.lastSample();
        out.avg_a = s.avg_a;
        out.avg_b = s.avg_b;
        out.diff_percent = s.diff_percent;
        out.target_norm = tracker_.lastTargetNorm();
        out.applied_norm = motor_.getFilteredNorm();
        out.applied_raw = motor_.getAppliedPwmRaw();
        tracker_.clearNewSample();
        return true;
    }

private:
    bool computeAutoBlock(unsigned long now_ms) {
        if (!has_diff_) {
            return true;
        }

        const float diff_abs = fabsf(last_diff_percent_);
        const float deadband_abs = fabsf(tracker_.deadband());
        const bool in_deadband = diff_abs <= deadband_abs;
        const bool is_blocked = now_ms < block_until_ms_;

        if (is_blocked) {
            return false;
        }

        if (block_until_ms_ > 0 && in_deadband) {
            block_until_ms_ = now_ms + block_duration_ms_;
            return false;
        }

        if (!in_deadband) {
            deadband_enter_ms_ = 0;
            block_until_ms_ = 0;
            return true;
        }

        if (deadband_enter_ms_ == 0) {
            deadband_enter_ms_ = now_ms;
        }
        if ((now_ms - deadband_enter_ms_) >= deadband_hold_ms_) {
            block_until_ms_ = now_ms + block_duration_ms_;
            deadband_enter_ms_ = 0;
            return false;
        }

        return true;
    }

    LightSensorPair sensors_;
    MotorDriver motor_;
    TrackerController tracker_;
    bool auto_block_enabled_ = false;
    unsigned long deadband_hold_ms_ = 1500;
    unsigned long block_duration_ms_ = 10000;
    unsigned long deadband_enter_ms_ = 0;
    unsigned long block_until_ms_ = 0;
    float last_diff_percent_ = 0.0f;
    bool has_diff_ = false;
    bool motor_override_active_ = false;
    bool motor_override_enabled_ = true;
    bool motor_enabled_last_ = true;
};
