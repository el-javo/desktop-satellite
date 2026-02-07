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
        motor_.tick(now_ms);
    }

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
    LightSensorPair sensors_;
    MotorDriver motor_;
    TrackerController tracker_;
};
