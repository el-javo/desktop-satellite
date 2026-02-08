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
        unsigned long refresh_interval_ms;
    };

    explicit DisplayManager(const Config& cfg)
        : cfg_(cfg) {}

    void begin() {
        if (cfg_.pin_blk >= 0) {
            pinMode(cfg_.pin_blk, OUTPUT);
            digitalWrite(cfg_.pin_blk, HIGH);
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

    void setTrackingInfo(float diff_percent) {
        diff_percent_ = diff_percent;
        dirty_ = true;
    }

    void setDeadzonePercent(float deadzone_percent) {
        deadzone_percent_ = deadzone_percent;
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
        drawHeader("OFF");
    }

    void drawConnecting() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        drawHeader("CONNECT");
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
        drawHeader("DASH");
        drawEnvBlock(10, 40);
        force_full_redraw_ = false;
    }

    void drawTracking() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        drawHeader("TRACK");
        drawDiffGauge(10, 70, 220, 40);
        drawEnvBlock(10, 125);
        force_full_redraw_ = false;
    }

    void drawEnvBlock(int x, int y) {
        if (!force_full_redraw_ &&
            temp_c_ == last_temp_c_ &&
            humidity_pct_ == last_humidity_pct_) {
            return;
        }

        const uint16_t dark_env = tft_.color565(0, 25, 0);
        const int box_w = 220;
        const int box_h = 104;
        tft_.fillRect(x, y, box_w, box_h, dark_env);
        tft_.drawRect(x, y, box_w, box_h, TFT_GREEN);
        tft_.setTextColor(TFT_YELLOW, dark_env);
        tft_.setTextDatum(TL_DATUM);

        // Make this block "dashboard-large" without affecting the rest of the UI.
        tft_.setTextSize(2);
        const int use_font = 2;
        const int pad_x = 10;
        const int pad_y = 10;
        const int line_h = tft_.fontHeight(use_font);

        char line1[24];
        char line2[24];
        snprintf(line1, sizeof(line1), "T: %.1fC", temp_c_);
        snprintf(line2, sizeof(line2), "H: %.1f%%", humidity_pct_);

        tft_.drawString(line1, x + pad_x, y + pad_y, use_font);
        tft_.drawString(line2, x + pad_x, y + pad_y + line_h + 12, use_font);

        tft_.setTextDatum(TL_DATUM);
        tft_.setTextSize(1);
        last_temp_c_ = temp_c_;
        last_humidity_pct_ = humidity_pct_;
    }

    void drawDiffGauge(int x, int y, int w, int h) {
        if (!force_full_redraw_ &&
            diff_percent_ == last_diff_percent_ &&
            deadzone_percent_ == last_deadzone_percent_ &&
            pwm_threshold_percent_ == last_pwm_threshold_percent_) {
            return;
        }

        const int inner_x = x + 1;
        const int inner_y = y + 1;
        const int inner_w = w - 2;
        const int inner_h = h - 2;
        const int center_x = x + w / 2;

        const uint16_t dark_green = tft_.color565(0, 40, 0);
        const uint16_t dark_blue = tft_.color565(0, 0, 60);
        const uint16_t dark_red = tft_.color565(60, 0, 0);

        tft_.drawRect(x, y, w, h, TFT_WHITE);

        const float diff_abs_full = fabsf(diff_percent_);
        const float dz_for_region = fabsf(deadzone_percent_);
        const float pwm_for_region = fabsf(pwm_threshold_percent_);
        const float dz_th = min(dz_for_region, pwm_for_region);
        const float pwm_th = max(dz_for_region, pwm_for_region);

        uint16_t region_bg = dark_red;
        uint16_t region_fg = TFT_RED;
        if (diff_abs_full <= dz_th) {
            region_bg = dark_green;
            region_fg = TFT_GREEN;
        } else if (diff_abs_full <= pwm_th) {
            region_bg = dark_blue;
            region_fg = TFT_BLUE;
        }

        tft_.fillRect(inner_x, inner_y, inner_w, inner_h, region_bg);

        // Diff value label (small), color-coded by current region
        {
            char diff_label[24];
            snprintf(diff_label, sizeof(diff_label), "DIFF: %.1f%%", diff_percent_);
            const int label_h = 14;
            const int label_y = y - label_h - 4;
            if (label_y > 28) { // keep below header
                tft_.fillRect(x, label_y, w, label_h, TFT_BLACK);
                tft_.setTextColor(region_fg, TFT_BLACK);
                tft_.setTextDatum(TL_DATUM);
                tft_.drawString(diff_label, x, label_y, 1);
            }
        }

        const float gauge_min = -35.0f;
        const float gauge_max = 35.0f;
        const float gauge_span = gauge_max - gauge_min;

        float dz = fabsf(deadzone_percent_);
        dz = constrain(dz, 0.0f, 100.0f);
        const int dz_px = (int)((dz / gauge_span) * inner_w);

        tft_.drawLine(center_x - dz_px, inner_y, center_x - dz_px, inner_y + inner_h, TFT_GREEN);
        tft_.drawLine(center_x + dz_px, inner_y, center_x + dz_px, inner_y + inner_h, TFT_GREEN);

        float pwm_th_draw = fabsf(pwm_threshold_percent_);
        pwm_th_draw = constrain(pwm_th_draw, 0.0f, 100.0f);
        const int pwm_px = (int)((pwm_th_draw / gauge_span) * inner_w);
        tft_.drawLine(center_x - pwm_px, inner_y, center_x - pwm_px, inner_y + inner_h, TFT_BLUE);
        tft_.drawLine(center_x + pwm_px, inner_y, center_x + pwm_px, inner_y + inner_h, TFT_BLUE);

        float diff = diff_percent_;
        diff = constrain(diff, gauge_min, gauge_max);
        const float diff_norm = (diff - gauge_min) / gauge_span;
        const int marker_x = inner_x + (int)(diff_norm * (inner_w - 1));
        tft_.drawLine(marker_x, inner_y + 1, marker_x, inner_y + inner_h - 1, TFT_WHITE);

        last_diff_percent_ = diff_percent_;
        last_deadzone_percent_ = deadzone_percent_;
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
    float diff_percent_ = 0.0f;
    float deadzone_percent_ = 1.0f;
    float pwm_threshold_percent_ = 10.0f;
    float last_temp_c_ = 9999.0f;
    float last_humidity_pct_ = 9999.0f;
    float last_diff_percent_ = 9999.0f;
    float last_deadzone_percent_ = 9999.0f;
    float last_pwm_threshold_percent_ = 9999.0f;
};
