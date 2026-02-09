#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
public:
    enum class Mode {
        Off,
        Connecting,
        Tracking,
        Dashboard
    };

    struct Config {
        int pin_dc;
        int pin_rst;
        int pin_blk;
        bool blk_active_high;
        unsigned long refresh_interval_ms;
    };

    explicit DisplayManager(const Config& cfg)
        : cfg_(cfg) {}

    void begin() {
        if (cfg_.pin_blk >= 0) {
            pinMode(cfg_.pin_blk, OUTPUT);
            setBacklight(true);
        }

        tft_.init();
        tft_.setRotation(0);
        tft_.fillScreen(TFT_BLACK);

        mode_ = Mode::Off;
        last_draw_ms_ = 0;
        force_full_redraw_ = true;
    }

    void setMode(Mode mode) {
        if (mode_ != mode) {
            mode_ = mode;
            dirty_ = true;
            force_full_redraw_ = true;
        }
    }

    void setTrackingInfoH(float diff_percent) {
        diff_h_percent_ = diff_percent;
        dirty_ = true;
    }

    void setTrackingInfoHV(float diff_h, float diff_v) {
        diff_h_percent_ = diff_h;
        diff_v_percent_ = diff_v;
        dirty_ = true;
    }

    void setDeadbandPercent(float deadband_percent) {
        deadband_percent_ = deadband_percent;
        dirty_ = true;
    }

    void setPwmThresholdPercent(float pwm_threshold_percent) {
        pwm_threshold_percent_ = pwm_threshold_percent;
        dirty_ = true;
    }

    void setEnvironment(float temp_c, float humidity_pct) {
        temp_c_ = temp_c;
        humidity_pct_ = humidity_pct;
        dirty_ = true;
    }

    void setBatteryPercent(float percent) {
        battery_percent_ = constrain(percent, 0.0f, 100.0f);
        dirty_ = true;
    }

    void setBacklight(bool on) {
        if (cfg_.pin_blk >= 0) {
            const bool level = cfg_.blk_active_high ? on : !on;
            digitalWrite(cfg_.pin_blk, level ? HIGH : LOW);
        }
    }

    void setBlocked(bool blocked) {
        blocked_ = blocked;
        dirty_ = true;
    }

    void setActiveIndicator(bool active) {
        active_ = active;
        dirty_ = true;
    }

    void tick(unsigned long now_ms) {
        if (!dirty_ && (now_ms - last_draw_ms_) < cfg_.refresh_interval_ms) {
            return;
        }
        last_draw_ms_ = now_ms;
        dirty_ = false;

        switch (mode_) {
        case Mode::Off:
            drawOff();
            break;
        case Mode::Connecting:
            drawConnecting();
            break;
        case Mode::Tracking:
            drawTracking();
            break;
        case Mode::Dashboard:
            drawDashboard();
            break;
        }
    }

