#pragma once

#include <Arduino.h>

class TouchButton {
public:
    struct Config {
        int pin;
        bool active_high;
        unsigned long debounce_ms;
        unsigned long long_press_ms;
    };

    explicit TouchButton(const Config& cfg)
        : cfg_(cfg) {}

    void begin() {
        pinMode(cfg_.pin, INPUT);
        last_raw_pressed_ = readPressed();
        stable_pressed_ = last_raw_pressed_;
        last_change_ms_ = millis();
    }

    void tick(unsigned long now_ms) {
        const bool raw_pressed = readPressed();
        if (raw_pressed != last_raw_pressed_) {
            last_raw_pressed_ = raw_pressed;
            last_change_ms_ = now_ms;
        }

        if (now_ms - last_change_ms_ >= cfg_.debounce_ms) {
            if (stable_pressed_ != raw_pressed) {
                stable_pressed_ = raw_pressed;
                if (stable_pressed_) {
                    press_start_ms_ = now_ms;
                    long_fired_ = false;
                } else {
                    if (!long_fired_) {
                        short_pending_ = true;
                    }
                }
            }
        }

        if (stable_pressed_ && !long_fired_ &&
            (now_ms - press_start_ms_) >= cfg_.long_press_ms) {
            long_fired_ = true;
            long_pending_ = true;
        }
    }

    bool consumeShortPress() {
        if (!short_pending_) {
            return false;
        }
        short_pending_ = false;
        return true;
    }

    bool consumeLongPress() {
        if (!long_pending_) {
            return false;
        }
        long_pending_ = false;
        return true;
    }

    bool isPressed() const { return stable_pressed_; }

private:
    bool readPressed() const {
        const bool raw = (digitalRead(cfg_.pin) != 0);
        return cfg_.active_high ? raw : !raw;
    }

    Config cfg_;
    bool last_raw_pressed_ = false;
    bool stable_pressed_ = false;
    unsigned long last_change_ms_ = 0;
    unsigned long press_start_ms_ = 0;
    bool long_fired_ = false;
    bool short_pending_ = false;
    bool long_pending_ = false;
};
