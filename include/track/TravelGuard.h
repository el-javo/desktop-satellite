#pragma once

#include <Arduino.h>

class TravelGuard {
public:
    struct Config {
        int limit_pin_1;
        int limit_pin_2;
        bool active_high;
        bool use_pullup;
        unsigned long debounce_ms;
        float sweep_norm;
        int dir_from_limit_1; // +1 or -1
        int dir_from_limit_2; // +1 or -1
    };

    explicit TravelGuard(const Config& cfg)
        : cfg_(cfg) {}

    void begin() {
        const uint8_t mode = cfg_.use_pullup ? INPUT_PULLUP : INPUT;
        if (cfg_.limit_pin_1 >= 0) {
            pinMode(cfg_.limit_pin_1, mode);
        }
        if (cfg_.limit_pin_2 >= 0) {
            pinMode(cfg_.limit_pin_2, mode);
        }

        limit_1_.stable = readPressedRaw(cfg_.limit_pin_1);
        limit_1_.last_raw = limit_1_.stable;
        limit_2_.stable = readPressedRaw(cfg_.limit_pin_2);
        limit_2_.last_raw = limit_2_.stable;
    }

    void tick(unsigned long now_ms) {
        const bool raw_1 = readPressedRaw(cfg_.limit_pin_1);
        const bool raw_2 = readPressedRaw(cfg_.limit_pin_2);
        updateSwitch(limit_1_, raw_1, now_ms);
        updateSwitch(limit_2_, raw_2, now_ms);

        const bool edge_1 = consumePressedEdge(limit_1_);
        const bool edge_2 = consumePressedEdge(limit_2_);

        if (state_ == SweepState::Idle) {
            if (edge_1) {
                state_ = SweepState::ToLimit2;
            } else if (edge_2) {
                state_ = SweepState::ToLimit1;
            }
            return;
        }

        if (state_ == SweepState::ToLimit2) {
            // Stop sweep only when destination limit is stable (debounced).
            if (limit_2_.stable) {
                state_ = SweepState::Idle;
            }
            return;
        }

        if (state_ == SweepState::ToLimit1) {
            // Stop sweep only when destination limit is stable (debounced).
            if (limit_1_.stable) {
                state_ = SweepState::Idle;
            }
            return;
        }
    }

    bool isSweepActive() const { return state_ != SweepState::Idle; }

    float sweepTargetNorm() const {
        const float mag = constrain(fabsf(cfg_.sweep_norm), 0.0f, 1.0f);
        if (state_ == SweepState::ToLimit2) {
            return (cfg_.dir_from_limit_1 >= 0) ? mag : -mag;
        }
        if (state_ == SweepState::ToLimit1) {
            return (cfg_.dir_from_limit_2 >= 0) ? mag : -mag;
        }
        return 0.0f;
    }

    bool isLimit1Pressed() const { return limit_1_.stable; }
    bool isLimit2Pressed() const { return limit_2_.stable; }

private:
    enum class SweepState {
        Idle,
        ToLimit1,
        ToLimit2
    };

    struct SwitchState {
        bool stable = false;
        bool last_raw = false;
        bool pressed_edge = false;
        unsigned long last_change_ms = 0;
    };

    bool readPressedRaw(int pin) const {
        if (pin < 0) {
            return false;
        }
        const bool raw = (digitalRead(pin) != 0);
        return cfg_.active_high ? raw : !raw;
    }

    void updateSwitch(SwitchState& sw, bool raw_pressed, unsigned long now_ms) {
        if (raw_pressed != sw.last_raw) {
            sw.last_raw = raw_pressed;
            sw.last_change_ms = now_ms;
        }

        if ((now_ms - sw.last_change_ms) < cfg_.debounce_ms) {
            return;
        }

        if (sw.stable != sw.last_raw) {
            const bool previous = sw.stable;
            sw.stable = sw.last_raw;
            if (!previous && sw.stable) {
                sw.pressed_edge = true;
            }
        }
    }

    bool consumePressedEdge(SwitchState& sw) {
        const bool edge = sw.pressed_edge;
        sw.pressed_edge = false;
        return edge;
    }

    Config cfg_;
    SwitchState limit_1_;
    SwitchState limit_2_;
    SweepState state_ = SweepState::Idle;
};