private:
    void drawHeader(const char* title) {
        tft_.fillRect(0, 0, 240, 28, TFT_BLUE);
        tft_.setTextColor(TFT_YELLOW, TFT_BLUE);
        tft_.setTextDatum(TL_DATUM);
        if (tft_.drawString(title, 8, 6, 2) == 0) {
            tft_.drawString(title, 8, 8, 1);
        }
    }

    void drawOff() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        // No header
    }

    void drawConnecting() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        // No header
        tft_.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft_.setTextSize(2);
        tft_.setCursor(20, 80);
        tft_.print("Connecting...");
        force_full_redraw_ = false;
    }

    void drawDashboard() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        // No header
        drawEnvBlock(10, 40);
        force_full_redraw_ = false;
    }

    void drawTracking() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        drawBatteryIndicator(20, 6, 200, 12);
        drawDiffGaugeCircle(120, 120, 70);
        drawActiveIndicator(200, 88);
        drawBlockedIndicator(200, 110);
        drawEnvBlock(10, 200);
        force_full_redraw_ = false;
    }

    void drawBatteryIndicator(int x, int y, int w, int h) {
        if (!force_full_redraw_ && battery_percent_ == last_battery_percent_) {
            return;
        }

        const uint16_t bg = TFT_BLACK;
        const uint16_t frame = tft_.color565(210, 220, 220);
        const uint16_t off = tft_.color565(30, 30, 30);

        uint16_t fill = tft_.color565(255, 60, 60);
        if (battery_percent_ >= 70.0f) {
            fill = tft_.color565(0, 220, 120);
        } else if (battery_percent_ >= 25.0f) {
            fill = tft_.color565(230, 200, 60);
        }

        tft_.fillRect(x, y, w, h, bg);

        const int segments = 20;
        const int gap = 1;
        const int inner_w = w - 2;
        const int inner_h = h - 2;
        const int pad_y = 1;
        const int total_gap = (segments - 1) * gap;
        const int seg_w = (inner_w - total_gap) / segments;
        const int total_w = (seg_w * segments) + total_gap;
        const int frame_w = total_w + 2;
        const int frame_x = x + ((w - frame_w) / 2);

        tft_.drawRect(frame_x, y, frame_w, h, frame);
        int filled = (int)floorf((battery_percent_ / 100.0f) * (float)segments);
        filled = constrain(filled, 0, segments);

        int sx = frame_x + 1;
        const int sy = y + 1 + pad_y;
        const int seg_h = inner_h - (2 * pad_y);
        for (int i = 0; i < segments; ++i) {
            const uint16_t color = (i < filled) ? fill : off;
            tft_.fillRect(sx, sy, seg_w, seg_h, color);
            sx += seg_w + gap;
        }

        last_battery_percent_ = battery_percent_;
    }

    void drawActiveIndicator(int x, int y) {
        if (!force_full_redraw_ && active_ == last_active_) {
            return;
        }

        const int size = 18;
        const uint16_t bg = tft_.color565(20, 20, 20);
        const uint16_t off_color = tft_.color565(40, 40, 40);
        const uint16_t on_color = TFT_GREEN;

        tft_.fillRect(x, y, size, size, bg);
        tft_.drawRect(x, y, size, size, active_ ? on_color : off_color);
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(active_ ? on_color : off_color, bg);
        tft_.drawString("A", x + size / 2, y + size / 2, 1);
        tft_.setTextDatum(TL_DATUM);

        last_active_ = active_;
    }

    void drawBlockedIndicator(int x, int y) {
        if (!force_full_redraw_ && blocked_ == last_blocked_) {
            return;
        }

        const int size = 18;
        const uint16_t bg = tft_.color565(20, 20, 20);
        const uint16_t off_color = tft_.color565(60, 60, 60);
        const uint16_t on_color = TFT_RED;

        tft_.fillRect(x, y, size, size, bg);
        tft_.drawRect(x, y, size, size, blocked_ ? on_color : off_color);
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(blocked_ ? on_color : off_color, bg);
        tft_.drawString("B", x + size / 2, y + size / 2, 1);
        tft_.setTextDatum(TL_DATUM);

        last_blocked_ = blocked_;
    }

    void drawEnvBlock(int x, int y) {
        if (!force_full_redraw_ &&
            temp_c_ == last_temp_c_ &&
            humidity_pct_ == last_humidity_pct_) {
            return;
        }

        const int box_w = 220;
        const int box_h = 36;
        tft_.fillRect(x, y, box_w, box_h, TFT_BLACK);
        tft_.setTextColor(TFT_WHITE, TFT_BLACK);
        tft_.setTextDatum(TL_DATUM);

        tft_.setTextSize(1);
        const int use_font = 2;
        const int pad_x = 8;
        const int pad_y = 8;

        char line[40];
        snprintf(line, sizeof(line), "T: %.1fC  H: %.1f%%", temp_c_, humidity_pct_);
        tft_.drawString(line, x + pad_x, y + pad_y, use_font);

        tft_.setTextDatum(TL_DATUM);
        last_temp_c_ = temp_c_;
        last_humidity_pct_ = humidity_pct_;
    }

    void drawDiffGaugeCircle(int cx, int cy, int r) {
        if (!force_full_redraw_ &&
            diff_h_percent_ == last_diff_h_percent_ &&
            diff_v_percent_ == last_diff_v_percent_ &&
            deadband_percent_ == last_deadband_percent_ &&
            pwm_threshold_percent_ == last_pwm_threshold_percent_) {
            return;
        }

        const uint16_t dark_green = tft_.color565(0, 40, 0);
        const uint16_t dark_blue = tft_.color565(0, 0, 60);
        const uint16_t dark_red = tft_.color565(60, 0, 0);

        const float diff_abs_full = max(fabsf(diff_h_percent_), fabsf(diff_v_percent_));
        const float deadband_for_region = fabsf(deadband_percent_);
        const float pwm_for_region = fabsf(pwm_threshold_percent_);
        const float deadband_th = min(deadband_for_region, pwm_for_region);
        const float pwm_th = max(deadband_for_region, pwm_for_region);

        uint16_t region_bg = dark_red;
        uint16_t region_fg = TFT_RED;
        if (diff_abs_full <= deadband_th) {
            region_bg = dark_green;
            region_fg = TFT_GREEN;
        } else if (diff_abs_full <= pwm_th) {
            region_bg = dark_blue;
            region_fg = TFT_BLUE;
        }

        tft_.fillCircle(cx, cy, r, region_bg);
        tft_.drawCircle(cx, cy, r, TFT_WHITE);

        const float gauge_min = -35.0f;
        const float gauge_max = 35.0f;
        const float gauge_span = gauge_max - gauge_min;

        const int deadband_r = (int)((fabsf(deadband_percent_) / gauge_span) * (2.0f * r));
        const int pwm_r = (int)((fabsf(pwm_threshold_percent_) / gauge_span) * (2.0f * r));

        if (deadband_r > 0) {
            tft_.drawCircle(cx, cy, deadband_r, TFT_GREEN);
        }
        if (pwm_r > 0) {
            tft_.drawCircle(cx, cy, pwm_r, TFT_BLUE);
        }

        // Crosshair
        const uint16_t dark_grey = tft_.color565(40, 40, 40);
        tft_.drawLine(cx - r, cy, cx + r, cy, dark_grey);
        tft_.drawLine(cx, cy - r, cx, cy + r, dark_grey);

        // Marker based on H/V diffs, clamped to circle
        float h_norm = diff_h_percent_ / gauge_max;
        float v_norm = diff_v_percent_ / gauge_max;
        h_norm = constrain(h_norm, -1.0f, 1.0f);
        v_norm = constrain(v_norm, -1.0f, 1.0f);
        const float len = sqrtf((h_norm * h_norm) + (v_norm * v_norm));
        const float scale = (len > 1.0f) ? (1.0f / len) : 1.0f;
        const int px = cx + (int)(h_norm * scale * (float)r);
        const int py = cy - (int)(v_norm * scale * (float)r);
        const bool saturated = len > 1.0f;
        const bool in_deadband = diff_abs_full <= deadband_th;
        const uint16_t dot_color = in_deadband
            ? TFT_GREEN
            : (saturated ? TFT_RED : TFT_WHITE);
        tft_.fillCircle(px, py, 4, dot_color);

        // Diff label above gauge
        char diff_label[28];
        snprintf(diff_label, sizeof(diff_label), "DIFF H/V: %.1f %.1f", diff_h_percent_, diff_v_percent_);
        tft_.setTextColor(region_fg, TFT_BLACK);
        tft_.setTextDatum(MC_DATUM);
        tft_.drawString(diff_label, cx, cy - r - 10, 1);
        tft_.setTextDatum(TL_DATUM);

        last_diff_h_percent_ = diff_h_percent_;
        last_diff_v_percent_ = diff_v_percent_;
        last_deadband_percent_ = deadband_percent_;
        last_pwm_threshold_percent_ = pwm_threshold_percent_;
    }

    Config cfg_;
    TFT_eSPI tft_;
    Mode mode_ = Mode::Off;
    bool dirty_ = true;
    bool force_full_redraw_ = true;
    unsigned long last_draw_ms_ = 0;
    float temp_c_ = 0.0f;
    float humidity_pct_ = 0.0f;
    float diff_h_percent_ = 0.0f;
    float diff_v_percent_ = 0.0f;
    float deadband_percent_ = 1.0f;
    float pwm_threshold_percent_ = 10.0f;
    float last_temp_c_ = 9999.0f;
    float last_humidity_pct_ = 9999.0f;
    float last_diff_h_percent_ = 9999.0f;
    float last_diff_v_percent_ = 9999.0f;
    float last_deadband_percent_ = 9999.0f;
    float last_pwm_threshold_percent_ = 9999.0f;
    bool blocked_ = false;
    bool last_blocked_ = false;
    bool active_ = false;
    bool last_active_ = false;
    float battery_percent_ = 60.0f;
    float last_battery_percent_ = -1.0f;
};
