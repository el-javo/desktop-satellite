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

        if (target_override_active_) {
            motor_.setTargetNormalized(target_override_norm_);
        }

        bool motor_enabled = true;
        if (motor_override_active_) {
            motor_enabled = motor_override_enabled_;
        }
        motor_enabled_last_ = motor_enabled;
        motor_.setEnabled(motor_enabled);
        motor_.tick(now_ms);
    }

    void setMotorOverride(bool enabled) {
        motor_override_active_ = true;
        motor_override_enabled_ = enabled;
    }

    void clearMotorOverride() { motor_override_active_ = false; }

    void setTargetOverride(float signed_norm) {
        target_override_active_ = true;
        target_override_norm_ = constrain(signed_norm, -1.0f, 1.0f);
    }

    void clearTargetOverride() { target_override_active_ = false; }

    bool isMotorEnabled() const { return motor_enabled_last_; }
    bool hasDiffSample() const { return has_diff_; }
    float lastDiffPercent() const { return last_diff_percent_; }
    float lastEffectiveDeadband() const { return tracker_.lastEffectiveDeadband(); }

    bool consumeLog(LogSample& out) {
        if (!tracker_.hasNewSample()) {
            return false;
        }
        const LightSensorPair::Sample s = tracker_.lastSample();
        out.avg_a = s.avg_a;
        out.avg_b = s.avg_b;
        out.diff_percent = s.diff_percent;
        out.target_norm = tracker_.lastTargetNorm();
        out.applied_norm = motor_.getAppliedNorm();
        out.applied_raw = motor_.getAppliedPwmRaw();
        tracker_.clearNewSample();
        return true;
    }

private:
    LightSensorPair sensors_;
    MotorDriver motor_;
    TrackerController tracker_;
    float last_diff_percent_ = 0.0f;
    bool has_diff_ = false;
    bool motor_override_active_ = false;
    bool motor_override_enabled_ = true;
    bool target_override_active_ = false;
    float target_override_norm_ = 0.0f;
    bool motor_enabled_last_ = true;
};
