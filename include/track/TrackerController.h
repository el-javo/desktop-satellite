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
        uint32_t low_light_level_1;
        float low_light_deadband_1_percent;
        uint32_t low_light_level_2;
        float low_light_deadband_2_percent;
        uint32_t low_light_level_3;
        float low_light_deadband_3_percent;
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
        const float db = effectiveDeadband(sample);
        const float pwm_threshold = fabsf(cfg_.diff_pwm_threshold);
        const bool move_pos = diff > db;
        const bool move_neg = diff < -db;

        float target_norm = 0.0f;
        if (move_pos || move_neg) {
            const float pwm_min = constrain(cfg_.pwm_min_norm, 0.0f, 1.0f);
            const float pwm_max = constrain(cfg_.pwm_max_norm, 0.0f, 1.0f);
            const float pwm_low = min(pwm_min, pwm_max);
            const float pwm_high = max(pwm_min, pwm_max);
            const float pwm_choice =
                (diff_abs >= pwm_threshold) ? pwm_high : pwm_low;
            target_norm = (move_pos ? pwm_choice : -pwm_choice);
        }

        last_target_norm_ = target_norm;
        motor_.setTargetNormalized(target_norm);
    }

    bool hasNewSample() const { return new_sample_; }
    void clearNewSample() { new_sample_ = false; }
    LightSensorPair::Sample lastSample() const { return last_sample_; }
    float lastTargetNorm() const { return last_target_norm_; }
    float deadband() const { return fabsf(cfg_.diff_deadband); }
    float lastEffectiveDeadband() const { return last_effective_deadband_; }

private:
    float effectiveDeadband(const LightSensorPair::Sample& sample) {
        float db = fabsf(cfg_.diff_deadband);
        const uint32_t max_signal = max(sample.avg_a, sample.avg_b);

        if (cfg_.low_light_level_3 > 0 && max_signal < cfg_.low_light_level_3) {
            db = max(db, fabsf(cfg_.low_light_deadband_3_percent));
        } else if (cfg_.low_light_level_2 > 0 && max_signal < cfg_.low_light_level_2) {
            db = max(db, fabsf(cfg_.low_light_deadband_2_percent));
        } else if (cfg_.low_light_level_1 > 0 && max_signal < cfg_.low_light_level_1) {
            db = max(db, fabsf(cfg_.low_light_deadband_1_percent));
        }

        last_effective_deadband_ = db;
        return db;
    }

    Config cfg_;
    LightSensorPair& sensors_;
    MotorDriver& motor_;
    LightSensorPair::Sample last_sample_;
    bool new_sample_ = false;
    float last_target_norm_ = 0.0f;
    float last_effective_deadband_ = 0.0f;
};
