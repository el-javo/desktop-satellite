#pragma once

#include <Arduino.h>

#include "track/TrackingUnit.h"

class TrackingCoordinator {
public:
    struct Config {
        float deadband_h;
        float deadband_v;
        unsigned long deadband_hold_ms;
        unsigned long block_duration_ms;
    };

    TrackingCoordinator(const Config& cfg, TrackingUnit& unit_h, TrackingUnit& unit_v)
        : cfg_(cfg), unit_h_(unit_h), unit_v_(unit_v) {}

    void setEnabled(bool enabled) {
        if (enabled_ == enabled) {
            return;
        }
        enabled_ = enabled;
        resetState();
    }

    void resetState() {
        deadband_enter_ms_ = 0;
        block_until_ms_ = 0;
        blocked_ = false;
        in_deadband_ = false;
        has_both_diffs_ = false;
    }

    void tick(unsigned long now_ms) {
        if (!enabled_) {
            return;
        }

        has_both_diffs_ = unit_h_.hasDiffSample() && unit_v_.hasDiffSample();
        if (!has_both_diffs_) {
            blocked_ = false;
            in_deadband_ = false;
            deadband_enter_ms_ = 0;
            block_until_ms_ = 0;
            applyMotorState(true);
            return;
        }

        const float diff_h_abs = fabsf(unit_h_.lastDiffPercent());
        const float diff_v_abs = fabsf(unit_v_.lastDiffPercent());
        const float db_h = fabsf(cfg_.deadband_h);
        const float db_v = fabsf(cfg_.deadband_v);
        in_deadband_ = (diff_h_abs <= db_h) && (diff_v_abs <= db_v);

        const bool is_blocked_window = now_ms < block_until_ms_;
        if (is_blocked_window) {
            blocked_ = true;
            applyMotorState(false);
            return;
        }

        if (block_until_ms_ > 0 && in_deadband_) {
            block_until_ms_ = now_ms + cfg_.block_duration_ms;
            blocked_ = true;
            applyMotorState(false);
            return;
        }

        if (!in_deadband_) {
            deadband_enter_ms_ = 0;
            block_until_ms_ = 0;
            blocked_ = false;
            applyMotorState(true);
            return;
        }

        if (deadband_enter_ms_ == 0) {
            deadband_enter_ms_ = now_ms;
        }
        if ((now_ms - deadband_enter_ms_) >= cfg_.deadband_hold_ms) {
            block_until_ms_ = now_ms + cfg_.block_duration_ms;
            deadband_enter_ms_ = 0;
            blocked_ = true;
            applyMotorState(false);
            return;
        }

        blocked_ = false;
        applyMotorState(true);
    }

    bool isBlocked() const { return blocked_; }
    bool isInDeadband() const { return in_deadband_; }
    bool hasBothDiffs() const { return has_both_diffs_; }

private:
    void applyMotorState(bool enabled) {
        unit_h_.setMotorOverride(enabled);
        unit_v_.setMotorOverride(enabled);
    }

    Config cfg_;
    TrackingUnit& unit_h_;
    TrackingUnit& unit_v_;
    bool enabled_ = false;
    bool blocked_ = false;
    bool in_deadband_ = false;
    bool has_both_diffs_ = false;
    unsigned long deadband_enter_ms_ = 0;
    unsigned long block_until_ms_ = 0;
};

