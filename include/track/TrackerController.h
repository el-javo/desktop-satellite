#pragma once

#include <Arduino.h>

#include "sensors/LightSensorPair.h"
#include "drivers/MotorDriver.h"

class TrackerController {
public:
    struct Config {
        float diff_deadband;
        float diff_pwm_threshold;
        float pwm_min_norm;
        float pwm_max_norm;
    };

    TrackerController(const Config& cfg, LightSensorPair& sensors, MotorDriver& motor)
        : cfg_(cfg), sensors_(sensors), motor_(motor) {}

    void tick() {
        LightSensorPair::Sample sample;
        if (!sensors_.consumeSample(sample)) {
            return;
        }

        last_sample_ = sample;
        new_sample_ = true;

        const float diff = sample.diff_percent;
        const float diff_abs = fabsf(diff);
        const float db = fabsf(cfg_.diff_deadband);
        const bool move_pos = diff > db;
        const bool move_neg = diff < -db;

        float target_norm = 0.0f;
        if (move_pos || move_neg) {
            const float pwm_min = constrain(cfg_.pwm_min_norm, 0.0f, 1.0f);
            const float pwm_max = constrain(cfg_.pwm_max_norm, 0.0f, 1.0f);
            const float pwm_low = min(pwm_min, pwm_max);
            const float pwm_high = max(pwm_min, pwm_max);
            const float pwm_choice =
                (diff_abs >= cfg_.diff_pwm_threshold) ? pwm_high : pwm_low;
            target_norm = (move_pos ? pwm_choice : -pwm_choice);
        }

        last_target_norm_ = target_norm;
        motor_.setTargetNormalized(target_norm);
    }

    bool hasNewSample() const { return new_sample_; }
    void clearNewSample() { new_sample_ = false; }
    LightSensorPair::Sample lastSample() const { return last_sample_; }
    float lastTargetNorm() const { return last_target_norm_; }

private:
    Config cfg_;
    LightSensorPair& sensors_;
    MotorDriver& motor_;
    LightSensorPair::Sample last_sample_;
    bool new_sample_ = false;
    float last_target_norm_ = 0.0f;
};
