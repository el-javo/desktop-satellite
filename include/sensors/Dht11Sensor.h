#pragma once

#include <Arduino.h>
#include <DHT.h>

class Dht11Sensor {
public:
    struct Config {
        int pin;
        unsigned long report_interval_ms;
        unsigned int samples_per_report;
        int dht_type;
    };

    struct Sample {
        float temperature_c = 0.0f;
        float humidity_pct = 0.0f;
    };

    explicit Dht11Sensor(const Config& cfg)
        : cfg_(cfg), dht_(cfg.pin, cfg.dht_type) {}

    void begin() { dht_.begin(); }

    void tick(unsigned long now_ms) {
        const unsigned long sample_interval_ms = sampleIntervalMs();
        if (now_ms - last_sample_ms_ < sample_interval_ms) {
            return;
        }
        last_sample_ms_ = now_ms;

        const float t = dht_.readTemperature();
        const float h = dht_.readHumidity();
        if (isnan(t) || isnan(h)) {
            return;
        }

        sum_temp_ += t;
        sum_hum_ += h;
        sample_count_++;

        if (sample_count_ >= samplesPerReportSafe()) {
            last_sample_.temperature_c = sum_temp_ / (float)sample_count_;
            last_sample_.humidity_pct = sum_hum_ / (float)sample_count_;

            sum_temp_ = 0.0f;
            sum_hum_ = 0.0f;
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
    unsigned int samplesPerReportSafe() const {
        return (cfg_.samples_per_report > 0) ? cfg_.samples_per_report : 1U;
    }

    unsigned long sampleIntervalMs() const {
        const unsigned int samples = samplesPerReportSafe();
        if (samples <= 1) {
            return cfg_.report_interval_ms;
        }
        const unsigned long interval = cfg_.report_interval_ms / samples;
        return (interval > 0) ? interval : 1UL;
    }

    Config cfg_;
    DHT dht_;
    unsigned long last_sample_ms_ = 0;
    float sum_temp_ = 0.0f;
    float sum_hum_ = 0.0f;
    unsigned int sample_count_ = 0;
    bool new_sample_ = false;
    Sample last_sample_;
};
