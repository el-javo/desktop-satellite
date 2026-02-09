#pragma once

#include <Arduino.h>

class MotorDriver {
public:
    struct Config {
        int in1_pin;
        int in2_pin;
        int pwm_freq;
        int pwm_res_bits;
        int pwm_channel_in1;
        int pwm_channel_in2;
        float smooth; // 0..1
        unsigned long update_interval_ms;
        float kick_norm; // 0..1
        unsigned long kick_duration_ms;
    };

    explicit MotorDriver(const Config& cfg)
        : cfg_(cfg) {
        pwm_range_ = (1UL << cfg_.pwm_res_bits) - 1UL;
    }

    void begin() {
        ledcSetup(cfg_.pwm_channel_in1, cfg_.pwm_freq, cfg_.pwm_res_bits);
        ledcSetup(cfg_.pwm_channel_in2, cfg_.pwm_freq, cfg_.pwm_res_bits);
        if (cfg_.in1_pin >= 0) {
            ledcAttachPin(cfg_.in1_pin, cfg_.pwm_channel_in1);
            has_in1_ = true;
            ledcWrite(cfg_.pwm_channel_in1, 0);
        }
        if (cfg_.in2_pin >= 0) {
            ledcAttachPin(cfg_.in2_pin, cfg_.pwm_channel_in2);
            has_in2_ = true;
            ledcWrite(cfg_.pwm_channel_in2, 0);
        }
    }

    void setTargetNormalized(float signed_norm) {
        const float next = constrain(signed_norm, -1.0f, 1.0f);
        if (next == 0.0f) {
            target_norm_ = 0.0f;
            last_target_sign_ = 0;
            return;
        }

        const int next_sign = (next > 0.0f) ? 1 : -1;
        if (last_target_sign_ != 0 && next_sign != last_target_sign_) {
            kick_pending_ = true;
        } else if (target_norm_ == 0.0f) {
            kick_pending_ = true;
        }

        last_target_sign_ = next_sign;
        target_norm_ = next;
    }

    void tick(unsigned long now_ms) {
        if (cfg_.update_interval_ms > 0 &&
            (now_ms - last_update_ms_) < cfg_.update_interval_ms) {
            return;
        }
        last_update_ms_ = now_ms;

        if (kick_pending_ && target_norm_ != 0.0f) {
            kick_active_until_ms_ = now_ms + cfg_.kick_duration_ms;
            kick_pending_ = false;
        }

        const float smooth = constrain(cfg_.smooth, 0.0f, 1.0f);
        const float alpha = 1.0f - smooth;
        filtered_norm_ += (target_norm_ - filtered_norm_) * alpha;

        float applied_norm = filtered_norm_;
        if (cfg_.kick_duration_ms > 0 && now_ms < kick_active_until_ms_) {
            const float kick = constrain(cfg_.kick_norm, 0.0f, 1.0f);
            if (kick > 0.0f) {
                const float sign = (target_norm_ >= 0.0f) ? 1.0f : -1.0f;
                applied_norm = sign * max(kick, fabsf(filtered_norm_));
            }
        }

        const float mag = fabsf(applied_norm);
        last_pwm_raw_ = (uint32_t)constrain(
            (int)lroundf(mag * (float)pwm_range_), 0, (int)pwm_range_);

        if (applied_norm > 0.0f) {
            if (has_in1_) {
                ledcWrite(cfg_.pwm_channel_in1, last_pwm_raw_);
            }
            if (has_in2_) {
                ledcWrite(cfg_.pwm_channel_in2, 0);
            }
        } else if (applied_norm < 0.0f) {
            if (has_in1_) {
                ledcWrite(cfg_.pwm_channel_in1, 0);
            }
            if (has_in2_) {
                ledcWrite(cfg_.pwm_channel_in2, last_pwm_raw_);
            }
        } else {
            if (has_in1_) {
                ledcWrite(cfg_.pwm_channel_in1, 0);
            }
            if (has_in2_) {
                ledcWrite(cfg_.pwm_channel_in2, 0);
            }
        }

        last_applied_norm_ = applied_norm;
    }

    uint32_t normToRaw(float norm) const {
        const float n = constrain(norm, 0.0f, 1.0f);
        return (uint32_t)lroundf(n * (float)pwm_range_);
    }

    float getFilteredNorm() const { return filtered_norm_; }
    float getAppliedNorm() const { return last_applied_norm_; }
    uint32_t getAppliedPwmRaw() const { return last_pwm_raw_; }

private:
    Config cfg_;
    unsigned long last_update_ms_ = 0;
    uint32_t pwm_range_ = 255;
    float target_norm_ = 0.0f;
    float filtered_norm_ = 0.0f;
    float last_applied_norm_ = 0.0f;
    uint32_t last_pwm_raw_ = 0;
    bool kick_pending_ = false;
    unsigned long kick_active_until_ms_ = 0;
    int last_target_sign_ = 0;
    bool has_in1_ = false;
    bool has_in2_ = false;
};
