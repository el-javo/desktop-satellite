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

    void setTrackingRawH(float avg_a, float avg_b) {
        h_avg_a_ = avg_a;
        h_avg_b_ = avg_b;
        dirty_ = true;
    }

    void setTrackingRawV(float avg_a, float avg_b) {
        v_avg_a_ = avg_a;
        v_avg_b_ = avg_b;
        dirty_ = true;
    }

    void setMotorPwmHV(float pwm_h_norm, float pwm_v_norm) {
        pwm_h_norm_ = constrain(pwm_h_norm, -1.0f, 1.0f);
        pwm_v_norm_ = constrain(pwm_v_norm, -1.0f, 1.0f);
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

    void setSolarChargePercent(float percent) {
        solar_percent_ = constrain(percent, 0.0f, 100.0f);
        dirty_ = true;
    }

    void setSolarCharging(bool charging) {
        solar_charging_ = charging;
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
        last_tick_ms_ = now_ms;
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
    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    uint16_t colBg() const { return rgb565(4, 8, 12); }
    uint16_t colPanel() const { return rgb565(6, 14, 18); }
    uint16_t colAccent() const { return rgb565(0, 220, 200); }
    uint16_t colAccentDim() const { return rgb565(0, 80, 70); }
    uint16_t colText() const { return rgb565(200, 255, 250); }
    uint16_t colTextDim() const { return rgb565(120, 180, 180); }
    uint16_t colWarn() const { return rgb565(255, 60, 60); }
    uint16_t colOk() const { return rgb565(0, 255, 180); }
    uint16_t colMid() const { return rgb565(120, 200, 255); }
    uint16_t colGrid() const { return rgb565(8, 20, 26); }
    uint16_t colLine() const { return rgb565(20, 40, 45); }

    void drawBackdrop() {
        tft_.fillScreen(colBg());
        const uint16_t grid = colGrid();
        for (int y = 0; y < 240; y += 24) {
            tft_.drawFastHLine(0, y, 240, grid);
        }
        for (int x = 0; x < 240; x += 24) {
            tft_.drawFastVLine(x, 0, 240, grid);
        }
        tft_.drawFastHLine(0, 0, 240, colAccentDim());
        tft_.drawFastHLine(0, 239, 240, colAccentDim());
    }

    void drawHeader(const char* title) {
        tft_.fillRect(0, 0, 240, 28, colPanel());
        tft_.drawFastHLine(0, 0, 240, colAccentDim());
        tft_.drawFastHLine(0, 27, 240, colAccent());
        tft_.setTextColor(colText(), colPanel());
        tft_.setTextDatum(TL_DATUM);
        if (tft_.drawString(title, 8, 6, 2) == 0) {
            tft_.drawString(title, 8, 8, 1);
        }
    }

    void drawOff() {
        if (force_full_redraw_) {
            drawBackdrop();
        }
        // No header
    }

    void drawConnecting() {
        if (force_full_redraw_) {
            drawBackdrop();
        }
        // No header
        tft_.setTextColor(colText(), colBg());
        tft_.setTextSize(2);
        tft_.setCursor(20, 80);
        tft_.print("Connecting...");
        force_full_redraw_ = false;
    }

    void drawDashboard() {
        if (force_full_redraw_) {
            drawBackdrop();
        }
        // No header
        drawEnvBlock(10, 40);
        force_full_redraw_ = false;
    }

    void drawTracking() {
        if (force_full_redraw_) {
            drawBackdrop();
        }
        drawBatteryIndicator(20, 6, 200, 12);
        drawSolarIndicator(20, 22, 200, 10);
        drawDiffGaugeCircle(120, 120, 70);
        drawPwmGauges(8, 94, 38, 14);
        drawActiveIndicator(200, 88);
        drawBlockedIndicator(200, 110);
        drawEnvBlock(10, 200);
        force_full_redraw_ = false;
    }

    void drawPwmGauges(int x, int y, int w, int h) {
        drawPwmGauge(x, y, w, h, "H", pwm_h_norm_, last_pwm_h_norm_);
        drawPwmGauge(x, y + h + 6, w, h, "V", pwm_v_norm_, last_pwm_v_norm_);
    }

    void drawPwmGauge(int x,
                      int y,
                      int w,
                      int h,
                      const char* axis_label,
                      float pwm_norm,
                      float& last_pwm_norm) {
        if (!force_full_redraw_ && pwm_norm == last_pwm_norm) {
            return;
        }

        const uint16_t bg = colBg();
        const uint16_t frame = colAccentDim();
        const uint16_t center = colLine();
        const uint16_t fill = (pwm_norm > 0.0f) ? colMid() : ((pwm_norm < 0.0f) ? colWarn() : colTextDim());

        tft_.fillRect(x, y, w, h, bg);
        tft_.drawRect(x, y, w, h, frame);

        const int mid_x = x + (w / 2);
        tft_.drawFastVLine(mid_x, y + 1, h - 2, center);

        const int inner_margin = 2;
        const int half_span = (w / 2) - inner_margin;
        int bar = (int)lroundf(fabsf(pwm_norm) * (float)half_span);
        bar = constrain(bar, 0, half_span);
        if (bar > 0) {
            if (pwm_norm > 0.0f) {
                tft_.fillRect(mid_x + 1, y + 2, bar, h - 4, fill);
            } else {
                tft_.fillRect(mid_x - bar, y + 2, bar, h - 4, fill);
            }
        }

        tft_.setTextColor(colTextDim(), bg);
        tft_.setTextDatum(MR_DATUM);
        tft_.drawString(axis_label, x - 2, y + (h / 2), 1);
        tft_.setTextDatum(TL_DATUM);

        last_pwm_norm = pwm_norm;
    }

    void drawBatteryIndicator(int x, int y, int w, int h) {
        if (!force_full_redraw_ && battery_percent_ == last_battery_percent_) {
            return;
        }

        const uint16_t bg = colBg();
        const uint16_t frame = colAccentDim();
        const uint16_t off = tft_.color565(20, 24, 28);

        uint16_t fill = colWarn();
        if (battery_percent_ >= 70.0f) {
            fill = colOk();
        } else if (battery_percent_ >= 25.0f) {
            fill = tft_.color565(230, 200, 60);
        }

        tft_.fillRect(x, y, w, h, bg);

        const int label_w = 26;
        tft_.setTextColor(colTextDim(), bg);
        tft_.setTextDatum(TL_DATUM);
        tft_.drawString("PWR", x, y, 1);
        tft_.setTextDatum(TL_DATUM);

        const int segments = 20;
        const int gap = 1;
        const int bar_x = x + label_w;
        const int bar_w = w - label_w;
        const int inner_w = bar_w - 2;
        const int inner_h = h - 2;
        const int pad_y = 1;
        const int total_gap = (segments - 1) * gap;
        const int seg_w = (inner_w - total_gap) / segments;
        const int total_w = (seg_w * segments) + total_gap;
        const int frame_w = total_w + 2;
        const int frame_x = bar_x + ((bar_w - frame_w) / 2);

        tft_.drawRect(frame_x, y, frame_w, h, frame);
        const int cap_w = 4;
        const int cap_h = h / 2;
        tft_.fillRect(frame_x + frame_w, y + (h - cap_h) / 2, cap_w, cap_h, frame);
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

    void drawSolarIndicator(int x, int y, int w, int h) {
        if (!force_full_redraw_ &&
            solar_percent_ == last_solar_percent_ &&
            solar_charging_ == last_solar_charging_) {
            return;
        }

        const uint16_t bg = colBg();
        const uint16_t frame = colAccentDim();
        const uint16_t off = tft_.color565(18, 20, 24);
        const uint16_t fill = solar_charging_
            ? rgb565(255, 180, 40)
            : rgb565(80, 80, 80);

        tft_.fillRect(x, y, w, h, bg);

        const int label_w = 34;
        tft_.setTextColor(colTextDim(), bg);
        tft_.setTextDatum(TL_DATUM);
        tft_.drawString("SOL", x, y - 1, 1);
        tft_.setTextDatum(TL_DATUM);

        const int segments = 20;
        const int gap = 1;
        const int bar_x = x + label_w;
        const int bar_w = w - label_w;
        const int inner_w = bar_w - 2;
        const int inner_h = h - 2;
        const int pad_y = 1;
        const int total_gap = (segments - 1) * gap;
        const int seg_w = (inner_w - total_gap) / segments;
        const int total_w = (seg_w * segments) + total_gap;
        const int frame_w = total_w + 2;
        const int frame_x = bar_x + ((bar_w - frame_w) / 2);

        tft_.drawRect(frame_x, y, frame_w, h, frame);
        int filled = (int)floorf((solar_percent_ / 100.0f) * (float)segments);
        filled = constrain(filled, 0, segments);

        int sx = frame_x + 1;
        const int sy = y + 1 + pad_y;
        const int seg_h = inner_h - (2 * pad_y);
        for (int i = 0; i < segments; ++i) {
            const uint16_t color = (i < filled) ? fill : off;
            tft_.fillRect(sx, sy, seg_w, seg_h, color);
            sx += seg_w + gap;
        }

        if (solar_charging_) {
            const int bx = x + w - 18;
            const int by = y - 6;
            tft_.fillTriangle(bx, by, bx + 6, by + 10, bx + 12, by, fill);
        }

        last_solar_percent_ = solar_percent_;
        last_solar_charging_ = solar_charging_;
    }

    void drawActiveIndicator(int x, int y) {
        if (!force_full_redraw_ && active_ == last_active_) {
            return;
        }

        const int size = 18;
        const uint16_t bg = colPanel();
        const uint16_t off_color = colLine();
        const uint16_t on_color = colOk();

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
        const uint16_t bg = colPanel();
        const uint16_t off_color = colLine();
        const uint16_t on_color = colWarn();

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
        tft_.fillRect(x, y, box_w, box_h, colBg());
        tft_.drawFastHLine(x + 4, y + 2, box_w - 8, colAccentDim());
        tft_.setTextColor(colText(), colBg());
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

        const uint16_t dark_green = tft_.color565(0, 35, 20);
        const uint16_t dark_blue = tft_.color565(0, 12, 45);
        const uint16_t dark_red = tft_.color565(45, 0, 10);
        const uint16_t ring_dim = colAccentDim();
        const uint16_t ring_bright = colAccent();

        const float diff_abs_full = max(fabsf(diff_h_percent_), fabsf(diff_v_percent_));
        const float deadband_for_region = fabsf(deadband_percent_);
        const float pwm_for_region = fabsf(pwm_threshold_percent_);
        const float deadband_th = min(deadband_for_region, pwm_for_region);
        const float pwm_th = max(deadband_for_region, pwm_for_region);

        uint16_t region_bg = dark_red;
        if (diff_abs_full <= deadband_th) {
            region_bg = dark_green;
        } else if (diff_abs_full <= pwm_th) {
            region_bg = dark_blue;
        }

        const float gauge_min = -35.0f;
        const float gauge_max = 35.0f;
        const float gauge_span = gauge_max - gauge_min;

        const int deadband_r = (int)((fabsf(deadband_percent_) / gauge_span) * (2.0f * r));
        const int pwm_r = (int)((fabsf(pwm_threshold_percent_) / gauge_span) * (2.0f * r));

        const bool base_changed =
            force_full_redraw_ ||
            deadband_percent_ != last_deadband_percent_ ||
            pwm_threshold_percent_ != last_pwm_threshold_percent_ ||
            region_bg != last_region_bg_;

        const int marker_radius = 4;

        if (base_changed) {
            tft_.fillCircle(cx, cy, r, region_bg);
            tft_.drawCircle(cx, cy, r, ring_dim);
            tft_.drawCircle(cx, cy, r - 1, ring_bright);
            if (deadband_r > 0) {
                tft_.drawCircle(cx, cy, deadband_r, colOk());
            }
            if (pwm_r > 0) {
                tft_.drawCircle(cx, cy, pwm_r, colMid());
            }
        } else if (has_marker_) {
            tft_.fillCircle(last_marker_x_, last_marker_y_, marker_radius, region_bg);
        }

        // Crosshair + ticks (redrawn to avoid marker erasing lines)
        const uint16_t dark_grey = colLine();
        tft_.drawLine(cx - r, cy, cx + r, cy, dark_grey);
        tft_.drawLine(cx, cy - r, cx, cy + r, dark_grey);
        for (int angle = 0; angle < 360; angle += 30) {
            const float a = radians((float)angle);
            const bool major = (angle % 90) == 0;
            const bool mid = (angle % 45) == 0;
            const int tick_len = major ? 8 : (mid ? 6 : 4);
            const uint16_t tick_col = major ? ring_bright : (mid ? ring_dim : colLine());
            const int x1 = cx + (int)((float)(r - tick_len) * cosf(a));
            const int y1 = cy + (int)((float)(r - tick_len) * sinf(a));
            const int x2 = cx + (int)((float)r * cosf(a));
            const int y2 = cy + (int)((float)r * sinf(a));
            tft_.drawLine(x1, y1, x2, y2, tick_col);
        }

        // Redraw outer and threshold rings on every frame so marker erase does not punch holes in them
        tft_.drawCircle(cx, cy, r, ring_dim);
        tft_.drawCircle(cx, cy, r - 1, ring_bright);
        if (deadband_r > 0) {
            tft_.drawCircle(cx, cy, deadband_r, colOk());
        }
        if (pwm_r > 0) {
            tft_.drawCircle(cx, cy, pwm_r, colMid());
        }

        // Marker based on H/V diffs, clamped to circle
        float h_norm = diff_h_percent_ / gauge_max;
        float v_norm = diff_v_percent_ / gauge_max;
        h_norm = constrain(h_norm, -1.0f, 1.0f);
        v_norm = constrain(v_norm, -1.0f, 1.0f);
        const float len = sqrtf((h_norm * h_norm) + (v_norm * v_norm));
        const float scale = (len > 1.0f) ? (1.0f / len) : 1.0f;
        int marker_limit = r - marker_radius - 1;
        if (marker_limit < 0) {
            marker_limit = 0;
        }
        const int px = cx + (int)(h_norm * scale * (float)marker_limit);
        const int py = cy - (int)(v_norm * scale * (float)marker_limit);
        const bool saturated = len > 1.0f;
        const bool in_deadband = diff_abs_full <= deadband_th;
        const uint16_t dot_color = in_deadband
            ? colOk()
            : (saturated ? colWarn() : colText());
        tft_.fillCircle(px, py, marker_radius, dot_color);
        last_marker_x_ = px;
        last_marker_y_ = py;
        has_marker_ = true;

        // Tracking labels around gauge
        const float diff_abs_h = fabsf(diff_h_percent_);
        const float diff_abs_v = fabsf(diff_v_percent_);
        uint16_t color_h = colWarn();
        uint16_t color_v = colWarn();
        if (diff_abs_h <= deadband_th) {
            color_h = colOk();
        } else if (diff_abs_h <= pwm_th) {
            color_h = colMid();
        }
        if (diff_abs_v <= deadband_th) {
            color_v = colOk();
        } else if (diff_abs_v <= pwm_th) {
            color_v = colMid();
        }

        char top_label[48];
        snprintf(
            top_label,
            sizeof(top_label),
            "dH:%4.1f%% A:%4.0f B:%4.0f",
            diff_h_percent_,
            h_avg_a_,
            h_avg_b_);
        const int label_w = 220;
        const int label_h = 12;
        tft_.fillRect(cx - (label_w / 2), cy - r - 18, label_w, label_h, colBg());
        tft_.setTextColor(color_h, colBg());
        tft_.setTextDatum(MC_DATUM);
        tft_.drawString(top_label, cx, cy - r - 10, 1);

        char bottom_label[48];
        snprintf(
            bottom_label,
            sizeof(bottom_label),
            "dV:%4.1f%% A:%4.0f B:%4.0f",
            diff_v_percent_,
            v_avg_a_,
            v_avg_b_);
        tft_.fillRect(cx - (label_w / 2), cy + r + 4, label_w, label_h, colBg());
        tft_.setTextColor(color_v, colBg());
        tft_.drawString(bottom_label, cx, cy + r + 10, 1);
        tft_.setTextDatum(TL_DATUM);

        last_diff_h_percent_ = diff_h_percent_;
        last_diff_v_percent_ = diff_v_percent_;
        last_deadband_percent_ = deadband_percent_;
        last_pwm_threshold_percent_ = pwm_threshold_percent_;
        last_region_bg_ = region_bg;
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
    unsigned long last_tick_ms_ = 0;
    uint16_t last_region_bg_ = 0;
    int last_marker_x_ = 0;
    int last_marker_y_ = 0;
    bool has_marker_ = false;
    float solar_percent_ = 0.0f;
    float last_solar_percent_ = -1.0f;
    bool solar_charging_ = false;
    bool last_solar_charging_ = false;
    float h_avg_a_ = 0.0f;
    float h_avg_b_ = 0.0f;
    float v_avg_a_ = 0.0f;
    float v_avg_b_ = 0.0f;
    float pwm_h_norm_ = 0.0f;
    float pwm_v_norm_ = 0.0f;
    float last_pwm_h_norm_ = 99.0f;
    float last_pwm_v_norm_ = 99.0f;
};
