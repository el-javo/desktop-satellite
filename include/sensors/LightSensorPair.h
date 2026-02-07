#pragma once

#include <Arduino.h>

class LightSensorPair {
public:
    struct Config {
        int pin_a;
        int pin_b;
        unsigned long read_interval_ms;
        unsigned long action_interval_ms;
    };

    struct Sample {
        float diff_percent = 0.0f;
        uint32_t avg_a = 0;
        uint32_t avg_b = 0;
    };

    explicit LightSensorPair(const Config& cfg)
        : cfg_(cfg) {}

    void tick(unsigned long now_ms) {
        if (now_ms - last_read_ms_ < cfg_.read_interval_ms) {
            return;
        }
        last_read_ms_ = now_ms;

        const int value_a = analogRead(cfg_.pin_a);
        const int value_b = analogRead(cfg_.pin_b);

        sum_a_ += (uint32_t)value_a;
        sum_b_ += (uint32_t)value_b;
        sample_count_++;

        const unsigned int samples_per_action =
            (cfg_.read_interval_ms > 0)
                ? (unsigned int)max(1UL, cfg_.action_interval_ms / cfg_.read_interval_ms)
                : 1U;

        if (sample_count_ >= samples_per_action) {
            const uint32_t total_sum = sum_a_ + sum_b_;
            float diff = 0.0f;
            if (total_sum > 0) {
                diff = ((float)((int32_t)sum_a_ - (int32_t)sum_b_) /
                        (float)total_sum) * 100.0f;
            }
            diff = constrain(diff, -100.0f, 100.0f);

            last_sample_.diff_percent = diff;
            last_sample_.avg_a = sum_a_ / sample_count_;
            last_sample_.avg_b = sum_b_ / sample_count_;

            sum_a_ = 0;
            sum_b_ = 0;
            sample_count_ = 0;
            new_sample_ = true;
        }
    }

    bool consumeSample(Sample& out) {
        if (!new_sample_) {
            return false;
        }
        out = last_sample_;
        new_sample_ = false;
        return true;
    }

private:
    Config cfg_;
    unsigned long last_read_ms_ = 0;
    uint32_t sum_a_ = 0;
    uint32_t sum_b_ = 0;
    unsigned int sample_count_ = 0;
    bool new_sample_ = false;
    Sample last_sample_;
};
