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
    };

    explicit MotorDriver(const Config& cfg)
        : cfg_(cfg) {
        pwm_range_ = (1UL << cfg_.pwm_res_bits) - 1UL;
    }

    void begin() {
        ledcSetup(cfg_.pwm_channel_in1, cfg_.pwm_freq, cfg_.pwm_res_bits);
        ledcSetup(cfg_.pwm_channel_in2, cfg_.pwm_freq, cfg_.pwm_res_bits);
        ledcAttachPin(cfg_.in1_pin, cfg_.pwm_channel_in1);
        ledcAttachPin(cfg_.in2_pin, cfg_.pwm_channel_in2);
        ledcWrite(cfg_.pwm_channel_in1, 0);
        ledcWrite(cfg_.pwm_channel_in2, 0);
    }

    void setTargetNormalized(float signed_norm) {
        target_norm_ = constrain(signed_norm, -1.0f, 1.0f);
    }

    void tick(unsigned long now_ms) {
        if (cfg_.update_interval_ms > 0 &&
            (now_ms - last_update_ms_) < cfg_.update_interval_ms) {
            return;
        }
        last_update_ms_ = now_ms;

        const float smooth = constrain(cfg_.smooth, 0.0f, 1.0f);
        const float alpha = 1.0f - smooth;
        filtered_norm_ += (target_norm_ - filtered_norm_) * alpha;

        const float mag = fabsf(filtered_norm_);
        last_pwm_raw_ = (uint32_t)constrain(
            (int)lroundf(mag * (float)pwm_range_), 0, (int)pwm_range_);

        if (filtered_norm_ > 0.0f) {
            ledcWrite(cfg_.pwm_channel_in1, last_pwm_raw_);
            ledcWrite(cfg_.pwm_channel_in2, 0);
        } else if (filtered_norm_ < 0.0f) {
            ledcWrite(cfg_.pwm_channel_in1, 0);
            ledcWrite(cfg_.pwm_channel_in2, last_pwm_raw_);
        } else {
            ledcWrite(cfg_.pwm_channel_in1, 0);
            ledcWrite(cfg_.pwm_channel_in2, 0);
        }
    }

    uint32_t normToRaw(float norm) const {
        const float n = constrain(norm, 0.0f, 1.0f);
        return (uint32_t)lroundf(n * (float)pwm_range_);
    }

    float getFilteredNorm() const { return filtered_norm_; }
    uint32_t getAppliedPwmRaw() const { return last_pwm_raw_; }

private:
    Config cfg_;
    unsigned long last_update_ms_ = 0;
    uint32_t pwm_range_ = 255;
    float target_norm_ = 0.0f;
    float filtered_norm_ = 0.0f;
    uint32_t last_pwm_raw_ = 0;
};
